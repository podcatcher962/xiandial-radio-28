/*
 * 拾声 (XianDial) 硬件版 —— 音频输出（I2S + ES8311 + FM8002E）
 *
 * 依据 10-03 参考对比：小智官方固件（lcdwiki-es3c35p 板级）用的就是
 *   ES8311 + I2C0 + 同一组 I2S 引脚，我们照同一套走，少走弯路。
 *
 * 链路：
 *   ESP32-S3 I2S(主) --MCLK/BCLK/WS/DOUT--> ES8311 --DAC--> FM8002E --> 喇叭
 *   ES8311 寄存器走 I2C0（和触摸共用一条总线，地址 0x18）
 *   PA_EN(IO1) 由本文件直接控制
 *
 * ★ 为什么 PA_EN 自己管：走 codec 驱动的 gpio_if 也能配，但那层是
 *   「传一个 pa_pin 进去、它内部 gpio_set_level」，出问题看不到电平。
 *   自己 gpio_config + set_level，日志里明确打一行，排查快得多。
 *
 * ★★★ 10-11 血泪：本链路曾让 2.8″ 整机无声过，坑不在本文件、而在【引脚冲突】。
 *   判据记住：**DOUT 那个脚绝不能同时被别的模块当模拟/输入用** ——
 *   2.8″ 板上 IO8 = I2S 的 DOUT，而 app_pwr.c 的电池检测曾无条件写死
 *   ADC_CHANNEL_7(=IO8) ⇒ 该脚的输出使能被清 0 ⇒ 引脚不推线
 *   ⇒ 只剩功放开关的「啪啪」电流声，而日志全绿、DAC 侧一切正常。
 *   ★ 教训 =「配置写了」≠「生效了」：写成功的日志证明不了信号出到了焊盘。
 *   真因已在 app_pwr.c 修掉，并在那里加了运行时自检（配错会打 ★★★ ADC 配错！）。
 *   ⇒ 本文件【刻意不】在 init 里再改一遍引脚路由：寄存器级「自愈」代码一旦写错
 *     反而会打断正常输出，得不偿失。要观测就调 gpio_dump_io_configuration()。
 */
#include "app_audio.h"

#include "app_display.h"
#include "app_pins.h"

#include <math.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"   /* ★ 10-04：codec 互斥锁要用信号量 API */

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "es8311_codec.h"

static const char *TAG = "XSAUD";

#define AUD_SR      44100
#define AUD_CH      2

/* ★★★ 10-11：麦克风（ADC）通路开关 —— **定义在 app_audio.h 里**（单一真源），
 *  本文件只使用。为什么放在头文件：`app_voice.c` 也要看这个开关，
 *  两处各写一份 `#ifndef` 迟早漂移（一个改成 1、另一个还是 0
 *  ⇒ 声明的和实现的对不上，编得过但链不到）。
 *
 *  为什么用编译开关而不是"直接开"：
 *    本文件是【3.5 与 2.8 共享】的（在 `_sync_core.py` 的 SYNC_FILES 里），
 *    而麦克风只有 2.8 需要（语音在 2.8 做，兰兰 10-11 定）。
 *    3.5 是另一块板、另一条产品线，**没理由替它多占 8 KB 内部 DMA**。
 *  ⇒ 默认关（3.5 行为与今天完全一致），es28 的 CMakeLists 传
 *    `-DAUD_MIC_ENABLE=1` 打开。同款手法见 app_settings.c 的
 *    `XS_CFG_DEF_VOLUME`（铁律 81：板相关常量必须能按板覆盖）。
 *
 *  ⚠️ 开与关【不是只差一个 RX 通道】—— 四处必须一起变，缺一处就是
 *    「编得过、跑起来读不到数据」或者「播放也坏了」。四处都在下面标了 #if。*/

/* ES8311 的 7 位 I2C 地址：驱动用 8 位形式 0x30，实际线上是 0x18（CE 接地）*/
#define ES8311_I2C_ADDR_7BIT   (ES8311_CODEC_DEFAULT_ADDR >> 1)

/* 写入统计（排查「有链路没声音」用：写入失败会在这里现形）*/
static uint32_t s_wr_bytes = 0;
static uint32_t s_wr_err   = 0;

static i2s_chan_handle_t       s_tx  = NULL;
#if AUD_MIC_ENABLE
/* ★ 全双工时的接收通道。只在 AUD_MIC_ENABLE=1 时存在 ——
 * 关掉时编译期就没有这个变量，避免"声明了没人用"的警告。*/
static i2s_chan_handle_t       s_rx  = NULL;
static bool                    s_mic_on = false;
#endif
static esp_codec_dev_handle_t  s_dev = NULL;
static bool                    s_ready = false;
static int                     s_vol = 60;
static int                     s_cur_sr = AUD_SR;   /* 当前 I2S/codec 采样率 */

/* ★★ 10-11：功放闸门状态（见 app_audio.h 里 app_audio_pa_gate 的说明）。
 *   s_pa_inited 防止在 GPIO 还没配好时被误调 —— app_audio_init() 之前
 *   播放层就可能因为别的初始化顺序先调到它。*/
static bool                    s_pa_inited = false;
static bool                    s_pa_on     = false;

/* 正弦表：512 点，按频率取步进即可合成任意音高（测试音用） */
#define SINE_N 512
static int16_t s_sine[SINE_N];

static void sine_init(void)
{
    const double pi = 3.14159265358979;
    for (int i = 0; i < SINE_N; i++) {
        double ph = 2.0 * pi * (double)i / (double)SINE_N;
        s_sine[i] = (int16_t)(sin(ph) * 18000.0);
    }
}

/* ============================================================
 *  ★★ 10-04：codec 操作互斥锁（本机第一例「跨任务共享句柄」）
 *
 *  起因：兰兰报「本地播放按倍速就宕机」。
 *  真因：app_audio_set_sample_rate() 要 close codec → 改 I2S 时钟 → open，
 *  而 close/open 期间 s_dev 处于【半死】状态（内部 codec_if 被关、
 *  I2S 通道未使能）。同一个时刻 player_task 正在 app_audio_write() 里
 *  往 s_dev 写 PCM ⇒ 写进一个正在被销毁/重建的对象 ⇒ 野指针 ⇒ 重启。
 *
 *  ★ 这正是速查卡铁律 18 的场景：跨任务共享的句柄，
 *   「句柄归谁」必须写进注释，另一侧只能通过请求变量通信。
 *   这里两个函数是同一文件、同一份句柄，但【调用方在不同任务】
 *   （player_task 写 / UI 任务改倍率），所以仍要互斥。
 * ============================================================ */
static SemaphoreHandle_t s_codec_mtx = NULL;

esp_err_t app_audio_write(const void *pcm, size_t bytes)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    /* ★ 拿不到锁就宁可丢这一帧也不要崩：DMA 欠载会自动补零 = 静音，
     *   而写进正在被 close/open 的 codec 是野指针。*/
    if (s_codec_mtx && xSemaphoreTake(s_codec_mtx, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    int ret = esp_codec_dev_write(s_dev, (void *)pcm, (int)bytes);
    if (s_codec_mtx) xSemaphoreGive(s_codec_mtx);
    if (ret == ESP_CODEC_DEV_OK) {
        s_wr_bytes += (uint32_t)bytes;
    } else {
        if (s_wr_err < 3) ESP_LOGW(TAG, "write FAILED ret=%d (bytes=%u)", ret, (unsigned)bytes);
        s_wr_err++;
    }
    return (ret == ESP_CODEC_DEV_OK) ? ESP_OK : ESP_FAIL;
}

bool app_audio_ready(void)      { return s_ready; }
int  app_audio_sample_rate(void){ return s_cur_sr; }

/* ---- 功放闸门（说明见 app_audio.h）----
 *  ★ 故意做成【幂等】而不是引用计数：调用方只有一个（播放层），
 *    计数只会在失败路径上泄漏，反而更难查。*/
void app_audio_pa_gate(bool on)
{
    if (!s_pa_inited) return;       /* GPIO 未配好：静默忽略，别去写一个没配的脚 */
    if (on == s_pa_on) return;      /* 同一状态不重复写（省一次无谓的 IO 翻转）*/
    gpio_set_level(PIN_PA_EN, on ? PA_EN_ACTIVE : PA_EN_IDLE);
    s_pa_on = on;
    /* 只在【状态真变了】时才打 —— 所以它不会刷屏（一次切台最多 2~3 行），
     * 但它是排查「啪声/无声」时唯一能看见功放时序的窗口：
     *   正常切台应该是  关 … 关 … 开
     *   若看到「开 开 开」⇒ 闸门没生效；若看到「关」之后再无「开」⇒ 新流没开声。 */
    ESP_LOGI(TAG, "PA_EN(IO%d) -> %d（%s）", PIN_PA_EN,
             (int)(on ? PA_EN_ACTIVE : PA_EN_IDLE), on ? "功放开" : "功放关");
}
bool app_audio_pa_is_on(void)   { return s_pa_on; }

esp_err_t app_audio_set_sample_rate(int hz)
{
    if (!s_dev || !s_tx)  return ESP_ERR_INVALID_STATE;
    if (hz < 8000 || hz > 192000) return ESP_ERR_INVALID_ARG;
    if (hz == s_cur_sr)   return ESP_OK;

    /* ★ 10-11：这两个必须在拿锁【之前】声明并给初值 —— 下面有 goto out，
     *   而 goto 会跳过中途的初始化（C 允许，但值不确定）。*/
    bool pa_restore = false;   /* 改钟前功放是不是开着 */
    bool opened_ok  = false;   /* codec 是否已成功 open（决定能不能灌静音帧）*/

    /* ★★★ 10-04：与 app_audio_write 用【同一把锁】—— close/open 期间
     *   必须没有别的任务在往 s_dev 写 PCM，否则写进一个正在销毁/重建的
     *   codec 对象 = 野指针 = 重启（兰兰报「本地按倍速就宕机」）。
     *   超时给 1.5 秒：写 512 帧只要几毫秒，拿不到说明有别的异常，
     *   宁可这次倍率不生效也不能崩。*/
    if (s_codec_mtx && xSemaphoreTake(s_codec_mtx, pdMS_TO_TICKS(1500)) != pdTRUE) {
        ESP_LOGE(TAG, "改采样率 %d Hz 时拿不到 codec 锁，本次倍率不生效", hz);
        return ESP_ERR_TIMEOUT;
    }

    /* ★★★ 10-11 静音过渡（上半）：关功放 + codec 软静音。
     *  兰兰真机反馈：「本地音乐在点倍速的时候也会啪一声」（切台那声同源）。
     *  真因：esp_codec_dev_close() 内部会 disable I2S 通道 ⇒ DOUT 立刻停止
     *  推数据 ⇒ ES8311 输入端从「有波形」变成「没信号」⇒ DAC 输出跳变
     *  ⇒ 而 PA_EN 这时候是开着的，等于把这次跳变直接放大给喇叭 = 一记爆音。
     *  ⇒ 先把喇叭断开（关功放），再让 codec 数字域淡出（软静音）。
     *  ⚠️ 下面全程持锁，【不能】调 app_audio_write() —— 那是同一把不可重入的锁，
     *     会自死锁。要写静音帧就直接调 esp_codec_dev_write()（见 out: 段）。*/
    pa_restore = s_pa_on;
    app_audio_pa_gate(false);
    esp_codec_dev_set_out_mute(s_dev, true);

    /* ★ 顺序不能反：i2s_channel_reconfig_std_clock 开头有守卫
     *   （要求通道处于【未使能】状态），所以必须先 close codec 让它把 I2S 关掉，
     *   再改时钟，最后 open 重新使能。*/
    esp_codec_dev_close(s_dev);

    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)hz);
    esp_err_t err = i2s_channel_reconfig_std_clock(s_tx, &clk);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "reconfig clock -> %d Hz failed: %s", hz, esp_err_to_name(err));
        /* 尽量退回原采样率，别把音频链路留在半死状态。
         * ★ 必须用【复合字面量】强转：I2S_STD_CLK_DEFAULT_CONFIG() 展开是
         *   `{ ... }` 初始化列表，只能用于初始化；直接 `clk = I2S_STD_...` 赋值
         *   会报 `error: expected expression before '{' token`（10-03 实锤）。*/
        clk = (i2s_std_clk_config_t)I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)s_cur_sr);
        i2s_channel_reconfig_std_clock(s_tx, &clk);
        esp_codec_dev_sample_info_t fs0 = {
            .bits_per_sample = 16, .channel = AUD_CH, .sample_rate = s_cur_sr,
        };
        esp_codec_dev_open(s_dev, &fs0);
        esp_codec_dev_set_out_vol(s_dev, s_vol);
        opened_ok = true;      /* ★ 10-11：回退也算 open 成功，静音过渡要走完 */
        goto out;              /* ★ 统一出口放锁，别漏一条 return */
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel         = AUD_CH,
        .sample_rate     = hz,
    };
    int ret = esp_codec_dev_open(s_dev, &fs);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "reopen codec @%d Hz failed ret=%d", hz, ret);
        err = ESP_FAIL;
        goto out;
    }
    opened_ok = true;
    /* open 会把音量重置成默认，必须重新设回用户调的值 */
    esp_codec_dev_set_out_vol(s_dev, s_vol);
    s_cur_sr = hz;
    ESP_LOGI(TAG, "sample rate -> %d Hz", hz);
    err = ESP_OK;
out:
    /* ★★★ 静音过渡（下半）—— 所有退出路径（含失败回退）都必须走到这里。
     *   失败路径下若漏了「解除静音 / 还原功放」，机器就变成【一直没声音】，
     *   那比原来那一声「啪」严重得多 ⇒ 收尾不能只写在成功分支里。
     *   ④ 灌静音帧铺满 DMA：dma_desc_num(4) × dma_frame_num(512) × 4 B
     *      = 8192 B。写满它，DAC 就稳定停在零电平上。
     *   ⑤ 解除软静音 —— 此刻数据本来就是 0，所以这一步【不产生跳变】。
     *   ⑥ 还原功放 —— 等 20 ms 让 I2S 时钟真转起来再开，
     *      避免开功放那一刻 DAC 还没稳。*/
    if (opened_ok && s_dev) {
        static const int16_t z[512 * 2] = {0};      /* 512 帧 × 2 声道 */
        for (int i = 0; i < 4; i++) {
            esp_codec_dev_write(s_dev, (void *)z, sizeof(z));
        }
        esp_codec_dev_set_out_mute(s_dev, false);
    }
    if (pa_restore) {
        vTaskDelay(pdMS_TO_TICKS(20));
        app_audio_pa_gate(true);
    }
    /* ★★★ 所有退出路径都在这里放锁。close/open 期间不放 = 其他任务
     *   写进半死的 codec = 宕机。漏任何一条 return 都会重现这个 bug。*/
    if (s_codec_mtx) xSemaphoreGive(s_codec_mtx);
    return err;
}
int  app_audio_get_volume(void) { return s_vol; }

#if AUD_MIC_ENABLE
/* ============================================================
 *  麦克风（ES8311 的 ADC 通路）—— 10-11 为 2.8 的语音功能加
 * ============================================================
 *  完整设计见 app_audio.h 那段。此处只留三条实现要点：
 *    ① 采样率【由调用方先切好】——本模块不碰 I2S 时钟，
 *       因为切钟要走 app_audio_set_sample_rate()（它带着完整的静音/功放过渡）。
 *    ② 关麦 = **输入静音**，绝不 `i2s_channel_disable(s_rx)` ——
 *       `audio_codec_data_i2s.c:577-582` 明写 S3 上 *"if disable RX, TX also not work"*，
 *       单关 RX 会把播放一起弄哑。
 *    ③ 所有对 `s_dev` 的调用都持 `s_codec_mtx`，与 `app_audio_write()`
 *       / `app_audio_set_sample_rate()` 互斥 —— 否则会在 codec 关闭/重建的
 *       中途读它（野指针，10-04 那次「按倍速就宕机」同源）。*/

/* ★ MIC 增益（dB）。范围 0~42，驱动内部按 6 dB 步进换算成寄存器值。
 *  为什么是 30 而不是拉满 42：板载 MEMS 麦（LMA2718B381）灵敏度不低，
 *  拉满时近距离说话会削顶（波形变方 ⇒ 后端识别率反而下降）。
 *  30 dB 是乐鑫小智那套整机的常用档。
 *  ⚠️ 这个值是【待定档】的 —— 要用真机电平测试（app_voice 的 mic 测试会打
 *     每声道 RMS/峰值）看「安静」与「说话」差多少再定。*/
#define MIC_GAIN_DB   30

/* 当前增益（可被 app_audio_set_mic_gain() 运行时改；见那个函数的说明）*/
static int s_mic_gain_db = MIC_GAIN_DB;

esp_err_t app_audio_mic_open(void)
{
    if (!s_dev || !s_rx)  return ESP_ERR_INVALID_STATE;
    if (s_mic_on)         return ESP_OK;          /* 幂等 */

    if (s_codec_mtx && xSemaphoreTake(s_codec_mtx, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(TAG, "开麦时拿不到 codec 锁");
        return ESP_ERR_TIMEOUT;
    }
    int g = esp_codec_dev_set_in_gain(s_dev, (float)s_mic_gain_db);
    int m = esp_codec_dev_set_in_mute(s_dev, false);
    if (s_codec_mtx) xSemaphoreGive(s_codec_mtx);

    if (g != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "开麦失败：set_in_gain=%d", g);
        return ESP_FAIL;
    }
    /* ★★★ 10-11 真机踩到：ES8311 **没有实现 `mute_mic`**（`es8311.c` 里从没
     *   赋值过 `codec->base.mute_mic`；有它的是 es7210 / es7243 / es7243e）。
     *   而 `esp_codec_dev_set_in_mute()` 在没有 mute_mic 时返回
     *   **`ESP_CODEC_DEV_NOT_SUPPORT`（262）**（esp_codec_dev.c:520-525）。
     *   ⇒ 一开始我把这当成"开麦失败"，整个自检直接中止 —— 而真相是
     *     **ES8311 根本不需要"解除静音"这一步**：
     *     `es8311_start()` 里已经把 `ADC_REG17 = 0xBF`（ADC 音量拉满）；
     *     只有 `es8311_suspend()` 才写 0x00。
     *   ⇒ 判定改成：只有 gain 失败才算失败；mute 返回 NOT_SUPPORT 属正常。*/
    if (m != ESP_CODEC_DEV_OK) {
        if (m == ESP_CODEC_DEV_NOT_SUPPORT) {
            ESP_LOGI(TAG, "ES8311 不支持输入静音（正常：它用 ADC 音量代替，"
                          "es8311_start 已把 REG17 置 0xBF）");
        } else {
            ESP_LOGW(TAG, "解除输入静音返回 %d（不致命，继续）", m);
        }
    }
    s_mic_on = true;
    ESP_LOGI(TAG, "mic on：增益 %d dB ｜ 采样率 %d Hz（注意：采样率由 I2S 决定，"
                  "TX/RX 共用）", s_mic_gain_db, s_cur_sr);
    return ESP_OK;
}

esp_err_t app_audio_mic_close(void)
{
    if (!s_dev || !s_rx)  return ESP_ERR_INVALID_STATE;
    if (!s_mic_on)        return ESP_OK;          /* 幂等 */

    if (s_codec_mtx && xSemaphoreTake(s_codec_mtx, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(TAG, "关麦时拿不到 codec 锁");
        return ESP_ERR_TIMEOUT;
    }
    /* ★ 只尝试静音输入，【绝不】 `i2s_channel_disable(s_rx)` ——
     *   `audio_codec_data_i2s.c:577-582` 明写 S3 上
     *   *"if disable RX, TX also not work"*，单关 RX 会把播放一起弄哑。
     *   ⚠️ ES8311 不支持静音 ⇒ 这里实际上是个空操作，ADC 保持在工作态。
     *     这是【可以接受】的：关麦之后没人再调用 mic_read()，
     *     I2S RX 的 DMA 只是自己循环覆盖（约 21 次中断/秒，可忽略），
     *     不产生任何读取/解码开销。*/
    esp_codec_dev_set_in_mute(s_dev, true);
    if (s_codec_mtx) xSemaphoreGive(s_codec_mtx);

    s_mic_on = false;
    ESP_LOGI(TAG, "mic off（已停读；ES8311 无输入静音，ADC 保持工作态）");
    return ESP_OK;
}

int app_audio_mic_read(void *buf, int bytes)
{
    if (!s_dev || !s_mic_on || buf == NULL || bytes <= 0) return -1;

    if (s_codec_mtx && xSemaphoreTake(s_codec_mtx, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;

    /* ★★ `esp_codec_dev_read()` 成功时返回 **ESP_CODEC_DEV_OK(0)，不是字节数**
     *   （esp_codec_dev.c:254-275 实读源码确认；底下 `_i2s_data_read` 是
     *    「读满 size 才算成功」的语义）。
     *   ⇒ 成功就是"读满了"，返回 bytes；失败返回 -1。
     *   ⚠️ 若哪天误按"返回值=字节数"来用，就会把成功（0）当成"读了 0 个字节"
     *     而丢掉全部数据 —— 而且不报错，只是"永远采不到声音"。*/
    int ret = esp_codec_dev_read(s_dev, buf, bytes);
    if (s_codec_mtx) xSemaphoreGive(s_codec_mtx);

    if (ret != ESP_CODEC_DEV_OK) {
        /* 只报前几次，别刷屏 */
        static int n_log = 0;
        if (n_log++ < 3) ESP_LOGW(TAG, "mic read 失败 ret=%d", ret);
        return -1;
    }
    return bytes;
}

bool app_audio_mic_is_on(void) { return s_mic_on; }

int  app_audio_get_mic_gain(void) { return s_mic_gain_db; }

/* 运行时改麦增益（0~42 dB）。给串口 `micg <db>` 用 ——
 * ★ 为什么要能在运行时改：定档要靠**真机扫几档听/看 RMS**，
 *   若每改一档都要重编+重烧（约 1 分钟），一轮下来就是十几分钟。
 *   有了它一次烧录就能扫完全部档位。*/
esp_err_t app_audio_set_mic_gain(int db)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    if (db < 0)  db = 0;
    if (db > 42) db = 42;          /* ES8311 的 ADC 增益上限 42 dB */

    s_mic_gain_db = db;
    if (!s_mic_on) return ESP_OK;  /* 没开麦就先记下，开麦时用 */

    if (s_codec_mtx && xSemaphoreTake(s_codec_mtx, pdMS_TO_TICKS(500)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    int r = esp_codec_dev_set_in_gain(s_dev, (float)db);
    if (s_codec_mtx) xSemaphoreGive(s_codec_mtx);

    if (r != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "set_in_gain(%d dB) 失败 ret=%d", db, r);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "mic 增益 -> %d dB", db);
    return ESP_OK;
}
#endif /* AUD_MIC_ENABLE */


/* ============================================================
 *  倍速播放
 * ============================================================
 * ★ 原理见 app_audio.h 的说明：把 I2S/codec 的时钟拉高到
 *   「源采样率 × 倍率」，解码器输出的采样点数不变、频率不变，
 *   于是整段音频在更短的时间内播完 ⇒ 音调不变、速度变快。
 *
 * ⚠️ 这里存的是【源采样率】（s_src_rate），不是当前时钟。
 *   因为每次换台/换流都要把时钟对齐到那个源的新采样率，
 *   届时要重新乘一次倍率（见 app_audio_apply_rate）。
 */
static int s_speed_pct = 100;      /* 100 / 120 / 140 */
static int s_src_rate  = AUD_SR;   /* 解码器输出的采样率（倍率的基准）*/

/* 把「源采样率 × 倍率」真正配到硬件上 */
static esp_err_t apply_rate(int src_rate)
{
    int hz = src_rate * s_speed_pct / 100;
    return app_audio_set_sample_rate(hz);
}

void app_audio_set_speed(int pct)
{
    /* 只认三档 —— 界面上也只有这三档，别的值一律夹到最近的档。
     * 理由：非整数倍率会让 4/6 的时钟分频算不出整除，
     * IDF 会退回近似值，听起来是「忽快忽慢」。不如不给。*/
    if (pct < 110)      pct = 100;
    else if (pct < 130) pct = 120;
    else                pct = 140;
    if (pct == s_speed_pct) return;

    s_speed_pct = pct;
    int hz = s_src_rate * s_speed_pct / 100;
    esp_err_t r = apply_rate(s_src_rate);
    if (r != ESP_OK) {
        /* 失败就把倍率退回去 —— 宁可还是 1.0X，也不要停在「界面说 1.4X
         * 实际声音是乱的」这种状态。*/
        s_speed_pct = (pct == 140) ? 120 : 100;
        apply_rate(s_src_rate);
        ESP_LOGE(TAG, "speed %d%% 失败，退回 %d%%", pct, s_speed_pct);
    } else {
        ESP_LOGI(TAG, "speed -> %d%%  (源 %d Hz → I2S %d Hz)",
                 s_speed_pct, s_src_rate, hz);
    }
}

int app_audio_get_speed(void) { return s_speed_pct; }

/* 播放器拿到流的真实采样率时调它：既对齐时钟，又记住倍率基准 */
esp_err_t app_audio_set_src_rate(int hz)
{
    if (hz < 8000 || hz > 192000) return ESP_ERR_INVALID_ARG;
    s_src_rate = hz;
    return apply_rate(hz);
}

void app_audio_set_volume(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    s_vol = percent;
    if (s_dev) esp_codec_dev_set_out_vol(s_dev, s_vol);
}

esp_err_t app_audio_init(void)
{
    /* ---- 1. 功放使能脚：先配好并置于【关】电平，等 codec 就绪再开功放 ----
     *   ★ 本板功放是【低电平使能】（证据见 app_pins.h 的 PA_EN_ACTIVE 注释）。
     *   一开始就开功放的话，ES8311 还没配好时 DAC 输出是浮动的，
     *   经 FM8002E 放大就是一记「啪」的爆音（开机音还没放就先吓一跳）。*/
    gpio_config_t pa = {
        .pin_bit_mask = 1ULL << PIN_PA_EN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pa), TAG, "pa gpio");
    gpio_set_level(PIN_PA_EN, PA_EN_IDLE);   /* 拉高 = 功放关（静音）*/
    /* ★ 10-11：从这一行起 s_pa_* 才与实际电平一致，
     *   app_audio_pa_gate() 也才允许被调用（之前 s_pa_inited=false 会静默忽略）。*/
    s_pa_inited = true;
    s_pa_on     = false;
    /* ⚠️ 10-11 教训：**不要**在这里用 gpio_get_level() 回读 PA_EN 做校验。
     *   gpio_config(GPIO_MODE_OUTPUT) 会把该脚的输入通路关掉（FUN_IE=0），
     *   而 gpio_get_level() 读的是 GPIO_IN_REG（焊盘输入值）⇒ 输出脚【恒读 0】。
     *   实测：写 1 回读 0 ⇒ 每开一次机就误报一条「PA_EN 没生效」——
     *   探针自己坏了，伪装成被测对象的重大结论（铁律 82/111 同源）。
     *   真要回读，得把 mode 改成 GPIO_MODE_INPUT_OUTPUT 把输入通路打开。*/

    /* ---- 2. I2S 标准模式（主），44100/16bit/立体声 ---- */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    /* ★★★ 2026-10-03 第十七次：显式加大 DMA 缓冲（兰兰报「海外台一开始都听不到，
     *   法国国际等一会儿才有，还是卡，卡得比以前更严重」）。
     *
     *   真因不是「网速慢」，是【读网是阻塞的】：
     *     player_task 里 `esp_http_client_read(http, buf, IN_CHUNK - in_len)`
     *     一次要凑满 64 KB 才返回。法国国际实测 124.9 kbps
     *     ⇒ 64 KB ÷ 15.6 KB/s ≈ 【4.2 秒】。
     *     这 4.2 秒里 player_task 卡在 read 上，【一帧 PCM 都没往 I2S 送】
     *     ⇒ 喇叭断音 4.2 秒 ⇒ 听到的就是「等一会儿才有」＋「一顿一顿」。
     *   IN_CHUNK 从 16 KB 涨到 64 KB 反而让间隔变成 8.4 秒 —— 这就是
     *   「卡得比以前更严重」的原因：**大缓冲在这套单任务结构里是负优化。**
     *
     *   两处一起改，才是真解：
     *     ① 这里：DMA 缓冲 6 个描述符 × 1024 帧 = 24 ms 实时余量，
     *        读网抖动时不会立刻断音。
     *     ② app_radio.c：IN_CHUNK 回到 16 KB，并且【只要求读满剩余空间的 1/4】
     *        就返回去解码（READ_MIN 门槛），把阻塞上限压到 ~1 秒。
     */
    chan_cfg.dma_desc_num  = 4;          /* 默认 6 → 4 */
    chan_cfg.dma_frame_num = 512;        /* 默认 256 帧（1 KB/描述符）→ 4 KB，合计 16 KB 内部 DMA */
    chan_cfg.auto_clear    = true;       /* 没数据时输出 0，防止残留噪声 */
#if AUD_MIC_ENABLE
    /* ★ 开全双工：一个通道句柄同时管 TX 与 RX。
     *   ⚠️ 不能"再给 I2S_NUM_0 建第二个通道"—— 同一个端口只能有一个通道句柄，
     *     第二次 `i2s_new_channel` 会直接返回 ESP_ERR_INVALID_STATE。*/
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, &s_rx), TAG, "i2s new chan(fdx)");
#else
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, NULL), TAG, "i2s new chan");
#endif

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUD_SR),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din  = PIN_I2S_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), TAG, "i2s std init");
#if AUD_MIC_ENABLE
    /* ★ RX 必须用【同一份 std_cfg】：RX 与 TX 共用 BCLK/WS，
     *   两边的 slot/位宽/时钟设成不一样时，IDF 会在下面
     *   `esp_codec_dev_open()` 的 set_fmt 阶段报错（而且是静默失败）。*/
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std_cfg), TAG, "i2s std init rx");
#endif
    /* ★ 绝对不要在这里 i2s_channel_enable()！
     *   esp_codec_dev_open() 内部顺序是：
     *     data_if->set_fmt()  -> i2s_channel_reconfig_std_slot()/reconfig_std_clock()
     *     data_if->enable(true)
     *   而 IDF 的 reconfig_std_slot/clock 开头就有守卫：
     *     ESP_GOTO_ON_FALSE(handle->state == I2S_CHAN_STATE_READY, ESP_ERR_INVALID_STATE,
     *                       ..., "I2S should be disabled before reconfiguring the slot");
     *   ⇒ 提前 enable 会让 set_fmt 直接失败（还是静默失败：esp_codec_dev_open 不检查
     *     set_fmt 的返回值），时钟/位宽按旧值跑，很容易就变成「界面正常但没声音」。
     *   正确做法：只「建通道 + 配 std 模式」，enable 交给 esp_codec_dev_open()。
     *   （10-03 实锤：i2s_std.c:395 / i2s_std.c:reconfig_std_slot 都有这条 READY 守卫）*/

    /* ---- 3. ES8311 ----
     *   先做一次 I2C 在线探测：这一步能干净地把「芯片不在总线上 / 地址错 /
     *   触摸那条 I2C0 冲突」和「codec 寄存器配错」区分开。
     *   探测失败不一定拦下来（codec 可能仍能跑），但日志里必须留痕。*/
    esp_err_t pr = i2c_master_probe(app_i2c_bus_get(), ES8311_I2C_ADDR_7BIT, 200);
    ESP_LOGI(TAG, "ES8311 probe @0x%02X -> %s", ES8311_I2C_ADDR_7BIT,
             pr == ESP_OK            ? "ACK (在线)" :
             pr == ESP_ERR_NOT_FOUND ? "NACK (芯片不在总线)" : "TIMEOUT (总线异常)");

    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr       = ES8311_CODEC_DEFAULT_ADDR,   /* 0x30(8bit) -> 内部 >>1 = 0x18 */
        .bus_handle = app_i2c_bus_get(),           /* 复用触摸那条 I2C0 */
    };
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl != NULL, ESP_FAIL, TAG, "i2c ctrl if");

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    ESP_RETURN_ON_FALSE(gpio_if != NULL, ESP_FAIL, TAG, "gpio if");

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if        = ctrl,
        .gpio_if        = gpio_if,
#if AUD_MIC_ENABLE
        /* ★ 开麦就必须 BOTH（见文件头 AUD_MIC_ENABLE 的 ③）。
         *   只写 DAC 时 ES8311 的 ADC 相关寄存器（0x11/0x12/0x17 …）
         *   一个都不会被配，`esp_codec_dev_read()` 能返回成功但数据全是 0
         *   —— 那是"配置写了≠生效了"的又一例（铁律 8x 家族）。*/
        .codec_mode     = ESP_CODEC_DEV_WORK_MODE_BOTH,
#else
        .codec_mode     = ESP_CODEC_DEV_WORK_MODE_DAC,
#endif
        .pa_pin         = -1,          /* ★ 不用它的 PA 控制，见文件头注释 */
        .pa_reverted    = false,
        .master_mode    = false,       /* ESP32 是 I2S 主机 */
        .use_mclk       = true,
        .digital_mic    = false,       /* ★ 板载是【模拟】MEMS 麦（LMA2718B381），走 ES8311 的 ADC */
        .invert_mclk    = false,
        .invert_sclk    = false,
        .hw_gain        = { .pa_voltage = 5.0f, .codec_dac_voltage = 3.3f, .pa_gain = 0.0f },
        .mclk_div       = 256,
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es_cfg);
    ESP_RETURN_ON_FALSE(codec_if != NULL, ESP_FAIL, TAG, "es8311 new");

    audio_codec_i2s_cfg_t i2s_data_cfg = {
        .port      = I2S_NUM_0,
        .tx_handle = s_tx,
#if AUD_MIC_ENABLE
        .rx_handle = s_rx,
#endif
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_data_cfg);
    ESP_RETURN_ON_FALSE(data_if != NULL, ESP_FAIL, TAG, "i2s data if");

    esp_codec_dev_cfg_t dev_cfg = {
#if AUD_MIC_ENABLE
        /* ★ dev_type 也必须跟着变成 IN_OUT：它决定 esp_codec_dev_open()
         *   会不会去 open ADC 那一半。只写 OUT 的话，即使 codec_mode=BOTH、
         *   rx_handle 也给了，读出来的仍是一条没被打开的 ADC 通路。*/
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
#else
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
#endif
        .codec_if = codec_if,
        .data_if  = data_if,
    };
    s_dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(s_dev != NULL, ESP_FAIL, TAG, "codec dev new");

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel         = AUD_CH,
        .sample_rate     = AUD_SR,
    };
    int ret = esp_codec_dev_open(s_dev, &fs);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "codec open FAILED ret=%d", ret);
        s_dev = NULL;
        return ESP_FAIL;
    }

    esp_codec_dev_set_out_vol(s_dev, s_vol);
    sine_init();

    /* ★★ 10-04：codec 互斥锁必须在对外开写【之前】建好。
     *   player_task 从这一刻起就可能调 app_audio_write，
     *   而 UI 任务随时可能调 set_sample_rate（换台对齐采样率 / 改倍速）
     *   ⇒ 两者共用 s_dev，必须串行化。*/
    if (!s_codec_mtx) {
        s_codec_mtx = xSemaphoreCreateMutex();
        if (!s_codec_mtx) {
            /* 建不出来就干脆不让别人用 codec：宁可「没声音」也不要「写野指针」*/
            ESP_LOGE(TAG, "codec 互斥锁创建失败 —— 音频写入将被拒绝");
        } else {
            ESP_LOGI(TAG, "codec 互斥锁已建（write / set_sample_rate 串行）");
        }
    }

    /* ---- 4. codec 就绪后才开功放 ----
     *   ★ 本板功放【低电平使能】，所以这里是写 PA_EN_ACTIVE(=0)，
     *     不是写 1！10-03 写反过一次，症状是「日志全绿但一点声音都没有」。*/
    gpio_set_level(PIN_PA_EN, PA_EN_ACTIVE);
    s_pa_on = true;                          /* ★ 10-11：与上面这行电平严格同步 */
    ESP_LOGI(TAG, "PA_EN(IO%d) = %d (功放使能 / 低电平有效)", PIN_PA_EN, PA_EN_ACTIVE);

    s_ready = true;

    ESP_LOGI(TAG, "audio ready: I2S0 %d Hz / 16bit / stereo, ES8311 @I2C0 0x18, vol=%d",
             AUD_SR, s_vol);
    return ESP_OK;
}

/* ============================================================
 *  开机提示音：三声上行「叮-咚-叮」，非阻塞
 * ============================================================ */
/* ★★ 10-04 第二十九次：并发保护。
 *   原来 app_audio_beep_async() 每次调用都 xTaskCreate 一个新任务，
 *   没有任何互斥。两个任务同时往同一条 I2S 写 PCM ⇒ 数据交错
 *   ⇒ 听起来是「提示音卡顿/断断续续」。
 *   触发路径不止开机那一次（状态页有「提示音」键，播放页空台时也响），
 *   连按几下就会撞上。⇒ 用一个句柄做「已在响就拒绝新请求」。*/
static TaskHandle_t s_beep_task = NULL;

static void beep_task(void *arg)
{
    (void)arg;
    if (!s_ready) { s_beep_task = NULL; vTaskDelete(NULL); return; }

    static int16_t buf[SINE_N * AUD_CH];
    const int freqs[3] = { 784, 988, 1319 };   /* G5 / B5 / E6 */
    const int n_frames  = AUD_SR * 170 / 1000;
    const int gap_frames = AUD_SR * 80 / 1000;

    uint32_t b0 = s_wr_bytes, e0 = s_wr_err;

    ESP_LOGI(TAG, "beep: 3 tones (784/988/1319 Hz)");
    for (int k = 0; k < 3; k++) {
        for (int done = 0; done < n_frames; ) {
            int n = n_frames - done;
            if (n > SINE_N) n = SINE_N;
            /* 包络：起止各 15% 做淡入淡出，避免"啪"的爆音 */
            for (int i = 0; i < n; i++) {
                int idx = (int)(((uint32_t)(done + i) * (uint32_t)freqs[k] * SINE_N
                                 / (uint32_t)AUD_SR) & (SINE_N - 1));
                int32_t v = s_sine[idx];
                int pos = done + i;
                int ramp = n_frames / 6;
                if (ramp > 0) {
                    if (pos < ramp)             v = v * pos / ramp;
                    else if (pos > n_frames - ramp) v = v * (n_frames - pos) / ramp;
                }
                int16_t s16 = (int16_t)v;
                buf[i * 2]     = s16;
                buf[i * 2 + 1] = s16;
            }
            app_audio_write(buf, (size_t)n * AUD_CH * sizeof(int16_t));
            done += n;
        }
        /* 间隔静音（每两音之间） */
        if (k < 2) {
            memset(buf, 0, sizeof(int16_t) * SINE_N * AUD_CH);
            for (int done = 0; done < gap_frames; ) {
                int n = gap_frames - done;
                if (n > SINE_N) n = SINE_N;
                app_audio_write(buf, (size_t)n * AUD_CH * sizeof(int16_t));
                done += n;
            }
        }
    }
    /* 收尾静音，把 DMA 里最后一点余量推出去 */
    memset(buf, 0, sizeof(int16_t) * SINE_N * AUD_CH);
    app_audio_write(buf, (size_t)SINE_N * AUD_CH * sizeof(int16_t));

    ESP_LOGI(TAG, "beep done: wrote %u B, err=%u (expect %u B)",
             (unsigned)(s_wr_bytes - b0), (unsigned)(s_wr_err - e0),
             (unsigned)((3 * n_frames + 2 * gap_frames + SINE_N) * AUD_CH * sizeof(int16_t)));
    s_beep_task = NULL;              /* 先清句柄，再删任务：反过来的话
                                     * 中间那一瞬新请求会以为「没人占用」*/
    vTaskDelete(NULL);
}

void app_audio_beep_async(void)
{
    if (!s_ready) return;
    if (s_beep_task != NULL) {
        ESP_LOGW(TAG, "beep 已在响，忽略这一次请求（别让两个任务抢 I2S）");
        return;
    }
    if (xTaskCreate(beep_task, "xs_beep", 4096, NULL, 5, &s_beep_task) != pdPASS) {
        s_beep_task = NULL;
        ESP_LOGE(TAG, "beep 任务创建失败");
    }
}
