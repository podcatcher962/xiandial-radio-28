/* ============================================================
 *  app_voice.c —— 语音唤醒与命令词（ESP-SR）
 *  详见 app_voice.h 顶部的设计说明。本版只做「挂载模型 + 报告」。
 * ============================================================ */
#include "app_voice.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_partition.h"     /* mmap model 分区（模型导出用）*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_path.h"        /* esp_srmodel_init / esp_srmodel_filter */
#include "app_audio.h"
/* ★ 批 C/D：AFE + WakeNet + MultiNet */
#if AUD_MIC_ENABLE
#include "esp_afe_sr_iface.h"   /* esp_afe_sr_iface_t / afe_fetch_result_t */
#include "esp_afe_sr_models.h"  /* esp_afe_handle_from_config() */
#include "esp_afe_config.h"     /* afe_config_init() / afe_config_t */
#include "esp_wn_iface.h"       /* wakenet_state_t / WAKENET_DETECTED */
#include "esp_mn_iface.h"       /* esp_mn_iface_t / esp_mn_results_t / ESP_MN_PREFIX */
#include "esp_mn_models.h"      /* esp_mn_handle_from_name() */
#include "esp_mn_speech_commands.h"  /* esp_mn_commands_add/update/alloc/free */

/* 执行动作要用到的模块（批 D）*/
#include "app_radio.h"
#include "app_pwr.h"
#include "app_fav.h"
#include "app_hist.h"
#include "app_settings.h"
#include "app_stlist.h"
#include "net_stations.h"       /* g_cat_name / NET_CAT_N / net_station_t */
#endif

static const char *TAG = "XSVOICE";

/* 模型分区名 —— 与 partitions.csv 的 `model` 行一致。
 * ⚠️ 改分区名要同时改这里、partitions.csv、_prep_voice_models.py 和烧录脚本。*/
#define VOICE_PARTITION   "model"

/* ★★★ 10-11 晚（第二轮）：模型走 **SPIFFS 分区** —— 这是 MultiNet7 唯一的家。
 *  完整证据链在 `sdkconfig.defaults` 与 `main/CMakeLists.txt` 两处注释里；
 *  一句话版：libmultinet 建 FST 需要「真 base_path」+「SPIFFS 参数」**两个**条件，
 *  三条路里只有 `srmodel_spiffs_init()` 同时满足
 *  （packed 被 `set_model_base_path(NULL)` 毁掉路径；SD 卡没有 SPIFFS 参数）。
 *  ⚠️ 曾经写过的「固件把模型导出到 SD 卡」那套已**删除**：
 *     SD 卡模式会在 libhufzip 的 `read_spiffs_params` 除零崩（hufzip_model.c:323）。*/

static srmodel_list_t *s_models = NULL;
static bool            s_ready  = false;

esp_err_t app_voice_init(void)
{
    if (s_models != NULL) return ESP_OK;

    /* ---- 唯一路径：SPIFFS 模式 ----
     *  `srmodel_spiffs_init(part)` 做两件事（正是 MultiNet7 要的两条命门）：
     *   ① 把 model 分区挂到 `/srmodel`，而 esp-sr 的全局基路径
     *      `SRMODE_BASE_PATH` 初值就是 `"/srmodel"`（model_path.c:20）
     *      —— 且这条路径**不会**像 mmap 那样被 `set_model_base_path(NULL)` 毁掉
     *      ⇒ libmultinet 能拼出 `/srmodel/mn7_cn/vocab`、`/srmodel/fst/commands_cn.txt`；
     *   ② SPIFFS 在位 ⇒ `read_spiffs_params` 有值 ⇒ libhufzip 除零问题消失。
     *  ⚠️ 它内部 `format_if_mount_failed = true` ⇒ 分区若不是有效 SPIFFS 镜像
     *     （比如误烧了旧的 packed 包），**它会把分区格式化**、然后 0 个模型。
     *     现象是"模型没挂上"，不是崩溃 —— 所以这里必须把 num<=0 当失败报出来。*/
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, VOICE_PARTITION);
    if (part == NULL) {
        ESP_LOGE(TAG, "分区表里找不到 %s 分区", VOICE_PARTITION);
        return ESP_ERR_NOT_FOUND;
    }

    s_models = srmodel_spiffs_init(part);
    if (s_models == NULL || s_models->num <= 0) {
        /* ⚠️ 不当致命错误：语音坏了不影响听电台（见 app_voice.h 的设计约定）。*/
        ESP_LOGE(TAG, "模型没挂上 —— 语音不可用");
        ESP_LOGE(TAG, "  排查：model 分区烧的是 build/model.bin（SPIFFS 镜像）吗？"
                      "（`_flash_es28.py voice`）");
        return ESP_FAIL;
    }

    /* ★★★ 修掉 `read_models_form_spiffs()` 的 off-by-one —— 不修就是"静默失效"。
     *  它按 `memcpy(name, d_name, len - 13)` 剪模型名（model_path.c:236），
     *  而后缀 `_MODEL_INFO_` 只有 12 字符 ⇒ **剪出来尾部多一个下划线**：
     *      文件名 `mn7_cn__MODEL_INFO_`(20) → 前 7 字符 → `mn7_cn_`
     *  这个多出的 '_' 会一路传下去：
     *      esp_srmodel_filter → esp_mn_handle_from_name("mn7_cn_")
     *      → libmultinet 拼 `/srmodel/mn7_cn_/vocab` ⇒ **目录不存在**
     *      ⇒ 又回到 `no ... cannot build fst` 崩溃。
     *  ⇒ 就地剪掉尾部所有 '_'，让它与真实目录名精确相等。
     *  ⚠️ 这【不是】在掩盖问题：esp-sr 这个函数就是这么写的（连官方 SPIFFS
     *     示例也得靠平铺命名去凑），剪裁是最小且可解释的补偿。*/
    for (int i = 0; i < s_models->num; i++) {
        char *n = s_models->model_name[i];
        if (n == NULL) continue;
        size_t L = strlen(n);
        size_t had = L;
        while (L > 0 && n[L - 1] == '_') n[--L] = '\0';
        if (had != L)
            ESP_LOGW(TAG, "  名字修剪：%s → %s（esp-sr 解析多带的下划线）",
                     s_models->model_name[i] ? n : "?", n);
    }

    ESP_LOGI(TAG, "模型已挂载（SPIFFS /srmodel）：%d 项", s_models->num);
    for (int i = 0; i < s_models->num; i++)
        ESP_LOGI(TAG, "  [%d] %s", i,
                 s_models->model_name[i] ? s_models->model_name[i] : "?");
    s_ready = true;
    return ESP_OK;
}

bool app_voice_ready(void)
{
    return s_ready;
}

const char *app_voice_model_name(int idx)
{
    if (s_models == NULL || idx < 0 || idx >= s_models->num) return NULL;
    return s_models->model_name[idx];
}

/* ============================================================
 *  ★ 10-11 批 B：麦克风电平自检（见 app_voice.h 那段说明）
 * ============================================================ */
#if AUD_MIC_ENABLE

/* 读一块的大小：4096 B = 1024 帧（16bit 交错双声道）
 * 按 44100 算约 23 ms、按 16000 算约 64 ms ⇒ 每档读几十次就够统计了。
 * 放 static 而不是栈上：4 KB 在 4 KB 栈的任务里会直接爆栈。*/
#define MIC_CHUNK      4096
static int16_t s_mic_buf[MIC_CHUNK / 2];

/* 采集一段时间，统计【每个声道】的 RMS 与峰值，然后打一行日志。
 *
 * ★ 为什么分声道统计而不是合并成一个数：
 *   ES8311 的 ADC 是**单声道**，它落在交错数据的哪个 slot（左还是右）
 *   由硬件/驱动决定，**从文档上看不出**。分开打就一次性看清了
 *   —— 有信号的那个声道就是麦，另一个会是恒 0。
 *   （10-11 实测：**L 有数、R 恒 0** ⇒ 麦在左声道。）
 *
 * ★★ 为什么前半/后半分开统计（10-11 加）：
 *   第一次实测出现一个矛盾：同增益（30 dB）下
 *     档① 44100 Hz → RMS 162 ／ 峰值 757      （安静）
 *     档② 16000 Hz → RMS 3169 ／ 峰值 32767   （削顶）
 *   同一只麦、同一增益，唯一的差别是**档②之前刚改过时钟**
 *   （`app_audio_set_sample_rate` 会关/开 codec 与 I2S）。
 *   ⇒ 强烈怀疑是 **ADC 在时钟切换后的启动瞬态**（一两个大毛刺），
 *     而不是"16k 档真的更响"。
 *   把一半切成两半就能一眼判定：
 *     前半爆表、后半正常 ⇒ 瞬态（丢掉开头即可，增益不用动）
 *     前后半都爆表         ⇒ 真的增益过高（要降 MIC_GAIN_DB）
 *   同时丢掉开头 200 ms 作为稳定期 —— 同理，
 *   `esp_codec_dev_open` 之后 ADC 需要一小段才稳。*/
#define MIC_SETTLE_MS  200

static void mic_pass(const char *tag, int ms)
{
    const int frames = MIC_CHUNK / 4;          /* 每帧 4 B（2ch × 16bit）*/
    const int64_t t0 = esp_timer_get_time();
    int64_t half = (int64_t)ms * 500;          /* 半程的截止（微秒）*/
    int64_t sl = 0, sr = 0, sl2 = 0, sr2 = 0;
    int n = 0, n2 = 0, peak_l = 0, peak_r = 0;
    bool settled = false;

    while ((esp_timer_get_time() - t0) < (int64_t)ms * 1000) {
        int got = app_audio_mic_read(s_mic_buf, MIC_CHUNK);
        if (got < 0) {
            ESP_LOGE(TAG, "  [%s] 读失败 —— 通路没通", tag);
            return;
        }
        int64_t el = esp_timer_get_time() - t0;

        /* 稳定期：只读不统计（丢数据但不丢时间轴）*/
        if (!settled && el < (int64_t)MIC_SETTLE_MS * 1000) continue;
        settled = true;

        for (int i = 0; i < frames; i++) {
            int l = s_mic_buf[i * 2];
            int r = s_mic_buf[i * 2 + 1];
            sl += (int64_t)l * l;
            sr += (int64_t)r * r;
            if (abs(l) > peak_l) peak_l = abs(l);
            if (abs(r) > peak_r) peak_r = abs(r);
            if (el < half) { sl2 += (int64_t)l * l; sr2 += (int64_t)r * r; n2++; }
        }
        n += frames;
    }

    if (n == 0) { ESP_LOGE(TAG, "  [%s] 一帧都没读到", tag); return; }
    int rms_l = (int)sqrt((double)sl / n);
    int rms_r = (int)sqrt((double)sr / n);
    int h_l   = n2 ? (int)sqrt((double)sl2 / n2) : 0;
    int h_r   = n2 ? (int)sqrt((double)sr2 / n2) : 0;
    ESP_LOGW(TAG, "  [%s] %d 帧（丢前 %d ms）", tag, n, MIC_SETTLE_MS);
    ESP_LOGW(TAG, "      全程 RMS L=%-6d R=%-6d ｜ 峰值 L=%-6d R=%-6d",
             rms_l, rms_r, peak_l, peak_r);
    ESP_LOGW(TAG, "      前半 RMS L=%-6d R=%-6d   ← 比后半大很多 = 换钟瞬态",
             h_l, h_r);
}

esp_err_t app_voice_mic_test(int ms)
{
    if (!app_audio_ready()) {
        ESP_LOGE(TAG, "音频没就绪，麦测试无从谈起");
        return ESP_ERR_INVALID_STATE;
    }
    if (ms < 300)  ms = 300;
    if (ms > 5000) ms = 5000;

    const int sr0 = app_audio_sample_rate();
    ESP_LOGW(TAG, "=== 麦电平自检开始（每档 %d ms，当前采样率 %d Hz，增益 %d dB）===",
             ms, sr0, app_audio_get_mic_gain());

    /* ---- 档①：当前采样率（通常 44100）---- */
    if (app_audio_mic_open() != ESP_OK) {
        ESP_LOGE(TAG, "档① 开麦失败 ⇒ 后面的档也不用试了");
        return ESP_FAIL;
    }
    mic_pass("档① 原始采样率", ms);
    app_audio_mic_close();

    /* ---- 档②：16000 Hz（esp-sr 要的档）---- */
    esp_err_t e = app_audio_set_sample_rate(16000);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "档② 切 16000 Hz 失败：%s", esp_err_to_name(e));
    } else if (app_audio_mic_open() != ESP_OK) {
        ESP_LOGE(TAG, "档② 开麦失败");
    } else {
        mic_pass("档② 16000 Hz", ms);
        app_audio_mic_close();
    }

    /* ---- 还原采样率：绝不能把播放链路留在 16 kHz（那会让电台变调）---- */
    if (app_audio_set_sample_rate(sr0) != ESP_OK) {
        ESP_LOGE(TAG, "⚠️ 采样率没能还原到 %d Hz —— 下次播放可能变调", sr0);
    }
    ESP_LOGW(TAG, "=== 麦电平自检结束（采样率已还原 %d Hz）===", sr0);
    ESP_LOGW(TAG, "  判读：安静时 RMS 应 <1000；说话/拍手时应明显抬到数千以上（<20000 不削顶）。");
    ESP_LOGW(TAG, "  L/R 只有一个有数 ⇒ 麦在那个声道（ES8311 的 ADC 是单声道，实为 L）。");
    ESP_LOGW(TAG, "  改增益后重测：`micg <dB>`（0~42）");
    return ESP_OK;
}

/* ============================================================
 *  ★ 10-11 批 C：唤醒（AFE + WakeNet）—— 设计见 app_voice.h
 * ============================================================ */
#define VOICE_SR        16000          /* AFE 固定要 16 kHz；不是可调项 */
#define VOICE_TASK_STK  6144
#define MIC_RAW_MAX     4096           /* 立体声 int16 个数（够 2048 帧）*/
#define MN_TIMEOUT_MS   6000           /* 唤醒后给多久说命令（6 秒）*/

static TaskHandle_t      s_task      = NULL;
static volatile bool     s_run       = false;
static volatile int       s_wake_cnt = 0;
static int                s_sr_backup = 44100;   /* 进来前的采样率，stop 时还原 */
/* ★ 批 D：命中了"需要独占音频通路"的命令时，先记在这里，
 *   等收尾段（麦已关、采样率已还原）再执行 —— 见 voice_task 末尾。*/
static volatile int       s_pending_act = 0;

/* ============================================================
 *  ★★ 10-11 批 D：命令词表 + 动作
 * ============================================================
 *  ⚠️⚠️ **中文命令词的 string 参数是【拼音】，不是汉字**。
 *    依据（实读源码，不是猜）：`esp_mn_commands_add()` 里
 *      `#ifdef CONFIG_SR_MN_EN_MULTINET7_QUANT` 才会调 flite_g2p 做
 *      「拼写→音素」，**中文走 #else 分支**，直接把 string 当音素串用；
 *      组件自带的 `model/multinet_model/fst/commands_cn.txt` 里也全是拼音
 *      （`1,ba xiao shi hou guan ji` = 「八小时后关机」）。
 *    ⇒ 下面的拼音全部由 pypinyin 生成（多音字不会错），不要手写。
 *
 *  ★ `command_id` 就是【动作编号】，同一个动作可以挂多条不同说法
 *    （API 本来就支持同 id 多 phrase = 同义词）⇒ 说「下一台」和
 *    「播放下一个」落到同一个 id，执行代码只写一份。
 *  ★ 表的顺序无关；`ESP_MN_MAX_PHRASE_NUM = 400`，我们 36 条远没到上限。*/
typedef enum {
    CMD_NONE = 0,
    CMD_SHUTDOWN = 1,   /* 关机（真关机，走 app_pwr_shutdown）*/
    CMD_PLAY     = 2,   /* 播放 / 继续 */
    CMD_PAUSE    = 3,   /* 暂停 */
    CMD_STOP     = 4,   /* 停止播放 */
    CMD_NEXT     = 5,   /* 下一台 */
    CMD_PREV     = 6,   /* 上一台 */
    CMD_VOL_UP   = 7,   /* 声音大一点 */
    CMD_VOL_DOWN = 8,   /* 声音小一点 */
    CMD_MUTE     = 9,   /* 静音 / 取消静音（同一个 toggle）*/
    CMD_FAV_ADD  = 10,  /* 收藏这台 */
    CMD_FAV_DEL  = 11,  /* 取消收藏 */
    CMD_REPLAY   = 12,  /* 再听一次（从头重放当前这条）*/
    /* 13~22 是栏目点播：动作编号本身不表达栏目，靠下表右列的 cat 映射 */
    CMD_CAT_1 = 13,     /* 播放新闻 */
    CMD_CAT_2 = 14,     /* 播放交通 */
    CMD_CAT_3 = 15,     /* 播放音乐 */
    CMD_CAT_4 = 16,     /* 播放文艺 */
    CMD_CAT_5 = 17,     /* 播放说书 */
    CMD_CAT_6 = 18,     /* 播放戏曲 */
    CMD_CAT_7 = 19,     /* 播放怀旧 */
    CMD_CAT_8 = 20,     /* 播放教育 */
    CMD_CAT_9 = 21,     /* 播放宗教 */
    CMD_CAT_10 = 22,    /* 播放境外 */
} voice_act_t;

typedef struct {
    int         id;      /* = voice_act_t */
    const char *py;      /* 拼音（pypinyin 生成）*/
    const char *zh;      /* 中文，只用于日志与串口回显 */
} voice_phrase_t;

static const voice_phrase_t k_phrases[] = {
    { CMD_SHUTDOWN, "guan ji",                "关机" },
    { CMD_SHUTDOWN, "guan bi dian yuan",      "关闭电源" },
    { CMD_PLAY,     "bo fang",                "播放" },
    { CMD_PLAY,     "ji xu bo fang",          "继续播放" },
    { CMD_PLAY,     "bo fang dian tai",       "播放电台" },
    { CMD_PAUSE,    "zan ting",               "暂停" },
    { CMD_STOP,     "ting zhi bo fang",       "停止播放" },
    { CMD_STOP,     "ting zhi",               "停止" },
    { CMD_NEXT,     "xia yi tai",             "下一台" },
    { CMD_NEXT,     "bo fang xia yi ge",      "播放下一个" },
    { CMD_NEXT,     "xia yi ge",              "下一个" },
    { CMD_PREV,     "shang yi tai",           "上一台" },
    { CMD_PREV,     "shang yi ge",            "上一个" },
    { CMD_VOL_UP,   "sheng yin da yi dian",   "声音大一点" },
    { CMD_VOL_UP,   "da sheng yi dian",       "大声一点" },
    { CMD_VOL_UP,   "yin liang da yi dian",   "音量大一点" },
    { CMD_VOL_DOWN, "sheng yin xiao yi dian", "声音小一点" },
    { CMD_VOL_DOWN, "xiao sheng yi dian",     "小声一点" },
    { CMD_VOL_DOWN, "yin liang xiao yi dian", "音量小一点" },
    { CMD_MUTE,     "jing yin",               "静音" },
    { CMD_MUTE,     "qu xiao jing yin",       "取消静音" },
    { CMD_FAV_ADD,  "shou cang zhe tai",      "收藏这台" },
    { CMD_FAV_ADD,  "shou cang",              "收藏" },
    { CMD_FAV_DEL,  "qu xiao shou cang",      "取消收藏" },
    { CMD_REPLAY,   "zai ting yi ci",         "再听一次" },
    { CMD_REPLAY,   "chong xin bo fang",      "重新播放" },
    { CMD_CAT_1,    "bo fang xin wen",        "播放新闻" },
    { CMD_CAT_2,    "bo fang jiao tong",      "播放交通" },
    { CMD_CAT_3,    "bo fang yin yue",        "播放音乐" },
    { CMD_CAT_4,    "bo fang wen yi",         "播放文艺" },
    { CMD_CAT_5,    "bo fang shuo shu",       "播放说书" },
    { CMD_CAT_6,    "bo fang xi qu",          "播放戏曲" },
    { CMD_CAT_7,    "bo fang huai jiu",       "播放怀旧" },
    { CMD_CAT_8,    "bo fang jiao yu",        "播放教育" },
    { CMD_CAT_9,    "bo fang zong jiao",      "播放宗教" },
    { CMD_CAT_10,   "bo fang jing wai",       "播放境外" },
};
#define PHRASE_N (int)(sizeof(k_phrases) / sizeof(k_phrases[0]))

/* 动作 → 栏目下标（只对 CMD_CAT_* 有意义）。
 * ⚠️ 这张映射必须与 net_stations.c 的 `g_cat_name[]` 顺序一致 ——
 *   那边改栏目顺序，这里要跟着改。
 *   实测 g_cat_name = [0]新闻综合 [1]交通台 [2]音乐 [3]文艺 [4]说书 [5]戏曲
 *                     [6]怀旧老歌 [7]网络台 [8]教育台 [9]电视伴音 [10]综合
 *                     [11]宗教 [12]境外新闻
 *   ⇒ 我们只取语义明确的 10 个（跳过 [7]网络台/[9]电视伴音/[10]综合 ——
 *     "播放网络台"这种说法含糊，宁可不说）。*/
static int act_to_cat(int act)
{
    switch (act) {
    case CMD_CAT_1:  return 0;
    case CMD_CAT_2:  return 1;
    case CMD_CAT_3:  return 2;
    case CMD_CAT_4:  return 3;
    case CMD_CAT_5:  return 4;
    case CMD_CAT_6:  return 5;
    case CMD_CAT_7:  return 6;
    case CMD_CAT_8:  return 8;
    case CMD_CAT_9:  return 11;
    case CMD_CAT_10: return 12;
    default:         return -1;
    }
}

/* 这条动作要不要独占音频通路（需要退出语音模式、把时钟还给 44.1 kHz）？
 *  · 不改 I2S 的（音量/静音/收藏）→ 就地执行、**继续听**（体验更好：
 *    喊一次「Hi 喵喵」可以连着调好几次音量）
 *  · 要起播放的（播放/切台/栏目点播/重放/停止）→ 必须退出语音模式。*/
static bool act_needs_audio(int act)
{
    switch (act) {
    case CMD_VOL_UP:
    case CMD_VOL_DOWN:
    case CMD_MUTE:
    case CMD_FAV_ADD:
    case CMD_FAV_DEL:
    case CMD_SHUTDOWN:      /* 关机也不碰 I2S（它自己会关功放/关 WiFi）*/
        return false;
    default:
        return true;
    }
}

/* ---- 16 kHz 提示音 ----
 *  ⚠️ 不能用现成的 `app_audio_beep_async()`：那个按 `AUD_SR`(44100) 算频率，
 *    而语音模式下的 I2S 是 16 kHz ⇒ 直接放会变成又长又低的一声怪响。
 *  这里按【当前实际采样率】现算，短促两声（"嘀-嘀"下降），
 *  包络做淡入淡出避免爆音。*/
static void voice_beep(int freq_hi, int freq_lo)
{
    enum { N = 256 };                  /* 256 帧一块 */
    static int16_t b[N * 2];
    const int parts[2] = { 70, 90 };   /* 两个音的时长（ms）*/
    int f[2] = { freq_hi, freq_lo };

    for (int k = 0; k < 2; k++) {
        int total = VOICE_SR * parts[k] / 1000;
        for (int done = 0; done < total; ) {
            int n = total - done;
            if (n > N) n = N;
            for (int i = 0; i < n; i++) {
                int pos = done + i;
                int32_t v = (int32_t)(sin(2.0 * 3.14159265358979
                                          * f[k] * pos / VOICE_SR) * 7000.0);
                /* 淡入淡出：各占 1/4，防"啪" */
                int ramp = total / 4;
                if (ramp > 0) {
                    if (pos < ramp)                 v = v * pos / ramp;
                    else if (pos > total - ramp)    v = v * (total - pos) / ramp;
                }
                b[i * 2] = (int16_t)v;
                b[i * 2 + 1] = (int16_t)v;
            }
            app_audio_write(b, (size_t)n * 4);
            done += n;
        }
    }
}

/* ---- 在【当前没在放音】的状态下执行一个动作 ----
 *  ⚠️ 调用时机有两种，都必须满足"语音模式已退出、采样率已还原"：
 *    ① 语音任务收尾时（s_task 已置 NULL 之后）
 *    ② 「轻」动作在语音任务循环里就地执行（音量/静音/收藏 —— 它们不碰 I2S）*/
static void voice_exec(int act)
{
    const char *zh = "?";
    for (int i = 0; i < PHRASE_N; i++)
        if (k_phrases[i].id == act) { zh = k_phrases[i].zh; break; }

    int cat = act_to_cat(act);
    if (cat >= 0) {
        /* ---- 栏目点播 ----
         * 挑法：**如果现在正在放这个栏目里的台，就跳到它的下一个**
         * ⇒ 连着说两次「播放音乐」会换一个台，而不是反复播同一个，
         *   这样"再说一次"天然变成"换一个"，不用额外命令。*/
        int n = app_radio_station_count();
        int cur = app_radio_station_current();
        int start = 0;
        if (cur >= 0 && cur < n && app_radio_is_stream()) {
            const net_station_t *s = app_st_get(cur);
            if (s && (int)(s->cat % NET_CAT_N) == cat) start = cur + 1;
        }
        int pick = -1;
        for (int k = 0; k < n; k++) {
            int i = (start + k) % n;
            const net_station_t *s = app_st_get(i);
            if (s && (int)(s->cat % NET_CAT_N) == cat) { pick = i; break; }
        }
        if (pick < 0) {
            ESP_LOGW(TAG, "语音「%s」：这个栏目里没有台", zh);
            return;
        }
        ESP_LOGW(TAG, "语音执行「%s」⇒ 台 #%d %s（栏目 %s）",
                 zh, pick, app_st_get(pick)->name, g_cat_name[cat]);
        app_radio_play_station(pick);
        return;
    }

    switch (act) {
    case CMD_SHUTDOWN:
        ESP_LOGW(TAG, "语音执行「%s」⇒ 真关机", zh);
        app_pwr_shutdown();          /* 不返回 */
        break;

    case CMD_PLAY:
        ESP_LOGW(TAG, "语音执行「%s」", zh);
        if (app_radio_is_paused()) {
            app_radio_set_paused(false);          /* 暂停中 ⇒ 继续 */
        } else if (app_radio_is_playing()) {
            ESP_LOGW(TAG, "  已经在放了");
        } else {
            /* 完全没在放 ⇒ 接着上次听的来（历史第一条），没有就台单第一个。
             * ★ 不静默失败：喊了就该有反应。*/
            int pool[APP_HIST_MAX];
            int n = app_hist_top(pool, APP_HIST_MAX);
            if (n > 0 && pool[0] >= 0 && pool[0] < app_radio_station_count()) {
                ESP_LOGW(TAG, "  没在放 ⇒ 接着上次听的 #%d %s",
                         pool[0], app_st_get(pool[0])->name);
                app_radio_play_station(pool[0]);
            } else if (app_radio_station_count() > 0) {
                ESP_LOGW(TAG, "  没在放也没有历史 ⇒ 从台单第一个起播");
                app_radio_play_station(0);
            } else {
                ESP_LOGW(TAG, "  台单是空的，无从播起");
            }
        }
        break;

    case CMD_PAUSE:
        ESP_LOGW(TAG, "语音执行「%s」", zh);
        if (app_radio_is_playing() && !app_radio_is_paused()) {
            app_radio_set_paused(true);
        } else {
            ESP_LOGW(TAG, "  当前没在放，暂停无意义");
        }
        break;

    case CMD_STOP:
        ESP_LOGW(TAG, "语音执行「%s」", zh);
        app_radio_stop();
        break;

    case CMD_NEXT:
    case CMD_PREV: {
        int step = (act == CMD_NEXT) ? 1 : -1;
        ESP_LOGW(TAG, "语音执行「%s」", zh);
        if (!app_radio_is_playing()) {
            /* 没在播时"下一台"没有参照点 —— 按台单第一条起播，
             * 而不是静默失败（用户喊了就该有反应）。*/
            ESP_LOGW(TAG, "  当前没在放 ⇒ 从台单第一个起播");
            if (app_radio_station_count() > 0) app_radio_play_station(0);
        } else {
            int r = app_radio_station_step(step);
            if (r < 0) ESP_LOGW(TAG, "  换台失败（到头了？）");
        }
        break;
    }

    case CMD_VOL_UP:
    case CMD_VOL_DOWN: {
        int step = (act == CMD_VOL_UP) ? 10 : -10;
        int v = app_audio_get_volume() + step;
        if (v < 0)   v = 0;
        if (v > 100) v = 100;
        if (v > 0 && xs_cfg_muted()) xs_cfg_set_mute(false);
        xs_cfg_set_volume(v);        /* 落盘，重启还记得 */
        app_audio_set_volume(v);
        ESP_LOGW(TAG, "语音执行「%s」⇒ 音量 %d%%", zh, v);
        break;
    }

    case CMD_MUTE:
        ESP_LOGW(TAG, "语音执行「%s」⇒ %s", zh,
                 xs_cfg_muted() ? "取消静音" : "静音");
        xs_cfg_toggle_mute();
        break;

    case CMD_FAV_ADD:
    case CMD_FAV_DEL: {
        int cur = app_radio_station_current();
        ESP_LOGW(TAG, "语音执行「%s」", zh);
        if (cur < 0 || !app_radio_is_stream()) {
            ESP_LOGW(TAG, "  现在放的不是电台 ⇒ 收藏没有意义");
            break;
        }
        if (act == CMD_FAV_ADD) {
            if (!app_fav_has(cur)) { app_fav_add(cur); ESP_LOGW(TAG, "  已收藏 %s", app_st_get(cur)->name); }
            else                   { ESP_LOGW(TAG, "  已经在收藏里了"); }
        } else {
            if (app_fav_has(cur))  { app_fav_remove(cur); ESP_LOGW(TAG, "  已取消收藏"); }
            else                   { ESP_LOGW(TAG, "  它本来就不在收藏里"); }
        }
        break;
    }

    case CMD_REPLAY:
        ESP_LOGW(TAG, "语音执行「%s」", zh);
        if (app_radio_is_stream()) {
            ESP_LOGW(TAG, "  直播流没有\"从头\"这回事");
        } else if (app_radio_is_playing()) {
            app_radio_seek(0);
        } else {
            ESP_LOGW(TAG, "  当前没在放");
        }
        break;

    default:
        ESP_LOGW(TAG, "语音动作 %d 没有实现", act);
        break;
    }
}

/* 独立任务而不是在 es28_main 的串口任务里跑：
 * 串口任务还要处理命令，被这个死循环占住就再也收不到 `wl off` 了。*/
static void voice_task(void *arg)
{
    (void)arg;
    int16_t *raw  = NULL;      /* 立体声交错，直接从 I2S RX 读出来 */
    int16_t *mono = NULL;      /* 取左声道后的单声道，喂给 AFE */
    int      chunk = 0;
    const esp_afe_sr_iface_t *iface = NULL;
    esp_afe_sr_data_t        *afe   = NULL;
    afe_config_t             *cfg   = NULL;
    /* 批 D：MultiNet */
    esp_mn_iface_t    *mn_iface = NULL;
    model_iface_data_t *mn_data = NULL;
    int                mn_chunk = 0;
    bool               mn_ready = false;

    /* ---- ① 切到 16 kHz 并开麦（顺序不能反，见 app_voice.h 的 ①）---- */
    s_sr_backup = app_audio_sample_rate();
    if (s_sr_backup != VOICE_SR) {
        if (app_audio_set_sample_rate(VOICE_SR) != ESP_OK) {
            ESP_LOGE(TAG, "切 16 kHz 失败 ⇒ 不能开听");
            goto out;
        }
    }
    if (app_audio_mic_open() != ESP_OK) {
        ESP_LOGE(TAG, "开麦失败 ⇒ 不能开听");
        goto out;
    }

    /* ---- ② 建 AFE ----
     *  input_format="M" = 单个麦克风（不是麦克风阵列）。
     *  ⚠️ 我们从 I2S 拿到的是【立体声】，但 AFE 这里声明的输入是【单声道】
     *     ⇒ 每一轮都要先把左声道抠出来再喂（下面 ③）。*/
    cfg = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (cfg == NULL) {
        ESP_LOGE(TAG, "afe_config_init 返回 NULL");
        goto out;
    }
    /* 内部 RAM 只剩约 177 KB，AFE 的大缓冲放 PSRAM（板上有 7.8 MB 空闲）。*/
    cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    iface = esp_afe_handle_from_config(cfg);
    if (iface == NULL) {
        ESP_LOGE(TAG, "esp_afe_handle_from_config 返回 NULL");
        goto out;
    }
    afe = iface->create_from_config(cfg);
    if (afe == NULL) {
        ESP_LOGE(TAG, "AFE create 失败（多半是内存不够，看上面的堆日志）");
        goto out;
    }

    /* ★ 注意 `get_feed_chunksize` 与 `get_fetch_chunksize` 是【两个不同的量】：
     *   feed  = 我们要【喂给它】的样本数（本循环用这个）
     *   fetch = 它【吐出来】的样本数（给 MultiNet 用，批 D 才需要）
     *   v2.6.0 的接口里 feed 的那个成员名就叫 `get_feed_chunksize`
     *   （旧版叫 get_samp_chunksize —— 我第一版就是照旧名写的，编不过）。*/
    chunk = iface->get_feed_chunksize(afe);
    int chunk_f = iface->get_fetch_chunksize(afe);
    int chn = iface->get_channel_num(afe);
    int sr  = iface->get_samp_rate(afe);
    ESP_LOGW(TAG, "AFE 就绪：feed=%d 样本/轮 ｜ fetch=%d ｜ 通道=%d ｜ 采样率=%d Hz",
             chunk, chunk_f, chn, sr);
    ESP_LOGW(TAG, "  唤醒词模型清单里若含 wn9_himiaomiao_tts ⇒ 喊「Hi，喵喵」");
    if (chunk <= 0 || chunk > MIC_RAW_MAX / 2) {
        ESP_LOGE(TAG, "feedsize %d 超出缓冲上限 %d ⇒ 拒绝启动（喂错大小会静默不唤醒）",
                 chunk, MIC_RAW_MAX / 2);
        goto out;
    }

    raw  = malloc((size_t)chunk * 4);        /* 立体声 16bit */
    mono = malloc((size_t)chunk * 2);        /* 单声道 16bit */
    if (raw == NULL || mono == NULL) {
        ESP_LOGE(TAG, "缓冲分配失败（需要 %d B + %d B）", chunk * 4, chunk * 2);
        goto out;
    }

    /* ---- ②b 建 MultiNet（批 D）----
     *  模型名从已挂载清单里按关键词找：`esp_srmodel_filter(models, "mn", "cn")`
     *  ⇒ 命中的就是 `mn7_cn`（我们只打包了它一个中文命令词模型）。*/
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    if (mn_name == NULL) {
        ESP_LOGE(TAG, "没有中文命令词模型（打包时漏了 mn7_cn？）⇒ 只能唤醒、听不懂命令");
    } else {
        mn_iface = esp_mn_handle_from_name(mn_name);
        if (mn_iface == NULL) {
            ESP_LOGE(TAG, "esp_mn_handle_from_name(%s) 失败", mn_name);
        } else {
            mn_data = mn_iface->create(mn_name, MN_TIMEOUT_MS);
            if (mn_data == NULL) {
                ESP_LOGE(TAG, "MultiNet create 失败");
            } else {
                mn_chunk = mn_iface->get_samp_chunksize(mn_data);
                ESP_LOGW(TAG, "MultiNet 就绪：%s ｜ 语言=%s ｜ %d 样本/轮 ｜ 超时 %d ms",
                         mn_name, mn_iface->get_language(mn_data),
                         mn_chunk, MN_TIMEOUT_MS);
                /* ⚠️ AFE 吐给我们的长度必须与 MultiNet 要的一致，否则喂进去
                 *   就是**静默**的错位（识别率极低而不报错）。
                 *   不一致时直接放弃命令词，只保留唤醒 —— 宁可少功能，不要假功能。*/
                if (mn_chunk != chunk_f) {
                    ESP_LOGE(TAG, "⚠️ AFE fetch(%d) ≠ MultiNet chunk(%d) ⇒ "
                                  "不启用命令词（喂错长度会静默失灵）",
                             chunk_f, mn_chunk);
                    mn_iface->destroy(mn_data);
                    mn_data = NULL;
                } else if (esp_mn_commands_alloc(mn_iface, mn_data) == ESP_OK) {
                    int ok = 0;
                    for (int i = 0; i < PHRASE_N; i++) {
                        if (esp_mn_commands_add(k_phrases[i].id,
                                                k_phrases[i].py) == ESP_OK) ok++;
                        else ESP_LOGW(TAG, "  命令词被拒：%s (%s)",
                                      k_phrases[i].zh, k_phrases[i].py);
                    }
                    if (esp_mn_commands_update() == NULL) {
                        mn_ready = true;
                        ESP_LOGW(TAG, "命令词已装 %d/%d 条（唤醒后 %d 秒内说命令）",
                                 ok, PHRASE_N, MN_TIMEOUT_MS / 1000);
                    } else {
                        ESP_LOGE(TAG, "esp_mn_commands_update 失败 ⇒ 不启用命令词");
                        esp_mn_commands_free();
                        mn_iface->destroy(mn_data);
                        mn_data = NULL;
                    }
                } else {
                    ESP_LOGE(TAG, "esp_mn_commands_alloc 失败");
                    mn_iface->destroy(mn_data);
                    mn_data = NULL;
                }
            }
        }
    }

    s_wake_cnt = 0;
    ESP_LOGW(TAG, "=== 开始听（喊「Hi，喵喵」%s）===",
             mn_ready ? "，然后说命令" : "（命令词不可用）");

    /* ---- ③ 主循环：两态状态机 ----
     *   WAIT_WAKE：只盯 WakeNet
     *   WAIT_CMD ：把 AFE 吐出的数据喂 MultiNet 认命令 */
    int  n_read_err = 0;
    bool wait_cmd   = false;
    while (s_run) {
        int got = app_audio_mic_read(raw, chunk * 4);
        if (got < 0) {
            if (n_read_err++ < 3) ESP_LOGW(TAG, "mic read 失败");
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        /* 抠左声道（实测麦在左；右声道恒 0）——见 app_voice.h 的数据流说明 */
        for (int i = 0; i < chunk; i++) mono[i] = raw[i * 2];

        int fr = iface->feed(afe, mono);
        if (fr < 0) {
            /* feed 失败也要让出，否则就是 10-11 那次"错误路径不让出 CPU"的翻版 */
            if (n_read_err++ < 3) ESP_LOGW(TAG, "afe feed 返回 %d", fr);
            vTaskDelay(1);
            continue;
        }

        afe_fetch_result_t *r = iface->fetch(afe);
        if (r == NULL) continue;

        if (!wait_cmd) {
            /* ---------- 等唤醒 ---------- */
            if (r->wakeup_state == WAKENET_DETECTED) {
                s_wake_cnt++;
                ESP_LOGW(TAG, "★★★ 唤醒 #%d ！音量 %.1f dBFS ｜ 模型#%d",
                         s_wake_cnt, r->data_volume, r->wakenet_model_index);
                if (mn_ready) {
                    /* 提示音 = "我在听"（用户唯一的即时反馈）
                     * ⚠️ 喇叭声会被自己的麦收进去 —— 但它短（160 ms）
                     *   且紧接着是 MultiNet 的识别窗口，风险可接受。
                     *   真出现"提示音把命令吃掉"，再改成识别窗口开始后延迟放。*/
                    voice_beep(1400, 1800);
                    mn_iface->clean(mn_data);   /* 清掉唤醒前的残留状态 */
                    wait_cmd = true;
                    ESP_LOGI(TAG, "  ⇒ 进入命令识别（%d 秒内说命令）", MN_TIMEOUT_MS / 1000);
                }
            } else if (r->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
                ESP_LOGI(TAG, "（通道校验通过，等唤醒词）");
            }
        } else {
            /* ---------- 等命令 ---------- */
            esp_mn_state_t st = mn_iface->detect(mn_data, r->data);
            if (st == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *res = mn_iface->get_results(mn_data);
                if (res && res->num > 0) {
                    int act = res->command_id[0];
                    ESP_LOGW(TAG, "★★★ 命令命中：%s（id=%d，置信度 %.2f）",
                             res->string, act, res->prob[0]);
                    if (act_needs_audio(act)) {
                        /* 要独占音频通路 ⇒ 记下来、退出循环，
                         * 由收尾段（采样率还原之后）执行。*/
                        s_pending_act = act;
                        break;
                    }
                    /* 轻动作：就地执行，然后【继续听】——
                     * 这样喊一次「Hi 喵喵」可以连着调好几次音量。*/
                    voice_exec(act);
                    mn_iface->clean(mn_data);
                    wait_cmd = true;
                }
            } else if (st == ESP_MN_STATE_TIMEOUT) {
                ESP_LOGI(TAG, "命令等待超时 ⇒ 回到等唤醒");
                wait_cmd = false;
            }
        }
    }

    ESP_LOGW(TAG, "=== 停止听（累计唤醒 %d 次）===", s_wake_cnt);

out:
    /* ★★★ 收尾必须走完：任何一条提前 goto 都要把「麦 / 采样率 / AFE 资源」
     *   还原，否则症状是【之后放音变调】或【再也听不了】（比启动失败更糟）。*/
    if (mn_data) {
        /* 命令词是挂在 mn 模型上的全局链表，destroy 之前必须先 free 掉，
         * 否则下次 start 时 esp_mn_commands_alloc 会报 INVALID_STATE。*/
        esp_mn_commands_free();
        if (mn_iface) mn_iface->destroy(mn_data);
    }
    if (afe   && iface) iface->destroy(afe);
    if (cfg)            afe_config_free(cfg);
    free(raw);
    free(mono);
    app_audio_mic_close();
    if (s_sr_backup != VOICE_SR) {
        if (app_audio_set_sample_rate(s_sr_backup) != ESP_OK)
            ESP_LOGE(TAG, "⚠️ 采样率没能还原到 %d Hz —— 播放可能变调", s_sr_backup);
    }
    s_run  = false;
    s_task = NULL;
    /* ★★ 批 D：到这里【麦已关、采样率已还原】⇒ 音频通路归 44.1 kHz 独占，
     *   现在才执行"要放音"的命令。
     *   ⚠️ 顺序关键：`s_task = NULL` 必须在执行之前 —— 若动作里再调
     *     `app_voice_stop()`（它等 s_task 变 NULL），放后面就是**自等自**的死锁。
     *   ⚠️ 关机动作不返回（整机进 deep sleep）⇒ 后面的 vTaskDelete 不执行，
     *     这是预期行为，不是漏洞。*/
    if (s_pending_act) {
        int act = s_pending_act;
        s_pending_act = 0;
        ESP_LOGW(TAG, "（语音模式已退出，采样率已还原 ⇒ 执行待办动作）");
        voice_exec(act);
    }
    vTaskDelete(NULL);
}

esp_err_t app_voice_start(void)
{
    if (!s_ready) {
        ESP_LOGE(TAG, "语音没就绪（模型没挂上）⇒ 不能开听");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_task != NULL) return ESP_OK;      /* 幂等 */

    s_pending_act = 0;      /* 清掉上一轮可能残留的待办动作 */
    s_run = true;
    if (xTaskCreate(voice_task, "xs_voice", VOICE_TASK_STK, NULL, 5, &s_task)
        != pdPASS) {
        s_run  = false;
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_voice_stop(void)
{
    if (s_task == NULL) return ESP_OK;
    s_run = false;
    /* 等任务自己收尾（它每轮最多阻塞 ~20 ms，收尾含关麦与还原采样率）*/
    for (int i = 0; i < 150 && s_task != NULL; i++) vTaskDelay(pdMS_TO_TICKS(20));
    if (s_task != NULL) ESP_LOGW(TAG, "voice 任务 3 秒未退出（s_task 仍非空）");
    return ESP_OK;
}

bool app_voice_is_running(void) { return s_task != NULL; }
int  app_voice_wake_count(void) { return s_wake_cnt; }
#endif /* AUD_MIC_ENABLE */

