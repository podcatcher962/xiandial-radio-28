/*
 * es28 Suite —— 主程序（2.8″ · 拾声完整版 · 横版 UI）
 *
 * ★ 这一版是【正式固件】，不是朝向自测版：
 *   开机 → 初始化服务层 → 进 3.5 横版拾声界面（ui_xiandial.c，按 320x240 重排）。
 *
 * ★ 两块板的分工（10-10 兰兰定）：
 *   3.5″（xs35，白壳）＝ 开发板，产品可清空重来；
 *   2.8″（es28，橙壳）＝ 产品线（配套壳 + 52mm 2W 喇叭）⇒ 本工程就是它的固件。
 *
 * ★ 朝向【已定案】（10-10 真机，见 app_pins.h）：
 *   屏 swap1 mx0 my0 ／ 触摸 swap1 mx1 my1；以「USB 口在右」为正向
 *   （兰兰照配套壳确认过：壳的 USB 开孔在右侧）。
 *
 * ★ 服务层初始化序列照抄 3.5 的 xiandial_main.c，只把 UI 入口
 *   ui_shell_init()（竖版四合一）换成 ui_init()（横版拾声）。
 *   ⚠️ 顺序有两条硬约束（3.5 踩过，别动）：
 *     · app_sd_mount() 必须在 app_st_load() 之前（台单要读卡）；
 *     · app_st_load() 必须在 app_fav_init() 之前（收藏按台名匹配台单）。
 */

#include <stdio.h>
#include <stdlib.h>          /* atoi：串口命令 `page <n>` / `dump <n>` 解析 */
#include <string.h>

#include "app_display.h"
#include "app_pins.h"

#include "app_audio.h"
#include "app_fav.h"
#include "app_hist.h"
#include "app_resume.h"
#include "app_pwr.h"
#include "app_radio.h"
#include "esp_timer.h"          /* esp_timer_get_time：语音自动任务的启动退避用 */
#include "app_sd.h"
#include "app_settings.h"
#include "app_sleep.h"
#include "app_stlist.h"
#include "app_sys.h"
#include "app_version.h"
#include "app_voice.h"          /* ★ 10-11：语音（ESP-SR）*/
#include "ui_xiandial.h"          /* ★ 横版拾声界面（ui_init / ui_reload_stations）*/

#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"

static const char *TAG = "ES28";

/* ============================================================
 *  极简串口截图通道（2.8 专用）
 * ============================================================
 *  为什么自己写一份、不直接用 3.5 的 app_st_start_serial_cmd()：
 *    那条命令通道里挂着 ui_demo（要调 ui_shell_* 那套竖版 UI 符号），
 *    而 2.8 编的是横版 ui_xiandial.c、没有 ui_shell.c ⇒ 一调就链接失败。
 *    这里只实现真正需要的那一条命令：lq_shot（截图）。
 *    好处：我自己能看屏 —— 改布局不用拿兰兰当验证工具。
 *
 *  协议与 3.5 完全一致（PC 侧复用 lingqian-radio/tools/_lq_shot.py）：
 *    发  "lq_shot\r\n"
 *    ←   "\r\nSHOT <w> <h> <nbytes> <crc32>\r\n"
 *    ←   nbytes 个原始字节（LVGL 行主序 RGB565，小端）
 *    ←   "\r\nSHOT_END\r\n"
 *  ⚠️ 发送期间【绝不能插日志】：控制台与这帧二进制共用同一条 USB-JTAG，
 *     插一行日志整帧就错位（颜色全乱、字节数却看不出来）。
 *  ⚠️ 分块必须 ≤48 B：USB-JTAG 的 TX 环默认只有 64 B，
 *     usb_serial_jtag_write_bytes() 装不下时【直接返回 0】（不阻塞不报错），
 *     3.5 那边第一版按 2048 发，整帧实发 0 字节。
 * ============================================================ */

/* 标准 CRC32（与 PC 侧 zlib.crc32 一致）*/
static uint32_t xs_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(c & 1u)));
        }
    }
    return c ^ 0xFFFFFFFFu;
}

static void xs_shot_send(void)
{
    const int W = LCD_H_RES, H = LCD_V_RES;
    const int nbytes = W * H * 2;
    static uint8_t *buf = NULL;

    if (buf == NULL) {
        buf = heap_caps_malloc((size_t)nbytes, MALLOC_CAP_SPIRAM);
        if (buf == NULL) {
            ESP_LOGE(TAG, "lq_shot: PSRAM 分配 %d B 失败", nbytes);
            return;
        }
    }
    /* ★ 取快照前先强制同步重绘一帧（理由同 3.5 的 st_shot）：
     *   否则"截到哪一帧"取决于 LVGL 最近写的是哪块缓冲，会截到上一屏。*/
    bool ok = false;
    if (lvgl_port_lock(0)) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(NULL);
        ok = app_display_snapshot(buf);
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "lq_shot: 拿不到 LVGL 锁");
        return;
    }
    if (!ok) {
        ESP_LOGE(TAG, "lq_shot: 还没出过帧");
        return;
    }

    uint32_t crc = xs_crc32(buf, (size_t)nbytes);
    char hdr[80];
    int n = snprintf(hdr, sizeof(hdr), "\r\nSHOT %d %d %d %08x\r\n",
                     W, H, nbytes, (unsigned)crc);

    /* ★★★ 10-10 补的一层闸门：**发帧期间把日志全关掉**。
     *   协议注释里写了「发送期间绝不能插日志」，但那只约束了【我自己】；
     *   别的任务照样在打 —— 最典型的是 spec_timer_cb（100 ms）和
     *   player_refresh（1 s），它们只往控制台写，却和这一帧抢同一条 USB-JTAG。
     *   后果非常隐蔽：帧头/字节数/结尾全都对得上，
     *   只有【内容整体错位】⇒ 表现是 PC 侧 CRC 校验失败，
     *   或者更糟 —— CRC 恰好撞对、截出一张颜色诡异的图。
     *   实测：第 2 页（播放）与第 5 页（本地音频）必挂，其余页正常，
     *   因为只有这两页挂得起 100 ms/1 s 的周期日志。
     *   ⇒ 用 esp_log_level_set("*", NONE) 把全局日志静默，
     *     发完再恢复（本工程默认 INFO）。日志不会丢：只是这几百毫秒不产生。*/
    esp_log_level_set("*", ESP_LOG_NONE);

    usb_serial_jtag_write_bytes(hdr, (size_t)n, pdMS_TO_TICKS(2000));

    int off = 0, stalls = 0;
    while (off < nbytes) {
        int chunk = nbytes - off;
        if (chunk > 48) chunk = 48;
        int w = usb_serial_jtag_write_bytes(buf + off, (size_t)chunk,
                                            pdMS_TO_TICKS(200));
        if (w <= 0) {
            if (++stalls >= 40) break;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        stalls = 0;
        off += w;
        uint8_t junk[64];
        usb_serial_jtag_read_bytes(junk, sizeof(junk), 0);
    }
    usb_serial_jtag_write_bytes("\r\nSHOT_END\r\n", 13, pdMS_TO_TICKS(2000));

    /* ★★★ 10-10 第二个坑（比日志静默更隐蔽）：**两条 TX 通道会互相插队**。
     *
     *   现象：截图帧的载荷里、甚至 "\r\nSHOT_END\r\n" 这 13 个字节【中间】，
     *         被塞进了几个字节的日志碎片。比如实测抓到：
     *           \r\nSHOT lq__s END\r\n  ← "lq__s" 就是 "lq_shot:" 的前 5 个字节
     *        字节数看着对、CRC 就是不过；第 2 页（播放）必挂。
     *
     *   根因：这不是「没关日志」，是【两个写者 + 一个异步排空】：
     *     · 本函数用 usb_serial_jtag_write_bytes() —— 它把数据放进
     *       **驱动自己的 TX 环形缓冲**，然后由 TX-done 中断异步往 FIFO 里搬；
     *       调用返回 ≠ 数据已经出线。
     *     · 而 ESP_LOGx 走的是【控制台】那条路（esp_usb_console），
     *       它是**直写 USB-JTAG FIFO** 的，完全不经过上面那个环。
     *   ⇒ 只要调用返回后立刻日志，日志的字节就会插到还没搬完的帧前面。
     *
     *   修法：发完 SHOT_END 之后【先等环形缓冲彻底排空】再恢复日志/打日志。
     *   120 ms 的依据：153600 B ÷ 48 B 一块、USB 全速，尾部最多几十毫秒；
     *   再留一倍余量。代价是每次截图多 0.12 秒，可以忽略。
     *   ⚠️ 别改成「把日志删掉就完事」—— 下一个人加一行日志就复现，
     *     必须留这个显式的排空等待。*/
    vTaskDelay(pdMS_TO_TICKS(120));

    esp_log_level_set("*", ESP_LOG_INFO);        /* ★ 恢复日志（见上）*/

    ESP_LOGW(TAG, "lq_shot: 整帧 %d B 已发（实发 %d）CRC %08x", nbytes, off, (unsigned)crc);
}

/* 串口命令任务：lq_shot / shot（截图）、page <n>（切页）、dump <n>（越界体检）*/
static void xs_cmd_task(void *arg)
{
    (void)arg;
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        cfg.rx_buffer_size = 512;
        cfg.tx_buffer_size = 512;
        if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) {
            ESP_LOGE(TAG, "usb_serial_jtag 驱动安装失败 —— 串口命令不可用");
            vTaskDelete(NULL);
        }
    }
    /* ★ 10-11：48 → 320。新增的 `play <完整路径>` 要能容纳卡里的中文长路径
     *   （"/sdcard/小宇宙播客 64K/城市节目/第01期.mp3" 这种 UTF-8 好几十字节）。
     *   48 字节连命令带路径根本装不下，会静默截断成一个不存在的路径。
     *   本任务栈 10240 B，320 字节的局部数组无压力。*/
    char line[320];
    int  len = 0;
    for (;;) {
        uint8_t ch;
        if (usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100)) != 1) {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (len > 0) {
                line[len] = '\0';
                if (strcmp(line, "lq_shot") == 0 || strcmp(line, "shot") == 0) {
                    xs_shot_send();
                } else if (strncmp(line, "page", 4) == 0 || strncmp(line, "dump", 4) == 0) {
                    /* ★★ 10-10 新增：`page <n>` 切页、`dump <n>` 列越界对象。
                     *   为什么必须持 LVGL 锁：goto_page() 会在里面建/删
                     *   lv_obj、挂定时器；本任务与 lvgl_port 的渲染任务并发，
                     *   不持锁就是典型的「偶发崩在 lv_draw_add_task」。
                     *   `dump` 不加锁会读到半改完的布局，报的越界是假的。*/
                    bool is_dump = (line[0] == 'd');
                    int  n = -1;
                    const char *p = line + 4;
                    while (*p == ' ') p++;
                    if (*p >= '0' && *p <= '9') n = atoi(p);
                    if (lvgl_port_lock(3000)) {
                        if (is_dump) ui_debug_dump(n);
                        else         ui_debug_goto(n);
                        lvgl_port_unlock();
                    } else {
                        ESP_LOGW(TAG, "%s: 拿不到 LVGL 锁", is_dump ? "dump" : "page");
                    }
                } else if (strncmp(line, "tcal", 4) == 0) {
                    /* ★★ 10-10 新增：`tcal` 开触摸自检层、`tcal 0` / `tcal off` 关。
                     *   为什么要持 LVGL 锁：它要建/删 lv_obj。*/
                    const char *p = line + 4;
                    while (*p == ' ') p++;
                    int on = !(*p == '0' || *p == 'o');   /* "tcal 0" / "tcal off" = 关 */
                    if (lvgl_port_lock(3000)) {
                        ui_debug_tcal(on);
                        lvgl_port_unlock();
                    } else {
                        ESP_LOGW(TAG, "tcal: 拿不到 LVGL 锁");
                    }
                } else if (strncmp(line, "tmap", 4) == 0) {
                    /* ★★ 10-10 新增：`tmap <swap> <mx> <my>` 运行时切触摸档位。
                     *   触摸朝向是 8 档，本命令让它【一次烧录全试完】——
                     *   配合 `tcal` 看读数就能定档，不必每档重编重烧。
                     *   例：tmap 1 0 0 ／ tmap 1 1 0 ／ tmap 0 1 1 ……
                     *   ⚠️ 只是运行期覆盖，重启回 app_pins.h 的编译期值。*/
                    int a = -1, b = -1, c = -1;
                    if (sscanf(line + 4, "%d %d %d", &a, &b, &c) == 3) {
                        app_display_set_orient_touch(a != 0, b != 0, c != 0);
                    } else {
                        ESP_LOGW(TAG, "用法: tmap <swap> <mx> <my>   例: tmap 1 0 0");
                    }
                } else if (strncmp(line, "snd", 3) == 0) {
                    /* ★★ 10-10 新增：`snd` 查、`snd on` / `snd off` 设【声音总开关】。
                     *   为什么必须加这条：
                     *   app_settings.c 的 DEF_MUTE = true（开机静音），而
                     *   apply_audio() 是 `mute ? 0 : volume` ⇒ 静音时 DAC 增益恒 0。
                     *   2.8 原来界面上【没有】任何地方能解除静音 ⇒
                     *   「没声音」是静音位还是硬件坏，光看屏分不出来，只能靠它。
                     *   判据：这里报的「实际增益」必须等于「音量设定」才算非静音。*/
                    const char *p = line + 3;
                    while (*p == ' ') p++;
                    if (*p == '\0') {
                        ESP_LOGW(TAG, "snd: 当前 %s（音量设定 %d / 实际增益 %d）",
                                 xs_cfg_muted() ? "已静音" : "有声",
                                 xs_cfg_volume(), app_audio_get_volume());
                    } else {
                        bool off = (p[0] == '0') || (p[0] == 'o' && p[1] == 'f');
                        xs_cfg_set_mute(off);              /* 低电平 PA 之外的第二道门 */
                        if (!off && xs_cfg_volume() < 30) xs_cfg_set_volume(60);
                        ESP_LOGW(TAG, "snd -> %s（音量设定 %d / 实际增益 %d）",
                                 xs_cfg_muted() ? "已静音" : "有声",
                                 xs_cfg_volume(), app_audio_get_volume());
                        if (!off) app_audio_beep_async();  /* 立刻给一声，验链路 */
                    }
                } else if (strncmp(line, "vol", 3) == 0) {
                    /* `vol <0-100>`：设音量并顺手解除静音（与播放页滑块同一语义）*/
                    int v = atoi(line + 3);
                    if (xs_cfg_muted()) xs_cfg_set_mute(false);
                    xs_cfg_set_volume(v);
                    ESP_LOGW(TAG, "vol -> 设定 %d / 实际增益 %d",
                             xs_cfg_volume(), app_audio_get_volume());
                } else if (strncmp(line, "ls", 2) == 0) {
                    /* ★ 10-11 新增（调试用）：`ls [目录]` 列一条目录里的条目。
                     *   为什么需要：验证"本地音频续播"必须真的点一首卡里的文件，
                     *   而触摸只能靠人按 —— 有了 ls + play，整条链路可以在
                     *   串口侧自动验完（先看有什么文件，再 play 它）。
                     *   默认列 /sdcard 的前 12 条。*/
                    const char *p = line + 2;
                    while (*p == ' ') p++;
                    const char *dir = (*p) ? p : "/sdcard";
                    static app_sd_entry_t ent[12];
                    int n = app_sd_list(dir, ent, 12);
                    ESP_LOGW(TAG, "ls %s -> %d 条", dir, n);
                    for (int i = 0; i < n && i < 12; i++) {
                        ESP_LOGW(TAG, "  %s%s  %u B", ent[i].is_dir ? "[目录] " : "       ",
                                 ent[i].name, (unsigned)ent[i].size);
                    }
                } else if (strncmp(line, "play ", 5) == 0) {
                    /* ★ 10-11 新增（调试用）：`play <完整路径>` 直接放一个本地文件。
                     *   走的是和界面完全相同的入口（app_radio_play_file），
                     *   所以"续播"该生效就会生效 —— 串口里看有没有
                     *   「续播：这条上次听到 N s」和「续播就位」两行就知道。
                     * ★★ 10-11 扩展：也认 http(s):// ⇒ `play <URL>` 直接放网络流。
                     *   为什么必须加这条：验证「坏流保护」（铁律 145）需要一个
                     *   **能连上、但解不出帧**的流。实有样本＝
                     *   一条 AAC+ 编码的流（Content-Type: audio/aacp
                     *   —— 这边解码器按 MP3 解析 ⇒
                     *   parser 搜 100 KB 找不到帧 ⇒ decode ret=-7 ⇒ 空转）。
                     *   界面里点到那个台太随机，靠串口才能**稳定重现**。
                     *   ⚠️ 名字就用 URL 本身：界面会把它当"台名"显示，
                     *     这样一眼能认出"现在放的是调试流"，不会和真台混。*/
                    const char *p = line + 5;
                    while (*p == ' ') p++;
                    esp_err_t r;
                    if (strncmp(p, "http://", 7) == 0 ||
                        strncmp(p, "https://", 8) == 0) {
                        r = app_radio_play_url(p, p);
                    } else {
                        r = app_radio_play_file(p);
                    }
                    ESP_LOGW(TAG, "play %s -> %s", p, esp_err_to_name(r));
                } else if (strncmp(line, "wl", 2) == 0) {
                    /* ★ 10-11 新增（调试用）：`wl on|off` 开关语音唤醒。
                     *   开之前先停播 —— AFE 要 16 kHz，而播放是 44.1 kHz，
                     *   I2S 的 TX/RX 共用 BCLK/WS ⇒ 两者不能同时存在。
                     *   （这也正是兰兰定的「暂停/停止时才听」策略。）*/
                    const char *p = line + 2;
                    while (*p == ' ') p++;
                    if (strncmp(p, "on", 2) == 0) {
                        app_radio_stop();
                        vTaskDelay(pdMS_TO_TICKS(500));   /* 等播放任务退干净 */
                        app_voice_start();
                    } else if (strncmp(p, "off", 3) == 0) {
                        app_voice_stop();
                    }
                    ESP_LOGW(TAG, "wl: running=%d 唤醒累计=%d",
                             (int)app_voice_is_running(), app_voice_wake_count());
                } else if (strncmp(line, "micg", 4) == 0) {
                    /* ★ 10-11 新增（调试用）：`micg <dB>` 运行时改麦增益（0~42）。
                     *   为什么要有它：定档要靠真机扫几档比 RMS，
                     *   若每改一档都重编+重烧，一轮要十几分钟。
                     *   有了它一次烧录就能扫完全部档位。*/
                    const char *p = line + 4;
                    while (*p == ' ') p++;
                    int db = (*p) ? atoi(p) : app_audio_get_mic_gain();
                    if (*p) app_audio_set_mic_gain(db);
                    ESP_LOGW(TAG, "mic 增益 = %d dB", app_audio_get_mic_gain());
                } else if (strncmp(line, "mic", 3) == 0) {
                    /* ★ 10-11 新增（调试用）：`mic [毫秒]` 做麦克风电平自检。
                     *   先停播 —— 麦要独占 codec 与 I2S 时钟（TX/RX 共用 BCLK/WS），
                     *   一边放音一边改采样率会把播放弄乱。
                     *   详见 app_voice.h 里 app_voice_mic_test 的说明。*/
                    int ms = 1000;
                    const char *p = line + 3;
                    while (*p == ' ') p++;
                    if (*p) ms = atoi(p);
                    app_radio_stop();
                    /* 等播放任务退干净：它退出时会关解码器、还内存，
                     * 而且 radio_start 靠 s_task==NULL 判断前一个任务结束
                     * ⇒ 这里给足时间，别在它还没退时就去重开 codec。*/
                    vTaskDelay(pdMS_TO_TICKS(400));
                    app_voice_mic_test(ms);
                } else if (strncmp(line, "rsm", 3) == 0) {
                    /* ★ 10-11 新增（调试用）：`rsm` 打印续播表。
                     *   验证"进度有没有存进去/有没有被清掉"，比听声音直接得多。*/
                    int c = app_resume_count();
                    ESP_LOGW(TAG, "rsm: %d 条（最近优先）", c);
                    for (int i = 0; i < c; i++) {
                        ESP_LOGW(TAG, "  [%d] %d s  %s", i,
                                 app_resume_pos_at(i), app_resume_path_at(i));
                    }
                } else if (strncmp(line, "stop", 4) == 0) {
                    /* ★ 10-11 新增（调试用）：`stop` 停播 —— 停下会走 done: 那条
                     *   收尾路径，正好验证"听到一半停下 ⇒ 进度保住了"。*/
                    app_radio_stop();
                    ESP_LOGW(TAG, "stop 已下发");
                } else if (strncmp(line, "sleeptest", 9) == 0) {
                    /* ★ 10-11 临时探针（验完删）：`sleeptest <分>` 设睡眠定时。
                     *   为什么要它：渐弱淡出这段代码【只有到点才会跑】，
                     *   而界面上的最小档是 30 分钟 —— 不可能坐着等半小时
                     *   去确认「音量最后有没有还原」。
                     *   这条命令能设 1 分钟，等 65 秒就能把整条路走完。*/
                    int m = atoi(line + 9);
                    app_sleep_set_minutes(m);
                    ESP_LOGW(TAG, "sleeptest %d -> 设定 %d 分 / 剩余 %d s",
                             m, app_sleep_minutes(), app_sleep_remaining_s());
                } else if (strncmp(line, "mode", 4) == 0) {
                    /* ★ 10-11 临时探针（验完删）：`mode <0|1|2>` 设 SD 播放模式。
                     *   0=连播 1=单曲循环 2=随机。不带参数就打印当前档。
                     *   为什么要它：模式的 UI 入口在播放页第八行，只能手点；
                     *   而"放完自动下一首"要等一整首放完（2 小时），
                     *   必须能脚本化地切模式 + 跳到结尾。*/
                    char *p = line + 4;
                    while (*p == ' ') p++;
                    if (*p == '\0') {
                        ESP_LOGW(TAG, "mode: 当前 %d", app_radio_play_mode());
                    } else {
                        app_radio_set_play_mode(atoi(p));
                        ESP_LOGW(TAG, "mode -> %d", app_radio_play_mode());
                    }
                } else if (strncmp(line, "seek ", 5) == 0) {
                    /* ★ 10-11 临时探针（验完删）：`seek <秒>` 跳到第 N 秒。
                     *   配合 mode 用：seek 到距结尾 20 秒处，等 25 秒就能
                     *   看到"正常放完 → 自动续下一首"整条路，
                     *   不必真的坐着听两个多小时。*/
                    int s = atoi(line + 5);
                    esp_err_t r = app_radio_seek(s);
                    ESP_LOGW(TAG, "seek %d -> %s（总长 %d）", s,
                             esp_err_to_name(r), app_radio_total_s());
                } else {
                    ESP_LOGW(TAG, "未知命令: %s（认 lq_shot / page <n> / dump <n> / "
                                  "tcal / tmap / snd [on|off] / vol <n> / "
                                  "ls [目录] / play <路径> / rsm / stop / sleeptest <分> / "
                                  "mode <0|1|2> / seek <秒>）", line);
                }
                len = 0;
            }
        } else if (len < (int)sizeof(line) - 1) {
            line[len++] = (char)ch;
        }
    }
}

/* ============================================================
 *  ★★★ 10-11：语音自动启停（兰兰定的策略：「只在没播放时听」）
 * ============================================================
 *  为什么必须自动，而不能像调试时那样手动发 `wl on`：
 *    麦克风（ES8311 ADC）与喇叭（DAC）在 S3 上共用一组 I2S，
 *    TX/RX 共享 BCLK/WS ⇒ **16 kHz 听 与 44.1 kHz 播无法并存**。
 *    「起播 → 停听」这半边已经由 app_radio.c 的 APP_RADIO_PLAY_HOOK 堵住了；
 *    本任务负责另一半：**播放结束之后，把听重新打开**。
 *
 *  策略：
 *    · 空闲（没在播、也没暂停）⇒ 常开听，随时可以喊「Hi，喵喵」
 *    · 一开始播（含暂停态：那时播放任务还在、I2S 仍被占）⇒ 立刻让路
 *
 *  ⚠️ 用 500 ms 轮询而不是事件回调：播放入口散在好几处（UI 点台 / 语音命令 /
 *     自动续播 / 单曲循环…），逐个挂钩必漏；而"在不在播"只有一个真源
 *     （app_radio_is_playing）。起播那一路已经由钩子做到零延迟，
 *     这里只兜"停播后恢复听"，0.5 s 延迟无害。
 *  ⚠️ 开机先等 2 秒：让 UI 与网络初始化走完再动音频时钟。*/
static void voice_auto_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(2000));

    int     fail           = 0;   /* 连续启动失败次数（退避用）*/
    int64_t last_try_ms    = 0;   /* 上次尝试启动的时刻（防止失败后疯狂重启）*/

    for (;;) {
        /* ★ 暂停时 app_radio_is_playing() 仍为 true（播放任务还在、I2S 仍被占）
         *   ⇒ 暂停期间同样算"忙"，不抢麦克风。*/
        bool busy = app_radio_is_playing();
        bool want = (!busy) && app_voice_ready();
        /* ★★★ 判据取【实际状态】，不用本任务的记忆变量。
         *   原因：起播那一路是 app_radio.c 的钩子【先】把语音停掉的（比本任务早
         *   500 ms），若这里还记着"我们在听"，两者必然对不上；更要命的是，
         *   一旦别处（串口 `wl on/off`、语音任务自己出错退出）动过状态，
         *   记忆变量就会让本任务**永远不再重新开听**。
         *   `app_voice_is_running()` 才是唯一真源。*/
        bool now  = app_voice_is_running();

        if (want && !now) {
            /* 10 秒内只尝试一次 —— 否则"启动成功但任务立刻失败退出"会变成
             * 每 0.5 s 重启一次的循环（日志刷屏、还白耗内存）。*/
            int64_t t = esp_timer_get_time() / 1000;
            if (t - last_try_ms > 10000) {
                last_try_ms = t;
                if (app_voice_start() == ESP_OK) {
                    ESP_LOGI(TAG, "空闲 ⇒ 语音自动开听（喊「Hi，喵喵」）");
                    fail = 0;
                } else {
                    fail++;
                }
            }
        } else if (!want && now) {
            app_voice_stop();
            ESP_LOGW(TAG, "开始播放 ⇒ 语音自动让路（麦与喇叭共用 I2S）");
            fail = 0;
        } else {
            fail = 0;             /* 状态已一致，无事可做 */
        }

        /* 一直起不来就别每 0.5 s 试一次，退到 5 s。*/
        vTaskDelay(pdMS_TO_TICKS(fail > 3 ? 5000 : 500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "============ 拾声 · 2.8″ (es28 · 横版 UI) ============");
    ESP_LOGI(TAG, "board: LCDWiki ES3C28P  ESP32-S3 N16R8  320x240 landscape");
    ESP_LOGI(TAG, "固件版本: %s (%s)  %s", XS_FW_VERSION, XS_FW_DATE, XS_FW_NOTE);
    ESP_LOGI(TAG, "朝向: 屏 swap1 mx0 my0 ／ 触摸 swap1 mx1 my1（USB 口在右为正）");

    /* ★★★ 10-10：让「大一点的分配」优先走 PSRAM，救内部 SRAM。必须放在
     *   任何 LVGL / 网络初始化【之前】。
     *
     *   背景：LVGL 从内建池改成 CLIB malloc 后，lv_obj 改走系统堆。
     *   ESP-IDF 默认 CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384 ⇒
     *   **≤16 KB 的分配一律强制放内部 SRAM**，而 lv_obj 只有 ~300 B，
     *   ⇒ 横版 UI 的 259 格网格 ≈ 180 KB 全塞进内部 SRAM。
     *   实测后果（10-10 的无限重启真因，不是 LVGL 池的问题）：
     *     ES28: boot done: heap free=7.79 MB  internal free=**1755 B**  psram free=7.79 MB
     *     lwip_arch: sys_init: failed to init lwip protect mutex  →  abort() → 重启
     *   —— PSRAM 还剩 7.79 MB 却分不出一个互斥锁，纯粹是"内部 RAM 被独占"造成的。
     *
     *   为什么用运行时开关、不改 sdkconfig：
     *     CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 属 SPIRAM 系 Kconfig，
     *     改它会触发【整个工程全量重编】（实测 32 分钟）；
     *     本函数是它的运行时版本 —— 改一行，重编 2 分钟。
     *   阈值取 0 的教训（10-10 踩过一轮）：
     *     一开始设 128（想"小分配留内部、大分配去 PSRAM"），结果 internal free
     *     仍只剩 1987 B、照样崩 —— 因为 LVGL 的小额分配**量极大**：
     *     每个 lv_label 的文本 buffer（台名二三十字节）、lv_obj 的 spec_attr、
     *     每次渲染的 draw task 结构……它们全都 ≤128 B，于是一样落进内部 RAM。
     *     ⇒ 只要还剩一类常用分配留在内部，259 格网格就能把它吃干。
     *     设 0 ⇒ 所有 malloc 一律先试 PSRAM，内部 SRAM 基本只留给
     *     明确点名 MALLOC_CAP_INTERNAL/DMA 的调用（LCD/音频驱动那些）。
     *   ⚠️ 明确用 heap_caps_malloc(MALLOC_CAP_DMA) 的缓冲（LCD/音频）不受本设置影响。*/
    heap_caps_malloc_extmem_enable(0);
    ESP_LOGI(TAG, "内存策略: 所有 malloc 优先 PSRAM（heap_caps_malloc_extmem_enable(0)）");

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    /* 1. 屏 + 触摸 + LVGL */
    ESP_ERROR_CHECK(app_display_init());

    /* 2. 音频（放在界面之前 ⇒ 开机提示音一定已就绪）。失败不致命。*/
    esp_err_t ar = app_audio_init();
    if (ar != ESP_OK) {
        ESP_LOGE(TAG, "audio init failed (%s) - UI keeps running", esp_err_to_name(ar));
    }

    /* 2.4 全局设置（音量/背光/静音，存 NVS）—— 必须在 audio 之后（要写 ES8311 增益） */
    xs_cfg_init();

    /* 2.45 SD 卡：必须【提前到台单加载之前】 */
    static app_sd_entry_t s_ents[24];
    esp_err_t sd = app_sd_mount();
    if (sd == ESP_OK) {
        int n = app_sd_list("/sdcard", s_ents, 24);
        ESP_LOGI(TAG, "SD root listing: %d entries", n);
    } else {
        ESP_LOGW(TAG, "SD not available (%s) - 台单用内置", esp_err_to_name(sd));
    }

    /* 2.5 播放器：注册解码器 */
    if (app_radio_init() != ESP_OK) {
        ESP_LOGW(TAG, "radio init failed - local playback unavailable");
    }

    /* 2.55 载入台单（读 /sdcard/stations.tsv，没有则回落内置） */
    app_st_src_t stsrc = app_st_load();
    app_st_dump_info();
    ESP_LOGI(TAG, "台单来源: %s（%d 条）",
             stsrc == APP_ST_SRC_TSV ? "SD 卡 stations.tsv" : "内置（自用版）",
             app_st_count());

    /* 2.6 收藏 / 历史：必须在 app_st_load() 之后（按台名匹配台单） */
    if (app_fav_init() != ESP_OK) {
        ESP_LOGW(TAG, "fav init failed - 收藏页会是空的");
    }
    if (app_hist_init() != ESP_OK) {
        ESP_LOGW(TAG, "hist init failed - 首页不会顶历史台");
    }
    /* ★ 10-11 续播：本地音频"上次听到第几秒"的表（独立 NVS 命名空间）。
     *   不依赖台单，放哪都行；跟 hist 一起初始化只是为了读起来顺。*/
    if (app_resume_init() != ESP_OK) {
        ESP_LOGW(TAG, "resume init failed - 本地音频不会续播");
    }

    /* 2.7 睡眠定时 */
    app_sleep_init();

    /* 2.7b ★ 10-11：语音（ESP-SR）。
     *   ⚠️ 失败【不】致命 —— 语音坏了不影响听电台，界面照跑。
     *      打 W 而不是 E：没烧 model 分区是很常见的一次性状态
     *      （只烧 app 就会这样），不值得让整机报错。*/
    if (app_voice_init() != ESP_OK) {
        ESP_LOGW(TAG, "语音不可用（model 分区没烧？用 _flash_es28.py voice）");
    }

    /* 2.8 电源 / 电量 */
    app_pwr_init();

    /* 2.9 板载 RGB 状态灯（IO42） */
    app_led_init();

    /* 3. 界面：开机直接进【横版拾声】（不是四合一主菜单） */
    if (lvgl_port_lock(0)) {
        ui_init();
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "lvgl lock failed");
    }

    ESP_LOGI(TAG, "boot done: heap free=%u  internal free=%u  psram free=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* 4. SD 内容日志 */
    if (sd == ESP_OK) {
        for (int i = 0; i < 24; i++) {
            if (s_ents[i].name[0] == '\0') break;
            ESP_LOGI(TAG, "   [%d] %s%s  %u B", i,
                     s_ents[i].name, s_ents[i].is_dir ? "  <DIR>" : "",
                     (unsigned)s_ents[i].size);
        }
        ESP_LOGI(TAG, "SD audio count (recursive, <=5 levels) = %d",
                 app_sd_count_audio("/sdcard"));
    }

    /* 5. 网络 */
    esp_err_t nr = app_net_init();
    if (nr != ESP_OK) {
        ESP_LOGW(TAG, "net init failed (%s)", esp_err_to_name(nr));
    }

    /* 6. WiFi 账密：优先 NVS（用户机上输的），没有才回退卡里 wifi.txt */
    if (nr == ESP_OK) {
        if (!app_net_connect_saved() && sd == ESP_OK) {
            esp_err_t wr = app_net_connect_from_file("/sdcard/wifi.txt");
            if (wr != ESP_OK) {
                ESP_LOGW(TAG, "wifi.txt connect: %s", esp_err_to_name(wr));
                ESP_LOGW(TAG, "两处都没有可用账密 —— 到「状态」页点「WiFi 设置」在机器上输");
            }
        }
    }

    ESP_LOGI(TAG, "after all init: internal free=%u  psram free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* 7. 调试钩子：串口截图命令（lq_shot）。栈给到 10240 —— 铁律 64：
     *    「能建界面」的串口命令任务是爆栈重灾区，且爆栈表现为"命令没生效"。*/
    xTaskCreate(xs_cmd_task, "xs_cmd", 10240, NULL, 4, NULL);

    /* 8. ★★★ 10-11：语音自动启停（空闲听 / 一播就让路；策略由兰兰定）。
     *   栈 6 KB —— 它只是调度器，真正的 AFE 大缓冲在 PSRAM。
     *   优先级 3：低于串口(4)与播放(8)，抢不到 CPU 也没关系，晚 0.5 s 轮一次而已。
     *   ⚠️ 用 app_voice_ready() 兜一层：模型没烧（只烧了 app）时别把这个任务
     *     建起来空转 —— 那会每 5 s 打一次"起不来"。*/
    if (app_voice_ready()) {
        xTaskCreate(voice_auto_task, "xs_vauto", 6144, NULL, 3, NULL);
    } else {
        ESP_LOGW(TAG, "语音不可用 ⇒ 不启自动听（model 分区没烧？用 _flash_es28.py voice）");
    }
}
