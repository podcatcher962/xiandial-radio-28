#include "app_settings.h"

#include "app_audio.h"
#include "app_display.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "XSCFG";

#define NVS_NS   "xs_cfg"
#define NVS_KEY  "cfg"

/* ★★★ 默认值：★ 10-11 起按【产品】覆盖（见各家 main/CMakeLists.txt）。
 *
 *  为什么必须分产品：两个产品对这个值的诉求正好相反 ——
 *    · 2.8「拾声」：一开机就该有声的电台机 ⇒ 音量 50、不静音
 *    · 3.5「拾声集（四合一）」：带观音灵签，兰兰 10-08 明确要
 *      「默认声音关闭」（灵签是"你点了它才念"的机器）
 *  把 2.8 的默认值写死进共享文件，3.5 出厂就会开着功放；
 *  反过来写死 3.5 的，兰兰手上那台 2.8 就是「第一次启动声音 0%」——
 *  这就是他 10-11 报的原始现象。⇒ 默认值属于【板相关的常量】，
 *  必须能按产品覆盖（铁律 81：跨板复用模块要先问"这常量是不是板相关的"）。
 *
 *  ★ ver 参与「老存档作废」：改任何一项默认值都要把 DEF_CFG_VER +1，
 *    否则老机器刷完新固件还是旧值（铁律：「配置写了」≠「生效了」）。*/
#ifndef XS_CFG_DEF_VOLUME
#define XS_CFG_DEF_VOLUME   100     /* 不带 override 时 = 3.5 四合一的老行为 */
#endif
#ifndef XS_CFG_DEF_MUTE
#define XS_CFG_DEF_MUTE     1       /* 1 = 开机静音（同上，3.5 的老行为）*/
#endif

#define DEF_CFG_VER    2
#define DEF_VOLUME     XS_CFG_DEF_VOLUME
#define DEF_BACKLIGHT  50
#define DEF_MUTE       (XS_CFG_DEF_MUTE != 0)

#define VOL_MIN  0
#define VOL_MAX  100
#define BL_MIN   20      /* 背光下限：调到 0 等于把屏关了，而那只是"想暗一点" */
#define BL_MAX   100

static xs_cfg_t s_cfg = { DEF_CFG_VER, DEF_VOLUME, DEF_BACKLIGHT, DEF_MUTE };

/* 「本次出声」旁路：不改 mute 标志，只让这一下出声 */
static bool s_boost = false;

/* 延时落盘定时器：连续点 ＋/－ 时不要把 NVS 写爆。
 * ★ NVS 单次写入约几毫秒，但连点十下就是十次擦写；做一个 1.5 秒的
 *   合并窗口，把"一串连续调节"压成"最后一次落盘"。*/
static esp_timer_handle_t s_save_timer = NULL;

/* ---------------- 硬件应用 ---------------- */
static void apply_audio(void)
{
    /* ★ 静音就是 DAC 增益归零，不碰播放器 ——
     *   一碰播放器就会"静音时把正在播的也停了"，语义完全错。*/
    int v = s_boost ? s_cfg.volume : (s_cfg.mute ? 0 : s_cfg.volume);
    app_audio_set_volume(v);
}

static void apply_bl(void)
{
    app_backlight_set(s_cfg.backlight);
}

/* ---------------- 落盘 ---------------- */
void xs_cfg_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    esp_err_t r = nvs_set_blob(h, NVS_KEY, &s_cfg, sizeof(s_cfg));
    if (r == ESP_OK) r = nvs_commit(h);
    nvs_close(h);
    if (r != ESP_OK) ESP_LOGW(TAG, "设置落盘失败: %s", esp_err_to_name(r));
}

static void save_timer_cb(void *arg)
{
    (void)arg;
    xs_cfg_save();
}

static void mark_dirty(void)
{
    if (!s_save_timer) return;
    esp_timer_stop(s_save_timer);                       /* 已在跑则重计时 */
    esp_timer_start_once(s_save_timer, 1500000);        /* 1.5 秒 */
}

/* ---------------- 生命周期 ---------------- */
void xs_cfg_init(void)
{
    bool migrated = false;      /* 存档不可用 → 用默认值，并需要立刻落盘 */

    /* 载入 */
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        xs_cfg_t t;
        size_t len = sizeof(t);
        if (nvs_get_blob(h, NVS_KEY, &t, &len) == ESP_OK && len == sizeof(t)) {
            /* ★ 合法性检查：NVS 里若是损坏/旧版格式的乱值，宁可回默认值，
             *   否则会出现"背光 = 5000"这种把硬件写坏的值。
             * ★ 10-11：ver 不匹配同样是「旧格式」——
             *   默认值改过一轮之后，老存档里的 mute/volume 会盖掉新默认值，
             *   表现就是「改了代码、刷了固件，行为一点没变」
             *   （兰兰手上那台的 NVS 里就存着 mute=true）。
             *   这里一并挡掉，让它回默认值。*/
            if (t.ver == DEF_CFG_VER &&
                t.volume >= VOL_MIN && t.volume <= VOL_MAX &&
                t.backlight >= BL_MIN && t.backlight <= BL_MAX) {
                s_cfg = t;
            } else {
                ESP_LOGW(TAG, "存档不可用（ver=%d 期望 %d，vol=%d bl=%d mute=%d）"
                              "→ 改用默认（音量 %d、%s）",
                         t.ver, DEF_CFG_VER, t.volume, t.backlight, (int)t.mute,
                         DEF_VOLUME, DEF_MUTE ? "静音" : "有声");
                migrated = true;
            }
        }
        nvs_close(h);
    }

    /* 落盘定时器 */
    const esp_timer_create_args_t targs = {
        .callback = save_timer_cb,
        .arg      = NULL,
        .name     = "xscfg",
    };
    esp_timer_create(&targs, &s_save_timer);

    /* 迁移时必须【立刻】以新格式落盘：否则每次开机都走一遍这段，
     * 而用户在设置页调的音量下一次开机又被打回默认 —— 看起来像"存不住"。*/
    if (migrated) xs_cfg_save();

    /* 立即写进硬件 */
    apply_audio();
    apply_bl();

    ESP_LOGI(TAG, "设置载入：音量=%d 背光=%d 声音=%s%s",
             s_cfg.volume, s_cfg.backlight, s_cfg.mute ? "关" : "开",
             migrated ? "（旧存档已作废，回默认）" : "");
}

/* ---------------- 读 ---------------- */
xs_cfg_t xs_cfg_get(void)   { return s_cfg; }
int  xs_cfg_volume(void)    { return s_cfg.volume; }
int  xs_cfg_backlight(void) { return s_cfg.backlight; }
bool xs_cfg_muted(void)     { return s_cfg.mute; }

/* ---------------- 写 ---------------- */
void xs_cfg_set_volume(int v)
{
    if (v < VOL_MIN) v = VOL_MIN;
    if (v > VOL_MAX) v = VOL_MAX;
    s_cfg.volume = v;
    apply_audio();
    mark_dirty();
}

void xs_cfg_set_backlight(int v)
{
    if (v < BL_MIN) v = BL_MIN;
    if (v > BL_MAX) v = BL_MAX;
    s_cfg.backlight = v;
    apply_bl();
    mark_dirty();
}

void xs_cfg_set_mute(bool m)
{
    s_cfg.mute = m;
    apply_audio();
    mark_dirty();
}

void xs_cfg_toggle_mute(void)
{
    xs_cfg_set_mute(!s_cfg.mute);
}

/* ---------------- 本次出声旁路 ---------------- */
void xs_cfg_audio_boost(void)
{
    s_boost = true;
    apply_audio();
}

void xs_cfg_audio_restore(void)
{
    s_boost = false;
    apply_audio();
}
