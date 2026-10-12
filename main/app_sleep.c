/*
 * ============================================================
 *  app_sleep.c —— 睡眠定时（到点自动停播）
 * ============================================================
 *  兰兰 10-04：「能不能设置睡眠定时，时间可以自己定」。
 *
 *  ★ 为什么单独开一个文件（而不是塞进 ui_xiandial.c）：
 *    这个模块有【状态 + 掉电保存 + 到点执行】三件事，
 *    而 ui_xiandial.c 已经 4000 行、LVGL 池只剩 26 KB。
 *    把它独立出来 ⇒ 逻辑能单独验，UI 只负责画和收点击。
 *
 *  ★★★ 三个设计决定，每一个都是踩过坑才定的：
 *
 *  ① 【倒计时用 uptime 算，不用系统时间】
 *     系统时间会被 SNTP 改（app_sys.c 里还会 +8h，见那儿的铁律）。
 *     如果用 now - set_at 算倒计时，改一次时间定时就乱了。
 *     uptime 是单调递增的，睡眠期间照走（不依赖 NVS）。
 *
 *  ② 【设置存 NVS，掉电不丢】
 *     睡前设好 30 分钟，半夜拔电/重启，定时不该丢。
 *     ★ 但存的是【设定值】（分钟数），不是【截止时刻】——
 *       存时刻的话重启后会拿旧时刻去比，语义就乱了。
 *
 *  ③ 【到点动作 = 停止播放，不是断电】
 *     兰兰明确选的（问过了）。理由也站得住：
 *       断电之后只能长按电源键开机，等于把设备「关死」了；
 *       停止播放 + 息屏则随时能按一下就回来。
 *     以后要做「真关机」也不难，把 act_stop() 换成关机调用即可，
 *     入口已经单独抽出来了。
 *
 *  用法（ui_xiandial.c）：
 *      app_sleep_init();
 *      app_sleep_set_minutes(30);      // 0 = 取消
 *      app_sleep_remaining_s();        // 剩余秒，0 = 没在跑
 *      app_sleep_is_on();
 *  到点由 app_sleep_tick() 驱动（挂在 1 秒定时器上）。
 * ============================================================ */
#include "app_sleep.h"

#include <stdio.h>
#include <string.h>

#include "app_radio.h"     /* app_radio_stop()：到点要停播 */
#include "app_audio.h"     /* ★ 10-11：渐弱淡出要读写 codec 音量 */

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "XSSLEEP";

#define NVS_NS   "xs_sleep"
#define NVS_KEY  "mins"

static int      s_mins    = 0;        /* 0 = 不定时；>0 = 设定分钟数 */
static int64_t  s_set_at  = 0;        /* 设定时刻的 uptime（微秒） */
static bool     s_fired   = false;    /* 已经到点停过一次，等用户重新设 */

/* ★ 10-11 渐弱淡出（定义在文件末尾，但 set_minutes 要用来「中途取消」）*/
static void fade_abort(bool restore_vol);

static void load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    int32_t v = 0;
    esp_err_t r = nvs_get_i32(h, NVS_KEY, &v);
    nvs_close(h);
    if (r != ESP_OK || v <= 0) return;
    /* ★ 只恢复「设定分钟数」，不恢复倒计时进度。
     *   重启后从头开始数，而不是接着上次数 —— 语义上更符合
     *   「我设了睡 30 分钟」这句话（用户重开机的动机通常就是重新开始）。*/
    s_mins   = (int)v;
    s_set_at = esp_timer_get_time() + (int64_t)s_mins * 60 * 1000000LL;
    ESP_LOGI(TAG, "睡眠定时恢复：%d 分钟（从头开始数）", s_mins);
}

static void save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, NVS_KEY, s_mins);
    nvs_commit(h);
    nvs_close(h);
}

esp_err_t app_sleep_init(void)
{
    s_mins   = 0;
    s_set_at = 0;
    s_fired  = false;
    load();
    return ESP_OK;
}

void app_sleep_set_minutes(int mins)
{
    /* ★ 10-11：无论用户是「取消定时」还是「重新设定」，都要先把
     *   可能正在进行的渐弱中止掉并把音量还原 —— 否则会出现
     *   「用户刚按了取消，音量却还在自己往下掉」这种见鬼的现象。*/
    fade_abort(true);

    if (mins <= 0) {
        bool was = (s_mins > 0);
        s_mins   = 0;
        s_set_at = 0;
        s_fired  = false;
        if (was) save();               /* 只在「从开着改成关」时写 NVS */
        ESP_LOGI(TAG, "睡眠定时已取消");
        return;
    }
    /* ★ 上限 4 小时（240 分）。再长没有实际意义，
     *   而且倒计时显示只有 4 位数字，超了会显示成 9999 分那种鬼样子。*/
    if (mins > 240) mins = 240;
    s_mins   = mins;
    s_set_at = esp_timer_get_time() + (int64_t)mins * 60 * 1000000LL;
    s_fired  = false;
    save();
    ESP_LOGI(TAG, "睡眠定时：%d 分钟后自动停播", mins);
}

int app_sleep_remaining_s(void)
{
    if (s_mins <= 0 || s_fired) return 0;
    int64_t left = s_set_at - esp_timer_get_time();
    if (left <= 0) return 0;
    return (int)(left / 1000000LL);
}

bool app_sleep_is_on(void)
{
    return s_mins > 0 && !s_fired;
}

int app_sleep_minutes(void)
{
    return s_mins;
}

bool app_sleep_fired(void)
{
    return s_fired;
}

/* ============================================================
 *  ★ 10-11 兰兰：「睡眠定时减弱可以做」—— 到点不再硬切，改渐弱淡出。
 *
 *  为什么要做：原来到点直接 app_radio_stop()，喇叭里「啪」一下没了。
 *    快睡着的人被这一下反而惊醒，睡眠定时的意义就打折。
 *
 *  做法：把音量在 3 秒里线性降到 0，再停播。
 *  ★ 三条不能省的（少一条就是给下次留坑）：
 *    ① 音量降到 0 之后【必须还原】。codec 音量是硬件状态，不还原的话
 *       下次开播是静音的 —— 而设置页还显示着原音量，用户会以为机器坏了。
 *       （这正是 10-10 那次「声音 已静音」的同款症状，据因不同、现象一样。）
 *    ② 还原要【等停播完】再做（这里给 400 ms 收尾期），否则等于最后
 *       100 ms 又用大音量把尾巴放出来，白渐弱。
 *    ③ 用户中途取消 / 重新设定，都必须还原音量并中止渐弱（见 set_minutes）。
 *
 *  ★ 为什么用独立的 esp_timer（100 ms）而不是挂在那条 1 秒心跳上：
 *    1 秒只够 3 个台阶，听起来是「哒、哒、哒」三下，不是渐弱。
 *
 *  ★ 静音状态下的处理：app_audio_get_volume() 拿到的是【生效值】
 *    （静音时 apply_audio 已经把它写成 0）⇒ vol0 == 0 时不必渐弱，
 *    直接停播，别去还原成 0 以外的东西（那会意外解除静音）。
 * ============================================================ */
#define FADE_STEP_MS   100
#define FADE_STEPS     30          /* 30 × 100 ms = 3 秒 */
#define FADE_TAIL_MS   400         /* 停播后等这么久才把音量还回去 */

static esp_timer_handle_t s_fade_timer;
static bool s_fading  = false;
static int  s_fade_step = 0;
static int  s_fade_vol0 = 0;

static void fade_abort(bool restore_vol)
{
    if (!s_fading) return;
    s_fading = false;
    if (s_fade_timer) esp_timer_stop(s_fade_timer);
    if (restore_vol) app_audio_set_volume(s_fade_vol0);
    ESP_LOGI(TAG, "渐弱中止（音量还原 %d）", s_fade_vol0);
}

static void fade_cb(void *arg)
{
    (void)arg;
    if (!s_fading) return;

    s_fade_step++;
    if (s_fade_step <= FADE_STEPS) {
        int v = s_fade_vol0 - (s_fade_vol0 * s_fade_step) / FADE_STEPS;
        if (v < 0) v = 0;
        app_audio_set_volume(v);
        if (s_fade_step == FADE_STEPS) {
            ESP_LOGI(TAG, "渐弱到 0 —— 停止播放");
            /* 只调停播，不碰屏幕亮度：夜间页那页本来就暗，
             * 再关背光的话用户按「亮屏键」之前会以为死机了。*/
            app_radio_stop();
        }
        return;
    }

    /* 收尾期：等停播把缓冲里的尾巴放完，再把音量还回去 */
    if ((s_fade_step - FADE_STEPS) * FADE_STEP_MS < FADE_TAIL_MS) return;
    /* ★ 只在「音量还停在我渐弱到的 0」时才还原。
     *   若这 3 秒里用户自己动了音量/解了静音，说明他的意图更新，
     *   我这记还原就是【覆盖用户操作】—— 那才是真 bug。
     *   （guard 用 <=2 而不是 ==0：渐弱的最后一步算出来就是 0，
     *     留 2 的余量防 codec 把 0 夹到别的值。）*/
    if (app_audio_get_volume() <= 2) app_audio_set_volume(s_fade_vol0);
    else ESP_LOGW(TAG, "渐弱期间音量被改过（现 %d）⇒ 不还原成 %d",
                  app_audio_get_volume(), s_fade_vol0);
    s_fading = false;
    if (s_fade_timer) esp_timer_stop(s_fade_timer);
    ESP_LOGI(TAG, "睡眠渐弱结束（音量 %d）", app_audio_get_volume());
}

static void start_fade(void)
{
    s_fade_vol0 = app_audio_get_volume();
    s_fade_step = 0;

    if (s_fade_vol0 <= 0) {          /* 本来就静音：没什么可渐弱 */
        ESP_LOGI(TAG, "音量已是 0 ⇒ 直接停播（不渐弱）");
        s_fading = false;
        app_radio_stop();
        return;
    }

    if (s_fade_timer == NULL) {
        esp_timer_create_args_t a = {
            .callback = fade_cb,
            .name     = "xs_sleep_fade",
        };
        if (esp_timer_create(&a, &s_fade_timer) != ESP_OK) {
            /* 建不出定时器就退化成硬切：宁可啪一下，也得按时停。*/
            ESP_LOGW(TAG, "渐弱定时器建不出 ⇒ 直接停播");
            s_fading = false;
            app_radio_stop();
            return;
        }
    }
    s_fading = true;
    ESP_LOGI(TAG, "到点 —— 从音量 %d 渐弱 %d ms 后停播", s_fade_vol0, FADE_STEPS * FADE_STEP_MS);
    esp_timer_start_periodic(s_fade_timer, FADE_STEP_MS * 1000);
}

bool app_sleep_tick(void)
{
    if (s_mins <= 0 || s_fired) return false;
    int64_t left = s_set_at - esp_timer_get_time();
    if (left > 0) return false;
    s_fired = true;                 /* 只触发一次，重复调用不重复停 */
    start_fade();
    return true;
}
