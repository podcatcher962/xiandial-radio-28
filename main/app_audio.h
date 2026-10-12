#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* ============================================================
 *  拾声 (XianDial) 硬件版 —— 音频（ES8311 + FM8002E 功放）
 * ============================================================
 *  板型 LCD Wiki ES3C35P：
 *    ESP32-S3 --I2S--> ES8311 --模拟--> FM8002E(PA) --> 喇叭
 *    I2S : MCLK=IO17 BCLK=IO18 WS=IO21 DOUT=IO15 DIN=IO16
 *    控制: I2C0（与触摸共用）= SDA IO38 / SCL IO39，地址 0x18(7bit)
 *    功放: PA_EN = IO1（★ 低电平使能 —— 见 app_pins.h 的 PA_EN_ACTIVE）
 *  ⚠️ 上面这组脚号是【3.5″ 板】的。本模块是两板共用文件，
 *     代码里一律用 app_pins.h 里的 PIN_ 宏（PIN_I2S_MCLK / PIN_I2S_DOUT / PIN_PA_EN）；
 *     2.8″ 板脚号完全不同 —— 尤其 2.8″ 的 IO8 就是 DOUT，
 *     见 app_audio.c 头部的「引脚冲突」血泪注释。
 *
 *  10-03 首次接通。设计取舍：
 *    · PA_EN 由本模块直接 GPIO 控制（不走 codec 驱动的 gpio_if），
 *      少一层间接、日志能直接看到电平，排查最省事。
 *    · 采样率固定 44100 / 16bit / 立体声 —— 网络电台与本地 MP3 的主流格式。
 */

/* 初始化 I2S + ES8311 + 功放。失败返回错误码，界面照跑（只是没声音）。*/
esp_err_t app_audio_init(void);

/* 音频链路是否就绪 */
bool app_audio_ready(void);

/* 音量 0~100（对数曲线由 codec 内部处理）*/
void app_audio_set_volume(int percent);
int  app_audio_get_volume(void);

/* 往 I2S 写一段 PCM（阻塞到写完）。格式：16bit 交错立体声。*/
esp_err_t app_audio_write(const void *pcm, size_t bytes);

/* 播放开机提示音（非阻塞：内部起一次性任务，播完自动退出）。
 * 用来在【不接任何音源】的情况下证明「喇叭真的会响」。*/
void app_audio_beep_async(void);

/* 当前采样率（Hz） */
int app_audio_sample_rate(void);

/* 切换采样率（Hz）：重配 I2S 时钟 + 重开 codec。
 * ★ 为什么需要：MP3 流不一定是 44100 —— 播客（如卡里的「小宇宙播客 64K」）
 *   常见 24000 / 32000。若不管它，按 44100 播放会变调、音长也对不上。
 *   播放器解出第一帧、拿到真实采样率后调本函数对齐，之后再开始写 PCM。
 * 返回 ESP_OK 表示已切到目标采样率。*/
esp_err_t app_audio_set_sample_rate(int hz);

/* ---- 倍速播放（10-04 兰兰：「增加倍速播放按键1.2X 或1.4X」）----
 *
 *  ★★ 为什么能做到「音调不变」——
 *   播 1.2 倍速有两种做法：
 *     ① 丢掉 1/5 的采样点直接送 I2S（最简单）
 *        ⇒ 声音【变调变尖】，像卡带快放。电台/播客听着非常难受。
 *     ② 让 I2S/codec 以【源采样率 × 倍率】运行，解码器照旧输出源采样率的 PCM
 *        ⇒ 同样的采样点数在【更短的时间里】被硬件取走 ⇒ 时间轴压缩 1.2 倍，
 *          而每个采样点代表的频率没变 ⇒ **音调完全不变**，只是读得快了。
 *   ②才是「倍速」该有的样子，实现也简单：只是把 I2S 时钟拉高 20%。
 *
 *  ★ 代价：codec 要重开一次（约几十毫秒，期间会有一声极短的爆音/静默）。
 *   所以切换是在【切下一段 PCM 之前】做的，且只切一次，不是每帧都切。
 *
 *  ★ 为什么不用软件重采样：44100→52920 的插值重采样要动 PCM 数据本身，
 *   在解码任务里加一层重采样器 = 多几百 KB 缓冲 + 每样本一次乘加，
 *   而收益（避免几十毫秒爆音）远不抵。ESP32-S3 上 I2S 时钟随便改。
 *
 *  用法：
 *      app_radio_set_speed(120);   // 1.0x / 1.2x / 1.4x，倍率乘 100
 *      app_radio_speed();           // 读当前倍率（100/120/140）*/
void      app_audio_set_speed(int pct);   /* 100 = 正常；内部换算成时钟倍率 */
int       app_audio_get_speed(void);

/* 播放器拿到流的真实采样率时调它（app_radio.c 内部用）。
 * 它做两件事：把 I2S 对齐到「源采样率 × 当前倍率」，并记住倍率基准，
 * 这样切倍速时不用再问播放器要采样率。*/
esp_err_t app_audio_set_src_rate(int hz);

/* ---- 功放闸门（10-11 新增：消掉切台/改倍速时那一声「啪」）----
 *
 *  症状：切台（上一个 / 下一个 / 回首页换栏目台）、本地音乐改倍速时，
 *        喇叭里会「啪」一下。
 *  真因：PA_EN 从 init 之后就【全程常开】。而每次「换流」或「改采样率」都会让
 *        I2S 输出瞬间断掉或重启 —— DOUT 从「正在推波形」直接塌到 0，
 *        （或从一个波形跳到另一个波形）。这个阶跃被 FM8002E 放大就是一记爆音。
 *        ⇒ 不是音频数据脏，是【开关时序】问题。
 *
 *  做法：在会断流的时间窗里把功放关掉（喇叭断开，内部怎么跳都听不见），
 *        等新流稳定了再开：
 *            app_audio_pa_gate(false);   // 切流前
 *            ... 停旧流 / 起新流 ...
 *            app_audio_pa_gate(true);    // 新流首帧写出后
 *
 *  ★ 与 app_audio_set_sample_rate() 的分工：那个函数内部【自己】会做一次完整的
 *    「关功放 → 软静音 → 改钟 → 灌静音帧 → 解除静音 → 恢复功放」过渡，
 *    所以「改倍速」这条路径调用方什么都不用做。
 *
 *  ★ 这是个【幂等】的开关，没有引用计数：谁最后写谁说了算。
 *    播放层只需要在「我这一段要求静音」的窗口里配对调用即可。*/
void app_audio_pa_gate(bool on);

/* 功放闸门当前是不是开着。播放层用它记住「切流前是不是我关的」，
 * 以便在失败路径上把状态还原回去。*/
bool app_audio_pa_is_on(void);

/* ============================================================
 *  ★★★ 10-11 新增：麦克风（ES8311 的 ADC 通路）—— 为 2.8 的语音功能
 * ============================================================
 *  板载麦克风：MEMS **LMA2718B381-OA7**（模拟麦，接 ES8311 的 MIC 输入）。
 *  `I2S DIN = PIN_I2S_DIN` 早就配好了（只是以前建 I2S 时第三个参数传 NULL，
 *  压根没建接收通道）。
 *
 *  ⚠️⚠️ 三条硬约束（都实测过，不是猜）：
 *    ① **采样率由 I2S 时钟决定，TX/RX 共用 BCLK/WS** ⇒ 开麦的采样率
 *       【必须等于】当时的播放采样率。而 esp-sr 要 16 kHz 单声道、
 *       播放是 44.1 kHz 立体声 ⇒ 两者不能同时存在。
 *       兰兰 10-11 定的策略是「**暂停/停止时才听**」⇒ 开麦前先把播放停下，
 *       于是可以安全地把时钟切到 16 kHz（零音质代价）。
 *       ⇒ 调用顺序必须是：**先 `app_audio_set_sample_rate(16000)`，再 `app_audio_mic_open()`**。
 *         本模块【不会】自己切采样率 —— 切钟要走 set_sample_rate()，
 *         它带着一整套「关功放 → 软静音 → 改钟 → 灌静音 → 解静音 → 恢复功放」
 *         的过渡（10-11 修爆音那次加的），重复实现一遍必然漏东西。
 *    ② **S3 上 RX 与 TX 是绑定的**：`audio_codec_data_i2s.c:577` 那条
 *       `#if SOC_I2S_HW_VERSION_1` 写着 *"if disable RX, TX also not work"*
 *       ⇒ 单独关 RX 会把播放也弄哑。所以关麦走的是「输入静音」，
 *       **不是**去 disable 通道。
 *    ③ 只有当 `AUD_MIC_ENABLE=1` 编译时才存在（见下）。3.5 那份编出来
 *       与今天完全一致 —— 不为它多占 8 KB 内部 DMA（RX 4 描述符 × 512 帧 × 4 B）。
 */
#ifndef AUD_MIC_ENABLE
#define AUD_MIC_ENABLE   0     /* 默认关：3.5 行为不变；es28 的 CMakeLists 传 1 打开 */
#endif

#if AUD_MIC_ENABLE
/* 开麦：让 ADC 通路进工作态并设好输入增益。
 * ★ 前置：必须已经 `app_audio_set_sample_rate()` 到你要的采样率。
 * 可重复调用（幂等）。*/
esp_err_t app_audio_mic_open(void);

/* 关麦：把输入静音。
 * ⚠️ **不是** disable I2S 通道（见上面 ② —— 那会把播放也弄哑）。
 * 可重复调用（幂等）。*/
esp_err_t app_audio_mic_close(void);

/* 读一段采集数据（阻塞，直到读满或超时）。
 * 格式与播放一致：**16bit 交错双声道**（ES8311 的 ADC 是单声道，
 * 它落在哪一个 slot 由硬件决定 —— 用 app_voice 的电平测试看哪个声道有信号即可）。
 * 返回实际读到的字节数；<0 表示错误。*/
int app_audio_mic_read(void *buf, int bytes);

/* 麦是不是开着。*/
bool app_audio_mic_is_on(void);

/* 麦克风增益（dB，0~42）。
 * ★ 为什么需要"运行时"改：定档要靠真机扫几档比 RMS，
 *   每改一档都重编+重烧约 1 分钟，一轮就是十几分钟。
 *   有了它一次烧录就能扫完。串口命令 `micg <dB>` 走这两个接口。*/
int       app_audio_get_mic_gain(void);
esp_err_t app_audio_set_mic_gain(int db);
#endif /* AUD_MIC_ENABLE */
