/*
 * es28 Suite —— 外壳层（2.8 版：★ 单产品）
 * ============================================================
 *  ★★★ 与 3.5 的 ui_shell.c 是【同名不同物】：
 *    3.5 那份是「多产品主菜单」（拾声电台 / 灵签 / 天气 / 股票 四宫格），
 *    本机只有一个产品 —— 拾声电台 ⇒ 这个外壳的职责只剩两件事：
 *      ① 开机直接进电台（没有主菜单这一层）；
 *      ② 给【串口调试命令通道】补齐它引用的符号（app_stlist.c 的
 *         app_open / app_home / radio / stk_kline / ui_demo speak …）。
 *
 *  ★ 为什么要在 2.8 侧实现这些符号，而不是把 app_stlist.c 里的命令删掉：
 *    那条通道里有一个【本机最缺的能力】—— lq_shot（整帧截图走串口到 PC）。
 *    这块板没有硬件 UART、我也看不到屏，2.8 又是 240x320 的新尺寸，
 *    界面适配全靠"截图 → PC 上看"这个闭环。把它删掉等于眼睛被蒙上。
 *    ★ 但 st_shot() 本身【不依赖任何 UI 符号】（只用 app_display_snapshot()），
 *      真正卡住链接的是同一条链里的 ui_demo / app_open / stk_kline 那几条。
 *      所以这里把那些符号做成"本机没有这个产品"的明确实现 ——
 *      ★ 不是静默 no-op：每条都打 W 级日志说清楚"本机没有它"，
 *        免得日后有人敲了命令、看到"没反应"以为通道坏了（铁律 72：假成功最贵）。
 *
 *  ★ 命名沿用 ui_shell.h 的契约（声明在 3.5 的 ui_shell.h 里，两边签名一致）
 *    ⇒ 头文件不用改、也不用从同步清单里摘。
 */
#include "ui_shell.h"

#include "ui_radio_v.h"          /* 2.8 唯一的产品 + 它的调试口 */
#include "ui_lingqian.h"         /* 下面几个只是为了让签名对得上（本机没有这些产品）*/
#include "ui_stock.h"
#include "ui_weather.h"
#include "ui_xiandial.h"         /* ui_reload_stations 的声明 */

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ES28SHELL";

/* 本机没有的产品：统一一句人话，别让它看起来像"命令没生效" */
#define NO_SUCH_APP(name)                                                      \
    ESP_LOGW(TAG, "%s：本机是【2.8 单产品固件】，只装了拾声电台，没有这个产品", name)

/* ============================================================
 *  外壳三件套
 * ============================================================ */
void ui_shell_init(void)
{
    /* 3.5：建四宫格主菜单。2.8：没有中间这一层，直接进电台。 */
    ESP_LOGI(TAG, "单产品外壳：开机直接进【拾声电台】");
    ui_radio_v_init();
}

void ui_shell_back(void)
{
    /* 3.5：切回主菜单屏并拆掉当前产品。
     * 2.8：没有主菜单可回 ⇒ 等价于"回到电台的列表视图"
     *      （从播放页/分类页/本地页按返回，就是这个语义）。 */
    ui_radio_v_demo_view(0);
    ESP_LOGI(TAG, "app_home → 电台列表视图（本机没有主菜单）");
}

void ui_shell_launch(int idx)
{
    if (idx == 0) {
        ui_radio_v_demo_view(0);
        ESP_LOGI(TAG, "app_open 0 → 拾声电台");
        return;
    }
    NO_SUCH_APP("app_open");
}

/* ============================================================
 *  调试命令要的符号 —— 本机没有的那些
 * ============================================================ */
void ui_shell_demo_cfg(void)        { NO_SUCH_APP("ui_demo cfg（设置面板）"); }
void ui_shell_demo_cfg_scroll(int y) { (void)y; NO_SUCH_APP("ui_demo cfg 滚动"); }
void ui_shell_demo_cfg_close(void)  { NO_SUCH_APP("ui_demo cfgclose"); }
void ui_shell_demo_prov(void)       { NO_SUCH_APP("ui_demo prov（配网引导层）"); }
void ui_shell_demo_prov_close(void) { NO_SUCH_APP("ui_demo provclose"); }
void ui_shell_demo_power(void)      { NO_SUCH_APP("ui_demo power（关机键）"); }
void ui_shell_demo_dump(void)       { NO_SUCH_APP("ui_demo dump"); }

void ui_lingqian_demo(const char *page, int pick)
{
    (void)page; (void)pick;
    NO_SUCH_APP("lq_demo（灵签）");
}

void ui_stock_open_kline(int idx)
{
    (void)idx;
    NO_SUCH_APP("stk_kline（股票）");
}

void ui_stock_demo_add(void)   { NO_SUCH_APP("stk_demo（股票）"); }
void ui_weather_demo_speak(void) { NO_SUCH_APP("ui_demo speak（天气语音）"); }

/* ============================================================
 *  台单热重载（串口 st_reload 命令 → 重建界面）
 * ============================================================
 *  3.5：外壳通知首页/网格重建。
 *  2.8：只有电台一个产品 ⇒ 直接让电台把筛选复位、按新台单重刷列表。
 *  ⚠️ 必须持 LVGL 锁（调用方 app_stlist.c 的串口任务是持锁进来的）。 */
void ui_reload_stations(void)
{
    ESP_LOGI(TAG, "台单热重载 → 电台列表复位筛选并重刷");
    ui_radio_v_demo_filter(-1, -1);
}
