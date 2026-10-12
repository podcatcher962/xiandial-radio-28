/*
 * 拾声 (XianDial) 硬件版 —— 界面
 * 设计语言「声场 Soundfield」· 横屏 480x320 · 触摸为主力
 *   底 #0B0E12 / 面板 #151A21 / 高亮 #1E252E / 主色拾声绿 #34D399
 *   次色电波青 #38BDF8 / 状态琥珀 #FBBF24 / 文本 #F2F5F8 / 次要 #7C8894
 *   夜间暖白 #C8BFA8 / 分隔 #2A323C
 *
 * 迭代记录
 *   10-02 第五次：把所有「该能点」的控件真的接上交互（此前只有导航栏接了）。
 *   10-03 第七次：
 *     ① 第 3 页「播放」从「假 FM 调谐盘」改成真正的【本地播放界面】：
 *        刻度环变成进度环，环心显示曲名 + 已播/总时长，右侧显示格式与状态，
 *        三个键变成 上一个/暂停/下一个（真的换曲），底部加「本地列表」回 SD 页。
 *        —— 兰兰反馈「按了本地音频文件没有对应的播放界面」，就是缺这一页。
 *     ② 点 SD 页里的音频文件后【自动跳到第 3 页】，不用手动点导航。
 *     ③ 状态页网络行显示 IP / SSID，并加「连 wifi.txt」按钮（随时重连）。
 *   10-03 第八次 —— ★ 让网络电台真的出声：
 *     ① 首页/发现页的【假台单换成真台单】（net_stations.c，119 台国内台，
 *        全部无 TLS 的 http，裸请求就 200）。点一行 = 真的开播，不是弹提示音。
 *     ② 播放页支持【直播态】：直播没有总时长，环变成「电平表」，
 *        时长行显示 LIVE，格式行显示「网络电台 · 64 kbps」。
 *     ③ 上一个/下一个键：在放本地文件就换曲，在放电台就换台。
 *     ④ 台名用【台名专用字库】xs_font_st14/18 —— 台名里有「圳 涿 碚 禺」
 *        这类常用字库没有的字，不单独做一份就是满屏方块（见 gen_fonts_st.py）。
 *
 *   10-03 第九次（本次）—— 兰兰上手后的第一批反馈：
 *     ① 顶栏右上不再是「WiFi 已连」几个字，改成【图标】：
 *        绿色 WiFi 图标 = 已连，暗的 = 没连上；旁边一个【内存卡图标】+「TF 29G」。
 *        （图标直接用 LVGL 内置 montserrat_14 里的 LV_SYMBOL_WIFI / _SD_CARD，
 *          不自绘矢量 —— 省事而且清晰。）
 *     ② 电台库从 119 台扩到 687 台，【栏目 10 个 / 省份 31 个】：
 *        栏目与省份不再靠台名猜，直接照抄网页版台单每条台的 g（栏目）/ c（省份）。
 *     ③ 首页「栏目」条、【发现页「地区」条】都改成【横向可滑】的：
 *        芯片放不下就往右滑，不再只有新闻/音乐/交通三个。
 *     ④ 首页/发现页/分类页的电台列表全部改成【双列 + 上下滑动】，
 *        并撤掉「换一批」——上下滑比翻批更顺手（兰兰原话）。
 *     ⑤ 点首页的栏目芯片 → 【全屏分类页】（只有名称、双列、右上角返回键）。
 *        屏幕小，全屏页一屏能放 14 个台，比挤在首页里强。
 *     ⑥ 收藏电台：首页那张卡原来点下去只弹提示音（兰兰说「点击无效」），
 *        现在进【全屏收藏页】；并且收藏【机器上能自己设】——
 *        播放页有「☆收藏」键、网格里长按也能收藏，存 NVS 掉电不丢（app_fav.c）。
 *
 *   10-03 第十次（本次）—— 兰兰上手后的第二批反馈：
 *     ① 顶栏去掉重复的容量显示：原来「● 就绪 · TF 29G」那格和 SD 卡图标右边
 *        的「29 G」是同一件事，屏幕上出现两次。SD 图标右边保留唯一一份，
 *        「● 就绪」那格换成【当前时间】（兰兰：「如果不重要可以替换为当前时间」）。
 *     ② 播放页加【返回键】，做到「从哪里来回到哪里」：
 *        每层网格记住自己来自哪一页（首页 / 发现 / 全屏分类页），
 *        播放页的返回键就回那一页；全屏分类页再点它自己的返回，回发现/首页。
 *        ★ 不做成「统一的播放页固定返回首页」—— 兰兰原话：
 *          「要返回刚刚在发现页选择的地区栏」。
 *     ③ 播放页加【动态频谱】：真频谱（app_radio 里 Hann 窗 + 12 点 Goertzel），
 *        不是静态示意。首页那张卡上的假柱子也换成真的。
 *     ④ 台单换成【自用版】：国内 686 台 + 港澳台/海外 201 台（_gen_stations_all.py）。
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_audio.h"
#include "app_display.h"
#include "app_fav.h"
#include "app_hist.h"
#include "app_resume.h"      /* ★ 10-11：本地音频续播（历史页列出 + 点击续播）*/
#include "app_pins.h"
#include "app_prov.h"
#include "app_pwr.h"         /* ★ 10-04：电量图标 + 关机（app_pwr.c 驱动）*/
#include "app_radio.h"
#include "app_sd.h"
#include "app_sleep.h"      /* 睡眠定时（10-04 新增） */
#include "app_sys.h"
#include "app_settings.h"    /* ★ 10-10：声音总开关（静音）——2.8 之前一次都没引，
                              *   结果 mute 默认 true 却没有任何界面能解除 ⇒
                              *   兰兰真机报「没声音」，且连开机提示音都是哑的。*/
#include "app_version.h"
#include "net_stations.h"
#include "ui_xiandial.h"

#include "esp_heap_caps.h"   /* ★ 10-11：SD 目录缓冲放 PSRAM（app_sd_entry_t 268 B/条）*/
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_stlist.h"

static const char *TAG = "XSUI";

/* ---------- 颜色 ---------- */
/* ★★ 10-10 兰兰定：**保持深色底**（理由：浅色太亮、夜里刺眼，电池供电也更耗电），
 *   但要求「色彩元素丰富些，蓝绿红都要有」。⇒ 底色/文字色一律不动，只补三色分配：
 *     红 C_RED   → 品牌（logo、拾声、顶栏下划线）、导航选中、收藏、关机、低电、频谱第三段
 *     蓝 C_CYAN  → 时钟、正在播放、首页栏目、上一个/下一个、横滑箭头、署名
 *     绿 C_GREEN → 播放按钮、频谱、SD 卡就绪、提示音、配网
 *   ⚠️ 配套硬修正：app_display.c 的 esp_lcd_panel_invert_color 原来是错的(false)，
 *      屏上一直在显示本表的【补色】（所以兰兰看到的是"白底+品红logo+蓝点"）。
 *      改成 true 之后：代码里的色 == 屏上的色，截图与实物也终于一致。
 *   ⚠️ 这三色都当【文字色】在白/深底上用过，改浅前先想对比度。*/
#define C_BG      lv_color_hex(0x0B0E12)
#define C_PANEL   lv_color_hex(0x151A21)
#define C_HILITE  lv_color_hex(0x1E252E)
#define C_GREEN   lv_color_hex(0x34D399)
#define C_CYAN    lv_color_hex(0x38BDF8)
#define C_AMBER   lv_color_hex(0xFBBF24)
#define C_TEXT    lv_color_hex(0xF2F5F8)
#define C_MUTED   lv_color_hex(0x7C8894)
#define C_WARM    lv_color_hex(0xC8BFA8)
#define C_LINE    lv_color_hex(0x2A323C)
#define C_RED     lv_color_hex(0xF87171)   /* ★ 红：品牌/选中/警示 */
#define C_ONDARK  lv_color_hex(0x06231A)   /* 绿底上的深字 */

/* ---------- 字库（由 firmware/gen_fonts.py 用 lv_font_conv 生成）---------- */
/* ★★★ 10-05 换字库 + 砍字号档（v1.36）
 *   旧：simhei/Deng（专有，中易＋方正）11/14/18/38/52 五档 ＋ cjk14 ＝ 10.85 MB
 *   新：Ark Pixel 12px（OFL-1.1）＋思源黑体补字（OFL-1.1）12/24 两档 ＝ 4.02 MB
 *
 *   为什么只剩两档：Ark Pixel 是【像素字体】，按整数倍放大才不虚。
 *   12→24 是干净的 2 倍；旧的 11/14/18/38/52 全是非整数缩放，
 *   像素风会被拉糊，与 v2.0「赛博荧光」像素风设计稿背道而驰。
 *   而且 52px 那 4.2 MB 只用来画开机页「拾声」两个字，一个台名都用不到。
 *
 *   ⚠️ 迁移映射（旧名已不再生成，漏改的地方编译器会抓出来，这是好事）：
 *       xs_font_11 / xs_font_14 → xs_font_12
 *       xs_font_18 / xs_font_38 → xs_font_24
 *       xs_font_52               → 【删】改用 24
 *       xs_font_cjk14            → xs_font_cjk12
 *       xs_font_st14 / st18      → xs_font_st12 / st24
 *
 *   授权：Ark Pixel 与思源黑体都是 SIL OFL-1.1，允许嵌入固件、允许子集化。
 *   Ark 的 OFL.txt 无 Reserved Font Name 条款 ⇒ 子集化不触发改名义务。
 *   评估全文见 xiandial-hw/拾声v2.0-字库版权风险评估.md
 *
 *★★★ 10-05 真机反馈 → 【加回 16px 中间档】（v1.37）
 *   v1.36 只留 12/24 两档，真机烧完兰兰报了一串「字体太大、换行、互相遮挡」：
 *   正在播放台名 / 音乐库 / 最近听过 / 地区列表标题 / 封面页署名。
 *   根因【不是布局写错，而是两档之间没有过渡档】：
 *   旧 18px 一次性映射到 24px（放大 33%），而这些 label 的容器宽高
 *   还是按 18px 时代算的 ⇒ 文字撑出容器、压住下一行。
 *   16px 实测体积 0.45 MB（12px 是 0.31、24px 是 0.96），
 *   app 分区还有 59% 余量，换得回所有「太大」的位置，划算。
 *   ⚠️ 16 对12 是 1.333倍非整数缩放，边缘有抗锯齿、锐利度不如 12/24。
 *     所以只用在【短固定文案】上（「音乐库」「最近听过」这类 3~4 字），
 *     长文与长台名仍走 12px + LV_LABEL_LONG_DOT，绝不靠放大解决。 */
/* ★★ 2.8 适配（10-10）：横版原档 24/16/12（Ark Pixel，旧档）→ 2.8 现有档 22/18/14。
 *   ⚠️ 全文件的字号引用【只有这里 + 下面 F_* 宏】两处，改完即全文件生效。*/
extern const lv_font_t xs_font_22;
extern const lv_font_t xs_font_18;
extern const lv_font_t xs_font_14;

/* ★ SD 页专用「大字符集」字库：GB2312 符号区 + 一级汉字（约 4400 字，另带 ASCII）。
 *   其余页面继续用小字库以省 Flash —— 那些页面的文案是编译期写死的，
 *   扫源码生成的字库足够。但 SD 页要显示【运行时才知道的文件名】，
 *   卡里叫「小宇宙播客」源码里从没出现过 ⇒ 小字库画出来全是方块（10-03 兰兰反馈）。
 *   所以 SD 页整体切到这个字库。
 *
 * ★★ 10-10 2.8 正式启用这一档：
 *   真机实测 SD 页文件名出方块 ——「□谈□ Vol. 07」「吃里□外」「□花渐欲」，
 *   缺的正是「扒」「乱」这类常用字（它们不在主字库那 2037 个界面字里）。
 *   已用 ESP32-Platform/_gen_fonts_es28.py cjk 烘出
 *     14px / 2bpp / 4565 字（源码字 ∪ GB2312 符号区+一级汉字）/ 1696 KB。
 *   ⚠️ 原来那行「#define xs_font_cjk12 xs_font_14」已删除 ——
 *      它把 F_SD 悄悄指回主字库，而主字库只含界面文案字，
 *      动态文件名必然出方块（这就是方块的真因，不是 FATFS 一件事）。
 *   ⚠️ 改 SD 页或换字体后重跑：python _gen_fonts_es28.py cjk（铁律 77）。*/
extern const lv_font_t xs_font_cjk12;

/* ★ 台名专用字库（firmware/gen_fonts_st.py 生成）：
  *   台名、栏目名、省份名都是运行时才知道显示哪一条的，共 772 个汉字，
  *   里面有「圳涿 澧 碚 禺 廣 華 藝 國」这些常用字库（GB2312 一级）没有的字 ——
  *   不单独出一份，三列网格里那些台名就是一排方块。
  *   覆盖率已实测：台单 753 字 + GB2312 4437 字，Ark＋思源【零缺口】。
  *   ⚠️ 自用版与发布版的这两个符号【同名】，字形来源不同（见 CMakeLists 注释）：
  *     自用版  fonts/xs_font_st12.c   ← 按 net_stations.c 的用字烘焙
  *     发布版  fonts/xs_font_pub_st12.c ← 按 GB2312 一级 3755 字烘焙
  *   ⇒ 这里的 extern 和下面两个 #define 【两个版本都不用改】。*/
#define xs_font_st12  xs_font_st16  /* ★ 2.8 无 st12 档：台名/栏目/省份统一用 st16 */
extern const lv_font_t xs_font_st16;
/* ★★ st24 已于 10-06 删除，不是忘了编译：
  *   10-05 换字号档时把它从 26px 映射到 24px 留着，但【没有任何地方用它】——
  *   全 ui_xiandial.c 检索只有这里的 extern 和下面的 #define 两处提到，
  *   一个 label 都没挂上去。它占 1.2 MB 编译时间和仓库体积，链接器虽会丢弃，
  *   但每次全量编译都要为它编一遍。
  *   真要用 24px 台名，走 F_H2（xs_font_24）—— 但它只含 625 个界面文案字，
  *   台名会出方块，见下面 F_H2 的警告。*/

#define F_TITLE  (&xs_font_22)   /* 大标题（v2.0 收紧，原 52px 已废）*/
#define F_BIG    (&xs_font_22)
#define F_MID    (&xs_font_18)   /* ★ 10-05 新增：3~6 字短标题专用。
                                   「音乐库」「最近听过」「封面署名」这类
                                   字少但用 24px 会撑出容器、用 12px 又太弱，
                                   16px 正好。⚠️ 不要用在长文/长台名上，
                                   16px 对 12px 是 1.333倍缩放，长文会显毛糙。*/
#define F_H2     (&xs_font_22)   /* 大号数字 / 台名（历史遗留，慎用）*/
#define F_BODY   (&xs_font_14)   /* 正文、列表行 */
#define F_TINY   (&xs_font_14)   /* 顶栏时钟、按钮小字 */
#define F_SD     (&xs_font_cjk12)/* SD 页（运行时文件名）*/
/* ★★★ 10-11 新增：**【任何可能显示用户文本】的 label 都必须用它**。
 *   用户文本 = 卡里的文件名/曲名、以及"界面词 + 动态内容"拼接出来的行。
 *   理由（真机实案）：F_ST18 指向的 xs_font_st16 是【台名专用子集，只有 871 码点】，
 *   它连「滑」「显示」「还没有」这种界面词都没有 —— 2.8 上用 st16 画曲名，
 *   一进 MP3 播放页就是方块（10-11 兰兰报的正是这个）。
 *   而 xs_font_cjk12 覆盖 4661 码点（GB2312 一级+二级），_font_coverage.py 全绿。
 *   ⚠️ 名字里的 "12" 是历史遗留：它的真实字号是 **14px**
 *      （见 ESP32-Platform/_gen_fonts_es28.py 的 G.CJK_SIZE = 14）。
 *   ⚠️ 但它**不含**台名里那些生僻字（「圳 涿 碚 禺」）⇒
 *      台名继续用 F_ST16，别反过来。判据：st16 = 台名；F_USER = 用户文本/界面词。*/
#define F_USER   (&xs_font_cjk12)
/* 台名/栏目/省份三处都用它（含 GB2312 里没有的那些字）*/
#define F_ST12   (&xs_font_st12)
#define F_ST16   (&xs_font_st16)  /* ★ 10-05 新增：播放页大台名 */
#define F_ST14   (&xs_font_st12)   /* 兼容旧名 → 12px */
#define F_ST18   (&xs_font_st16)   /* ★ 10-05：旧名改指 16px（v1.36 指向 24px 太大）*/
/* ★ 台名【永远不要用 F_H2/F_BIG】—— 那是主字库，只有 625 个界面文案字，
 *   台单有 753 个字，常用台名里的一些字就不在里面 ⇒ 真机上方块。*/

/* ★ 顶栏图标字体：LVGL 自带的 montserrat_14 里含 LV_SYMBOL_* 那一段
 *   （码位 U+F000~U+F8A3，WIFI=U+F1EB、SD_CARD=U+F7C2）。
 *   sdkconfig 里 CONFIG_LV_FONT_MONTSERRAT_14=y，所以不用自己画矢量图。
 *   比自绘三条弧 + 一个方块省事得多，而且边缘清晰。*/
#define F_ICON   (&lv_font_montserrat_14)

/* ---------- 布局常量 ---------- */
/* ★★ 2.8 适配（10-10）：3.5 横屏 480x320 → 2.8 横屏 320x240。
 *   宽比 0.667、高比 0.75 ⇒ 顶栏 38→28、导航栏 56→40。
 *   CONT_W = 320-40 = 280 ｜ CONT_H = 240-28 = 212。*/
#define TOPBAR_H   28
#define RAIL_W     40
#define CONT_X     RAIL_W
#define CONT_Y     TOPBAR_H
#define CONT_W     (LCD_H_RES - RAIL_W)     /* 424 */
#define CONT_H     (LCD_V_RES - TOPBAR_H)   /* 282 */

#define PAGES_N    8      /* 0 首页 1 发现 2 播放 3 夜间 4 状态 5 本地音频
                           * 6 全屏电台列表（首页栏目 / 发现页地区 / 收藏 共用）
                           * 7 WiFi 连接设置（第二十二次新增，状态页进去）*/
#define NAV_N      5      /* 左侧导航栏只有前 5 个（第 6/7 页是全屏覆盖页，靠返回键）*/

/* ---------- 静态句柄 ---------- */
static lv_obj_t *s_splash_scr;
static lv_obj_t *s_main_scr;
static lv_obj_t *s_pages[PAGES_N];
static lv_obj_t *s_nav_btns[NAV_N];
static lv_obj_t *s_nav_marks[NAV_N];
/* ★ 第二十二次：导航栏文字句柄。改绿色之后，切页必须同步改亮度，
 *   否则「首页」永远是亮绿、「状态」永远是暗的。*/
static lv_obj_t *s_nav_labels[NAV_N];
static int       s_cur_page = 0;

static lv_obj_t *s_lbl_station;   /* 首页：大卡台名 */
static lv_obj_t *s_lbl_source;    /* 首页：大卡来源/码率 */
static lv_obj_t *s_lbl_clock;     /* 夜间：时钟 */
static lv_obj_t *s_lbl_date;      /* 夜间：日期（10-04 从写死改成真时间） */
static lv_obj_t *s_lbl_playstate; /* 状态：播放状态（★ 首页/发现页/夜间页共用，见下） */
/* ★★ 10-04：s_lbl_playstate 是【跨页共享】的 —— 首页的「播放中 · 台名」
 *   （第 1752 行）和播放页的「播放中/已暂停」（第 2753 行）都往它写。
 *   所以夜间页必须用自己的句柄，否则两个页面的定时器互相覆盖文字
 *   （这正是兰兰看到「夜间页的当前播放显示不对」的原因之一）。*/
static lv_obj_t *s_lbl_night_state; /* 夜间：状态（10-04 独立，专用） */
/* ★★ v1.35：夜间页改成【进页才建、离开就删】（同第 8 页 WiFi 设置）。
 *   真机崩在这：兰兰报「点击睡眠页就重启」。
 *   根因 = 夜间页最重（40 根装饰竖条 + 8 颗胶囊×2 + 时钟 + 滑块 ≈ 62 个对象），
 *   一显示就要为每个对象分配 lv_draw_task_t，而池只剩 13.7 KB：
 *       lv_draw_add_task(): new_task = lv_malloc_zeroed(...);   → 返 NULL
 *                          LV_ASSERT_MALLOC(new_task);         → release 下【空操作】
 *                          new_task->area = *coords;           → 往 NULL 写 → EXCVADDR=0x08
 *   ⚠️ 和第 8 页一样的病、同一个坑位（lv_draw_add_task），只是这次更重。
 *   ⚠️ 钩子必须挂在 goto_page 里，不能挂在返回键上（见那里注释）。*/
static bool s_night_built;
/* ★★ 10-11：s_lbl_playname（第九行的小曲名）【已删】——
 *   兰兰：「播放页底部有当前电台状态用不到，底部改为音量标签和+-」。
 *   第九行整行让给音量控件。曲名在第二行已有一个更大的（且同样可点进「本列表」）
 *   ⇒ 按本工程一贯原则「320×240 上不该有两处同样的入口」，这次收成一个。
 *   ⚠️ 删的是【重复的那一处】，ev_open_own_list 的入口没丢（见 s_np_name）。*/
static lv_obj_t *s_lbl_playbtn;   /* 播放页：中间键文字 */
static lv_obj_t *s_lbl_favbtn;    /* 播放页：☆/★ 收藏键 */
static lv_obj_t *s_btn_speed;     /* 播放页：倍速键（第二十二次）*/
static lv_obj_t *s_lbl_speed;     /* 播放页：倍速键上的「倍速 1.2X」*/
/* ★★ 10-11：SD 播放模式 —— 由【一颗循环键】改成【三颗独立键】
 *   （兰兰：「连播，随机，单曲可以有位置做按键」）。
 *   为什么改：循环键要「点几下才知道现在是哪个」，而且从当前态走到目标态
 *   平均要点 1.5 下；三颗键一眼看全、一下到位。
 *   ⚠️ 数组顺序必须与 app_radio 的模式枚举一致（0=连播 1=单曲 2=随机）。*/
static lv_obj_t *s_btn_mode3[3];
static lv_obj_t *s_lbl_mode3[3];
static lv_obj_t *s_playbtn_lbl;   /* 首页：暂停圆钮上的文字 */
static lv_obj_t *s_lbl_topstat;   /* 顶栏右侧时钟（HH:MM）*/
static lv_obj_t *s_ic_wifi;       /* 顶栏：WiFi 图标（LV_SYMBOL_WIFI）*/
static lv_obj_t *s_ic_sd;         /* 顶栏：内存卡图标（LV_SYMBOL_SD_CARD）*/
static lv_obj_t *s_ic_batt;       /* 顶栏：电池图标（10-04；只有图标无数字）*/
static int       s_batt_shown_pct = -2;   /* 上次显示的电量档，-2=还没刷过 */
static lv_obj_t *s_lbl_sdinfo;    /* 顶栏：内存卡容量文字「29 G」——唯一一处*/

/* ★ 播放页的返回键回哪一页（兰兰 10-03：「要做到从哪里来能够回到哪里」）。
 *   点首页的台 = 0；点发现页的台 = 1；点全屏分类页/收藏页的台 = 6。
 *   首页大卡进播放页时也记 0。SD 页点文件进播放页记 5。*/
static int       s_play_from = 0;

static lv_timer_t *s_splash_timer;   /* 停留计时（3.5 s）*/
static lv_timer_t *s_spl_timer;      /* 动画推送（60 ms）*/
/* ★ 10-04 第二十九次：splash 只允许撤一次。
 *   声明必须放在这儿（splash_build 在 1800 行就要把它清零，
 *   而定义在文件后段的 splash_done_cb 里）。*/
static bool s_splash_done;
static lv_timer_t *s_stat_timer;
static lv_timer_t *s_clock_timer;
static lv_timer_t *s_date_timer;    /* 夜间页日期（10-04 新增） */
static lv_timer_t *s_nstate_timer;  /* 夜间页播放状态（10-04 新增） */
static lv_timer_t *s_sleep_timer;   /* 睡眠定时倒计时（10-04 新增） */
static lv_timer_t *s_spec_timer;   /* 100 ms：真频谱专用，见 spec_timer_cb */
static lv_timer_t *s_seek_timer;   /* 20 ms：进度条拖动专用，见 seek_timer_cb */
/* ★ 第二十四次：s_wifi_timer（WiFi 页删/建待办）已删 —— 屏幕键盘去掉了。*/

static bool s_playing = true;

/* SD「全卡音频 N 首」缓存。
 * ★ 为什么必须缓存：app_sd_count_audio() 是递归 + 逐条 stat，
 *   在 32 GB 塞满音频的卡上要扫几千条目录项。而 ui_sd_page_refresh()
 *   每次切子目录/返回上一级都会调用 —— 不缓存的话每点一下就卡几秒。
 *   策略：只在「从首页进入 SD 页」时置脏，其余刷新一律用缓存值。*/
static bool s_sd_count_dirty = true;
static int  s_sd_audio_total = -1;

/* ---- 第 3 页「本地播放」的句柄 ----
 * ★ player_refresh() 定义在文件后半段，但 SD 页点击（sd_row_clicked）
 *   要用到它，所以先在这里前向声明，避免把函数搬来搬去。*/
static void player_refresh(void);

/* 同理：播放页的真频谱重画与返回键文案（定义在文件后半段那一节）*/
static void spec_refresh(void);
static const char *play_from_name(void);

/* 播放页返回键上的文字（定义在 page_play_build 那一节，
 *   但 player_refresh 要在切页时同步它 —— 所以这里先声明）*/
static lv_obj_t *s_lbl_playback;

/* 当前时刻（分钟）。定义在文件末尾那一节（顶栏与夜间页共用）。*/
static int now_minutes(void);

/* ★ 按【UTF-8 字符】截断（定义在下面 SD 页那一节）。
 *   首页台名要截断，所以先在这里声明一份，别把函数搬来搬去。*/
static void utf8_copy_n(char *dst, size_t dstsz, const char *src, int max_chars);

/* 播放页左半。10-03 第十一次：200x200 的进度环删掉了（兰兰「圆圈在，频谱就展不开」），
 * 现在是「曲名 + 大频谱 + 底部 6 px 细进度条」。*/
/* ★★ 进度条：可拖动（本地音频），直播时自动变成只读底线
 * ============================================================
 *  ★ 兰兰 10-03 第十四次：「播放页左下方有个横条（应该是进度条吧）
 *    确实是只能显示进度不能拖动，本地音乐播放页也是一样的不能拖动」
 *
 *  我上一版把它做成了【两个纯 box】（底槽 + 绿色前景改宽度），
 *  那是「只显示」不是「可拖」—— 兰兰说得对。
 *
 *  为什么不用 lv_slider 现成的：
 *    lv_slider 自带 style（轨道 + 指示器 + 旋钮）三套，每个都是 lv_obj，
 *    一个就吃掉几 KB；池只剩 21 KB。lv_arc 更贵（10-03 第十一次已经因为它
 *    把频谱挤成 20 px 而删掉了）。所以还是 box 拼，但补上【触摸事件】。
 *
 *  拖动时的三步：
 *    ① 按下 → 记下当时的总时长，按 x 坐标算出目标秒，显示预览时间
 *    ② 拖动中 → 实时改前景宽度 + 刷新预览时间（不 seek，松手才 seek）
 *    ③ 松手 → app_radio_seek(目标秒)，后端 fseek + 重建解码器（约 200 ms）
 *
 *  直播（is_url）不能跳：这时把触摸回调直接不挂，进度条只显示满格绿线。
 */
/* 播放页「版面账」★★★ 2.8 重排（10-10，兰兰：「看不见的按键、超出窗口的按键
 *   你来重新排」）。
 *
 *   3.5 那一版是【左右两栏】：左半 x=4..248 显示区、右半 x=256..420 控制区。
 *   搬到 280 宽的内容区后，右半从 x=256 起 ⇒ 整栏（含返回键、收藏、倍速、
 *   频谱键、三个小键、音量滑块）**一个像素都不在屏内** —— 这就是
 *   「看不见的按键」的真正来源，不是坐标差半格。
 *   ⇒ 2.8 改成【上下九行、每行整宽】：
 *     y=2..24    传输行：返回 / 收藏 / 上一个 / 播放 / 下一个（5×50，缝 4）
 *     y=26..44   曲名（F_ST18 16px，★ 点它 = 看「本列表」）
 *     y=45..60   时间行（左 132）+ 状态（右 136）
 *     y=62..110  频谱（272×48，纯柱状，不可点）
 *     y=113..126 来源（左 132）+ 进度提示（右 136）
 *     y=127..143 进度条（272 宽 16 高 · 热区 124..146 · 可拖）
 *     y=147..171 4 个跳分键（4×65，缝 4 = 272，高 24）
 *     y=174..190 倍速 / 本地 / 音量文字 / 滑块
 *     y=192..209 曲名（左，可点）+ 一行提示（右）
 *   ★ 「4 个跳分键 + 倍速 + 本地」原来在右栏一列，2.8 放不下 ⇒ 倍速与本地
 *     挪到音量那一行（见 page_play_build 尾段）。
 * ★ 下面这几个值被 seek_sec_at()（算「手指点的是不是进度条」）用到，
 *   所以必须是编译期常量，不能是文件后段的 static 变量。*/
/* ★★★ 2.8 三改（10-10）：兰兰「频谱只保留一种柱状就可以了」
 *   ⇒ 模式切换整套拿掉，标题带还给频谱：
 *     二改时是 SPEC_Y 77 / SPEC_H 32（上面 61..76 留给「点频谱换图 · 柱」标题条）
 *     现在    SPEC_Y 62 / SPEC_H 48（62..110）
 *   上面第三行占到 y=60、下面 s_np_meta 在 y=113 ⇒ 两头都不挤，
 *   频谱从 32 px 长到 48 px，柱形看得更清楚。
 *   → 详情见 spec_build() / spec_refresh()。*/
#define SPEC_H  48
#define SPEC_Y  62
/* ★★★ 10-03 第十八次重排播放页左半（兰兰：「快进10有了，但10秒太短，
 *   需要分钟级别的，2分钟，10分钟」＋「本地拖动还是不行」）。
 *
 *   第十六次版面：[退10][条 168][进10] 一行 —— 跳转键只能塞两个。
 *   第十八次改成【两行】，跳转键从 2 个变 4 个，条恢复整宽：
 *     上行 y=186..206：4 个跳转键，每个 58×20，缝 4
 *              4 → 62 → 120 → 178 → 236..240（4×58 + 3×4 = 244 ✓）
 *              文案：退2  退10  进10  进2
 *     下行 y=210..238：整宽进度条 244 宽 × 28 高
 *              ★ 28 px 厚：20 px 那版兰兰还是说按不准
 *     热区 y=200..250（50 高），盖满 244 宽 —— 上面是键，下面才是条，不冲突
 *
 *   为什么给到 10 分钟：本地音乐常见 3~8 分钟，±10 s 一次只能跳过一句词；
 *   ±2 min 才是「跳过一整段」的量级。四个键覆盖「微调 / 大段」两档。
 */
/* ★★★ 2026-10-04 第二十次重排（兰兰：「快进10分钟有效，但经常要触碰几次才有效，
 *   2分钟无反应可能不灵敏」）。
 *
 *   ❶ 跳转键高度 20 → 30（SEEK_STEP_H）：一整行 30 高，按得准。
 *   ❷ 事件从 LV_EVENT_CLICKED 改成 **LV_EVENT_PRESSED**（按下即跳）：
 *      CLICKED 要「按下 + 在同一对象内松手」，中途只要 LVGL 判定成滚动就作废。
 *      兰兰「要碰几次」就是这个。改成按下即跳就没有那个窗口。
 *   ❸ 按钮不再挂 ev_press（它按下时改 opa 会触发重排，
 *      对象坐标变了 LVGL 就转成滚动 —— 正是 ❷ 的触发源）。
 *
 *   版面（CONT 高 282，左半 x=4..248 宽 244）：
 *      跳转键行 y=182..212（4 × 58 宽，缝 4 = 4×58+3×4 = 244 ✓）
 *      进度条   y=216..244（28 高）
 *      提示行   y=248..262
 *   热区 216..244 再上下各留 2 px ⇒ SEEK_HIT_Y=214，HIT_H=32
 */
/* ★★★ 2026-10-04 第二十一次重排（兰兰：「快进快退按键挡住了频谱最下端，
 *   不美观，可以把快进按键放在拖动条下面，拖动条可以设置个拖动手柄」）。
 *
 *   问题：第二十次把跳转键放在 y=182..212，而频谱是 58~196
 *   ⇒ 键的上半截 182~196 正好压在频谱最下面 14 px 上（兰兰说的「挡住最下端」）。
 *
 *   新版面（CONT 高 282，左半 x=4..248 宽 244）：
 *      提示行   y=44..56（频谱上方那行，见 s_lbl_level）
 *      频谱     y=58..196
 *      进度条   y=200..226（26 高）
 *      跳转键行 y=234..264（4 × 58 宽，缝 4 = 4×58+3×4 = 244 ✓）
 *      底部余   264..282 空着
 *   ⇒ 键在条【下面】，两者不再抢同一条空间。
 *
 *   ★★ 拖动手柄（兰兰：「拖动条可以设置个拖动手柄，这样或许有效」）：
 *     之前的 knob 只有 4 px 宽、且只在拖动时才显形（LV_OPA_TRANSP），
 *     看起来就是「条上有一道白线」，不知道能拖。
 *     现在改成 18×34 的圆角把手、**常显**（不透明），
 *     停在当前进度位置；直播时整条变灰、把手藏起来（不能拖）。
 *     把手本身也参与命中：seek_sec_at() 算的是「手指 x → 秒」，
 *     所以按在把手两侧 Anywhere 在条内都生效，不要求精确抓住把手。
 */
/* ★★★ 2.8 重排（10-10）：这几个宏全部按「整宽 272 = CONT_W - 8」重算。
 *   3.5 的值（条 244 宽 / 跳转键 58 / 行 y=234）在 212 高的页里全在屏外。*/
#define SEEK_BAR_Y   127             /* 进度条顶 */
#define SEEK_BAR_W   272             /* 整宽（= CONT_W - 8）*/
#define SEEK_BAR_X   4
#define SEEK_BAR_H   16              /* 16 px 厚（2.8 屏矮；3.5 是 26）*/
#define SEEK_KNOB_W  14              /* 拖动手柄 */
#define SEEK_KNOB_H  20
#define SEEK_STEP_W  65              /* 4 个键：4×65 + 3×4 = 272 ✓ */
#define SEEK_STEP_H  24
#define SEEK_STEP_GAP 4
#define SEEK_ROW1_Y  147             /* 跳分键行（在进度条下面）*/
/* 触摸热区：盖住条 127..143，上下各留 3 px ⇒ 124..146（22 高）。
 * ⚠️ 上边界 124 与「来源/进度提示」那行（110..125）重叠 1 px，
 *   但那行是 label（不收事件），且都在 p 上 ⇒ 谁在上面谁收事件。*/
#define SEEK_HIT_Y 124
#define SEEK_HIT_H 22

static lv_obj_t *s_arc_play;     /* 进度条底槽（灰色那条，只显示，不收事件）*/
static lv_obj_t *s_prog_play;    /* 进度条前景（绿色，宽 = 进度比例 × SEEK_BAR_W）*/
static lv_obj_t *s_seek_hit;     /* 透明触摸热区，事件挂这里 */
static lv_obj_t *s_seek_knob;    /* 拖动时的白色游标（4 px 宽）*/
static lv_obj_t *s_lbl_level;    /* 进度条下面那行提示（可拖 / 直播只读）*/
static int       s_seek_bar_w = SEEK_BAR_W;   /* 进度条总宽（换算拖动比例用）*/
static bool      s_seek_drag   = false; /* 正在拖 */
static int       s_seek_target = 0;     /* 拖动中的目标秒 */
/* ★ 第二十次：本轮是否由 seek_drag_poll() 接管了这次拖动。
 *   接管了就由轮询负责松手提交，事件回调只兜底（见 ev_seek_release）。*/
static bool      s_seek_polled = false;

static void fmt_mmss(int sec, char *out, size_t n);   /* 定义在文件后段 */
static void tap_note(const char *name);               /* 定义在文件后段 */
static void batt_indicator_refresh(void);             /* 定义在 4048 行 */
static void stat_refresh(void);                       /* 定义在 4127 行 */

/* 已播/总时长标签（定义在「播放页控件」那一段，seek 预览要提前改它）*/
static lv_obj_t *s_np_time;

/* 取当前触摸点（LVGL v9 是两参数版：indev + lv_point_t）
 *
 * ★★★ 2026-10-04 第二十一次：这里必须把【屏幕绝对坐标】换成
 *   【播放页容器内坐标】，否则拖动永远判定失败。
 *
 *   真相：lv_indev_get_point() 给的是整个屏幕（480×320）的绝对坐标，
 *   而 seek_sec_at() 里的 SEEK_BAR_X/SEEK_HIT_Y 全是【页内】坐标
 *   （页容器 p 摆在 (CONT_X, CONT_Y) = (56, 38)）。
 *   差值 x 少 56、y 少 38 —— 于是：
 *     · 手指按在条上（页内 y≈230 ⇒ 绝对 y≈268），
 *       判定 `py >= 214 && py <= 246` 直接不成立 ⇒ 永远不启拖；
 *     · 横坐标也偏 56 ⇒ 即便过了纵向判定，取到的秒数也是错的。
 *   ⇒ 第十八~二十版我一直在调热区高度（20→28→32→50）、调 LV_DIR_NONE、
 *     改用轮询自管手势 —— 全是在错的坐标系里打转，
 *     所以兰兰连报四次「本地拖动依然无效」。
 *
 *   ★ 教训：凡是自己读 indev 坐标做判定（不走 LVGL 事件派发），
 *     必须先做一次「绝对 → 页内」的换算。这是自管手势的固定成本。*/
static void seek_get_point(lv_indev_t *ind, int *px, int *py)
{
    lv_point_t pt;
    *px = *py = 0;
    if (!ind) return;
    lv_indev_get_point(ind, &pt);
    *px = pt.x - CONT_X;      /* 减去左侧导航栏宽 */
    *py = pt.y - CONT_Y;      /* 减去顶栏高 */
}

static void seek_preview_show(int sec)
{
    if (!s_np_time) return;
    int tot = app_radio_total_s();
    char a[16], b[16];
    fmt_mmss(sec, a, sizeof(a));
    if (tot > 0) { fmt_mmss(tot, b, sizeof(b)); lv_label_set_text_fmt(s_np_time, "%s / %s", a, b); }
    else          { lv_label_set_text(s_np_time, a); }
}

/* 触摸坐标 → 秒。返回 -1 表示超出范围（点在条外/无效）
 * ★ 第二十二次：拖动中（s_seek_drag）时【纵向不再设限】。
 *   兰兰报「拖动有作用，但不灵敏」——手指按住手柄后会自然上下浮动
 *   几像素（手的抖动），原来 `py <= SEEK_HIT_Y+SEEK_HIT_H+2` 一越界
 *   就 return -1，手柄立刻「粘住」不动了 ⇒ 主观上就是「不灵敏」。
 *   既然已经起拖，纵向宽容一点；横向仍然按 x 算，位置不会乱。*/
static int seek_sec_at(int px, int py)
{
    int tot = app_radio_total_s();
    if (tot <= 0) return -1;
    if (!s_seek_drag) {
        /* 热区上下各再留 2 px 余量（手指粗）*/
        if (py < SEEK_HIT_Y - 2 || py > SEEK_HIT_Y + SEEK_HIT_H + 2) return -1;
    }
    int w = s_seek_bar_w;
    int d = px - SEEK_BAR_X;              /* 左边 4 px */
    if (d < 0) d = 0;
    if (d > w) d = w;
    return (int)((int64_t)d * tot / w);
}

/* 把拖动手柄放到「离 px 最近的合法位置」。
 * ★ 手柄比条高（34 > 26），所以要垂直居中在条上：y = 条顶 - (手柄高-条高)/2。
 *   横向要【夹】在条内：手柄宽 18，若按最右边，中心不能超过 w-9，
 *   否则手柄有一半跑到条外面，看着像脱靶。*/
static void seek_knob_place(int px)
{
    if (!s_seek_knob) return;
    int d = px - SEEK_BAR_X;
    int half = SEEK_KNOB_W / 2;
    if (d < half)                d = half;
    if (d > s_seek_bar_w - half)  d = s_seek_bar_w - half;
    lv_obj_set_pos(s_seek_knob, SEEK_BAR_X + d - half,
                   SEEK_BAR_Y - (SEEK_KNOB_H - SEEK_BAR_H) / 2);
}

/* 拖动手柄显形 / 藏形。
 * ★ 第二十一次：把手改成【常显】——之前只在拖动时显形，
 *   平时完全透明，看起来就是「一条不能碰的绿线」，
 *   兰兰自然不会想到去拖它。*/
static void seek_knob_show(int px, bool on)
{
    if (!s_seek_knob) return;
    lv_obj_set_style_bg_opa(s_seek_knob, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (on) seek_knob_place(px);
}

/* 按下（事件回调）。★ 只是给轮询【打提前量】：
 *   轮询每 100 ms 一次，有一拍延迟；按下瞬间先把游标显出来，手感才跟手。
 *   真正的起拖判定仍在 seek_drag_poll()（那边能读 indev 状态，判得更准）。*/
static void ev_seek_press(lv_event_t *e)
{
    int px = 0, py = 0;
    seek_get_point(lv_event_get_indev(e), &px, &py);
    if (s_seek_drag) return;          /* 轮询已经起拖了，别重复 */
    if (!app_radio_can_seek()) {
        tap_note(app_radio_is_stream() ? "直播不能拖" : "读不出总长");
        return;
    }
    int sec = seek_sec_at(px, py);
    if (sec < 0) return;
    s_seek_drag   = true;
    s_seek_target = sec;
    ESP_LOGI(TAG, "seek DRAG-START -> %d s (tot=%d)", sec, app_radio_total_s());
    seek_knob_show(px, true);
    seek_preview_show(sec);
}

static void ev_seek_release(lv_event_t *e)
{
    (void)e;
    /* 真正的提交在 seek_drag_poll()（它看 indev 状态才知道手指已抬起）。
     * 这里只在「轮询还没接上」时兜一次，避免极端情况下拖了没反应。*/
    if (s_seek_drag && !s_seek_polled) {
        s_seek_drag = false;
        if (app_radio_seek(s_seek_target) == ESP_OK) {
            tap_note("跳转");
            ESP_LOGI(TAG, "seek request(fallback): %d s", s_seek_target);
        }
        player_refresh();
    }
}

/* 拖动：完全自管手势，不依赖 LVGL 的 PRESSING 事件。
 * ------------------------------------------------------------
 * ★★ 兰兰 10-04 报「本地进度条依然无效」，这是本轮第一号问题。
 *
 * 为什么不能靠 LV_EVENT_PRESSING（第十八~十九版的做法）：
 *   热区设了 LV_DIR_NONE（必须，否则父容器抢走手势），
 *   而 LVGL 只有在【对象被选为滚动对象】时才会持续派发 PRESSING；
 *   LV_DIR_NONE 的对象永远不是滚动对象 ⇒ PRESSING 永远不来。
 *   ⇒ 第十八版「靠 PRESSING 更新预览」从一开始就不可能工作。
 *
 * 现在怎么做（只依赖 LVGL 一定会给我的东西：indev 的状态与坐标）：
 *   按下/松手 —— 靠 indev 状态在轮询里自己认（PRESSED→起拖，RELEASED→提交），
 *                 事件回调只作为补充（有些屏按下时轮询刚好错过一拍）。
 *   指到哪    —— 每 100 ms 读一次 indev 当前点，直接改前景宽度。
 *   ⇒ 三件事都不再与 LVGL 的滚动判定竞争。
 */
static void seek_drag_poll(void)
{
    if (!s_seek_hit) return;
    lv_indev_t *ind = lv_indev_get_act();
    if (!ind) return;
    int px = 0, py = 0;
    seek_get_point(ind, &px, &py);
    bool down = (lv_indev_get_state(ind) == LV_INDEV_STATE_PRESSED);
    bool in_bar = (py >= SEEK_HIT_Y && py <= SEEK_HIT_Y + SEEK_HIT_H);

    /* ---- ① 起拖：手指按下且落在条上 ---- */
    if (down && in_bar && !s_seek_drag) {
        if (!app_radio_can_seek()) {
            /* 只提示一次，不反复打断 */
            tap_note(app_radio_is_stream() ? "直播不能拖" : "读不出总长");
            return;
        }
        int sec = seek_sec_at(px, py);
        if (sec >= 0) {
            s_seek_drag   = true;
            s_seek_polled = true;      /* 本次拖动由轮询接管 */
            s_seek_target = sec;
            seek_preview_show(sec);
            ESP_LOGI(TAG, "seek DRAG-START -> %d s (tot=%d)", sec, app_radio_total_s());
        }
    }

    if (!s_seek_drag) return;

    /* ---- ② 松手：提交 ---- */
    if (!down) {
        s_seek_drag   = false;
        s_seek_polled = false;
        if (app_radio_seek(s_seek_target) == ESP_OK) {
            tap_note("跳转");
            ESP_LOGI(TAG, "seek request: %d s", s_seek_target);
        } else {
            tap_note("跳不了");
            ESP_LOGW(TAG, "seek refused: target=%d can_seek=%d total=%d",
                     s_seek_target, (int)app_radio_can_seek(), app_radio_total_s());
        }
        player_refresh();
        return;
    }

    /* ---- ③ 拖动中：前景与手柄跟着手指走（不 seek）---- */
    int sec = seek_sec_at(px, py);
    if (sec < 0) return;
    int tot = app_radio_total_s();
    int w = (int)((int64_t)sec * s_seek_bar_w / (tot > 0 ? tot : 1));
    if (w != lv_obj_get_width(s_prog_play)) {
        lv_obj_set_width(s_prog_play, w > 0 ? w : 1);
    }
    seek_knob_show(px, true);
    if (sec != s_seek_target) {
        s_seek_target = sec;
        seek_preview_show(sec);
    }
}

/* ★ 第二十次：旧的 ev_seek_pressing / ev_seek_release 已删除。
 *   前者依赖 LV_EVENT_PRESSING —— 而 LV_DIR_NONE 的对象永远不会被
 *   LVGL 选为滚动对象 ⇒ PRESSING 根本不会派发，留着是死代码。
 *   后者的提交职责已并入 seek_drag_poll()（它看 indev 状态更准）。
 */

/* ============================================================
 *  ★★★ 跳转键：退2分 / 退10分 / 进10分 / 进2分（第二十次改「按下即跳」）
 * ============================================================
 * 兰兰 10-03：「快进10有了，但是10秒太短，需要分钟级别的，2分钟，10分钟」。
 * 兰兰 10-04：「快进10分钟有效，但经常要触碰几次才有效，2分钟无反应」。
 *   事件已从 LV_EVENT_CLICKED 改成 LV_EVENT_PRESSED（按下即跳）——
 *   根因与四个按钮的建法见 page_play_build 里的注释。
 */
static void ev_seek_step(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);   /* 正负 120 或 600 秒 */
    if (!app_radio_can_seek()) {
        tap_note("直播不能跳");
        return;
    }
    int cur = app_radio_elapsed_s();
    int tgt = cur + delta;
    if (tgt < 0) tgt = 0;
    int tot = app_radio_total_s();
    if (tgt >= tot) tgt = tot - 1;

    if (app_radio_seek(tgt) != ESP_OK) {
        tap_note("跳不了");
        return;
    }
    /* ⚠ 缓冲给 32 而不是 16：gcc 的 -Werror=format-truncation 按最坏情况算
     *   「快进 」(7 字节) + int 最长 11 字节 + 「 秒」(4 字节) 会超过 16。
     *   给足 32 就不会判截断（速查卡铁律：报截断时加大数组没用，要算准）。*/
    char msg[32];
    int  a = delta < 0 ? -delta : delta;
    if (a >= 60) snprintf(msg, sizeof(msg), "%s%d 分 %d 秒",
                          delta > 0 ? "快进" : "快退", a / 60, a % 60);
    else         snprintf(msg, sizeof(msg), "%s%.10d 秒", delta > 0 ? "快进 " : "快退 ", a);
    tap_note(msg);
    ESP_LOGI(TAG, "step %+d s -> %d/%d", delta, tgt, tot);
    player_refresh();
}

/* ============================================================
 *  倍速键（第二十二次）—— 1.0X → 1.2X → 1.4X → 1.0X 循环
 * ============================================================
 * ★ 为什么挂 LV_EVENT_PRESSED 而不是 CLICKED：
 *   与跳转键同一个理由 —— CLICKED 要「按下 + 在同一对象内松手」，
 *   中途被 LVGL 判成滚动就作废，表现为「要碰几次」。
 *   倍速是「立刻要听到效果」的操作，按下即切最合适。
 *
 * ★ 切速会重开 codec（约几十毫秒静默），所以
 *   【没在播的时候不给切】—— 免得按了没声音、还以为机器坏了。
 *   空闲时按它就提示一句。*/
static void ev_speed_cycle(lv_event_t *e)
{
    (void)e;
    if (!app_radio_is_playing()) {
        tap_note("先播一个再调速");
        return;
    }
    int cur = app_radio_speed();
    int nxt = (cur < 110) ? 120 : ((cur < 130) ? 140 : 100);
    app_radio_set_speed(nxt);

    if (s_lbl_speed) {
        /* 键宽 78，11px 六个字符（倍速 1.4X = 8 字符 ≈ 56 px）放得下 */
        lv_label_set_text_fmt(s_lbl_speed, "倍速 %d.%dX", nxt / 100, (nxt / 10) % 10);
        /* 非 1.0X 时把字换成琥珀色，一眼知道「现在在加速」——
         * 不然过一会儿就忘了自己刚才按过。*/
        lv_obj_set_style_text_color(s_lbl_speed, (nxt == 100) ? C_GREEN : C_AMBER, 0);
    }
    char msg[24];
    snprintf(msg, sizeof(msg), "%.10s %d.%dX",
             nxt == 100 ? "已恢复正常" : "已切到", nxt / 100, (nxt / 10) % 10);
    tap_note(msg);
}

/* ★ 10-11 SD 播放模式键（兰兰：「SD卡播放模式…单曲循环，连播，随机都要」）。
 *
 *  三档循环切换，按一下换下一档。键上只放短名（连播/单曲/随机）——
 *  48 px 宽放得下（实测各 28 px，见 _ui_width.py）。
 *
 *  ★ 与倍速键同一个理由用 LV_EVENT_PRESSED（按下即生效，不等松手）：
 *    CLICKED 要「按下 + 在同一对象内松手」，被 LVGL 判成滚动就作废，
 *    表现为「要碰几次」。
 *  ★ 与倍速键的区别：【没在播的时候也允许切】。
 *    倍速要立刻听到效果，所以空闲时不给切；而模式是「下次放完怎么走」的
 *    预设，用户完全可能先设好再点歌 —— 拦着反而莫名其妙。*/
static const char *const k_mode_short[3] = { "连播", "单曲", "随机" };
static const char *const k_mode_full[3]  = {
    "已切到连播",
    "已切到单曲循环",
    "已切到随机播放",
};

/* ★★ 10-11：三颗模式键的"谁亮"由这里统一刷。
 *   为什么要一个 sync 函数而不是"点的时候自己改自己"：
 *     ① 点第 3 颗时，第 1 颗必须灭掉 —— 两颗都亮就成了"同时选中两个"；
 *     ② 页面重建（离开再进来）时要按【NVS 里的真实模式】点亮，
 *        不能默认第 0 颗（用户上次设的是"随机"，进来就必须亮"随机"）。
 *   ⇒ 一处实现，两个入口（点击后 / 建页后）都调它。*/
static void mode_sync(void)
{
    int m = app_radio_play_mode();
    if (m < 0 || m > 2) m = 0;
    for (int i = 0; i < 3; i++) {
        if (s_btn_mode3[i]) {
            lv_obj_set_style_bg_color(s_btn_mode3[i],
                                      (i == m) ? C_HILITE : C_PANEL, 0);
        }
        if (s_lbl_mode3[i]) {
            /* 选中 = 绿（与倍速键"常态"同色），未选中 = 灰。
             * ⚠️ 不再用「连播青 / 单曲琥珀 / 随机琥珀」那套 ——
             *   那是【一颗循环键】时代的做法：用颜色说明"现在不是连播"。
             *   三颗并列后，颜色只承担一件事：谁是当前。 */
            lv_obj_set_style_text_color(s_lbl_mode3[i],
                                        (i == m) ? C_GREEN : C_MUTED, 0);
        }
    }
}

/* 点第 m 颗模式键（m 由 user_data 传，0/1/2）。
 * ★ 与倍速键的区别：【没在播的时候也允许切】。
 *   倍速要立刻听到效果，所以空闲时不给切；而模式是「下次放完怎么走」的
 *   预设，用户完全可能先设好再点歌 —— 拦着反而莫名其妙。*/
static void ev_mode_pick(lv_event_t *e)
{
    int m = (int)(intptr_t)lv_event_get_user_data(e);
    if (m < 0 || m > 2) return;
    if (m == app_radio_play_mode()) { mode_sync(); return; }   /* 重复点：只复位高亮 */
    app_radio_set_play_mode(m);
    mode_sync();
    tap_note(k_mode_full[m]);
}

static lv_obj_t *s_np_name;      /* 曲名（页面顶部，两行）*/
/*static lv_obj_t *s_np_time;*/      /* 已播 / 总时长 —— 已在文件头为 seek 预览提前声明 */
/* ★★ 10-11：s_np_hint（最底部那行状态提示）【已删】——
 *   兰兰：「播放页底部有当前电台状态用不到」。第九行整行让给音量控件。
 *   「接着上次听」那一条没丢，搬进了 s_np_meta 的第一行（见 player_refresh）。*/
static lv_obj_t *s_np_meta;      /* 右卡：格式信息 */
static lv_obj_t *s_np_state;     /* 右卡：状态 / 错误原因 */

/* 当前本地播放所在的目录（「上一个 / 下一个」要在同一目录里换曲）。
 * ★ 只存目录名（< 256 字节），不存整个播放列表 —— 一个节目目录几十上百首、
 *   每首文件名最长 255 字节，缓存列表要几十 KB 内存，不值得。*/
static char s_local_dir[256] = "/sdcard";

/* ============================================================
 *  小工具
 * ============================================================ */
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, const lv_font_t *font, lv_color_t color,
                       const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    return l;
}

/* ★ 前向声明：splash_build 在文件前段，而 now_minutes 定义在后段。
 *   （第 10-04 那次「顶栏时钟死 label」就是前向声明漏了、
 *     结果 stat_refresh 那段代码白写了。）*/
static int  now_minutes(void);
static void splash_done_cb(lv_timer_t *t);

/* 拾声 标识：五根声波柱 */
static lv_obj_t *logo_mark(lv_obj_t *parent, int x, int y, int h, lv_color_t color)
{
    static const int wgt[5] = {40, 70, 100, 66, 46};
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, 30, h);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 5; i++) {
        int bh = h * wgt[i] / 100;
        if (bh < 3) bh = 3;
        box(c, i * 6, (h - bh) / 2, 4, bh, color, 2);
    }
    return c;
}

/* 频谱柱 —— ★ 10-03 起改成【真频谱】。
 * 原来这里画的是一组写死的假柱（static const int wgt[13]），
 * 兰兰要「动态频谱」，就把首页这处也接到 app_radio 的 Goertzel 分析上。
 * 数据源是解码后的 PCM，所以首页那张卡上的柱子和耳朵里听到的声音一致。*/
#define HOME_SPEC_N  13          /* 首页卡上摆几根（比播放页密一点，140 px 宽）*/
static lv_obj_t *s_home_bar[HOME_SPEC_N];
static int       s_home_bw = 8, s_home_gap = 2, s_home_h = 18, s_home_y = 80, s_home_x = 14;

static void spectrum(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color)
{
    s_home_x = x;
    s_home_y = y;
    s_home_h = h;
    s_home_bw = (w - (HOME_SPEC_N - 1) * s_home_gap) / HOME_SPEC_N;
    if (s_home_bw < 3) s_home_bw = 3;
    for (int i = 0; i < HOME_SPEC_N; i++) {
        /* 初始 3px 底线：不放的时候也别是一排空白 */
        s_home_bar[i] = box(parent, x + i * (s_home_bw + s_home_gap),
                            y + h - 3, s_home_bw, 3, color, 2);
    }
}

/* 首页那排柱子按真频谱重画（每 12 根取 1 根，13 根对应 12 个频段）*/
static void home_spec_refresh(void)
{
    int n = app_radio_spectrum_bins();
    for (int i = 0; i < HOME_SPEC_N; i++) {
        /* 13 根对 12 段：最后一根跟着最高频段走（多一根显得不空）*/
        int idx = (i < n) ? i : (n - 1);
        int v = (n > 0) ? app_radio_spectrum(idx) : 0;
        if (!app_radio_is_playing() || app_radio_is_paused()) v = 0;
        int bh = s_home_h * v / 100;
        if (bh < 3) bh = 3;
        if (bh > s_home_h) bh = s_home_h;
        lv_obj_set_size(s_home_bar[i], s_home_bw, bh);
        lv_obj_set_pos(s_home_bar[i],
                       s_home_x + i * (s_home_bw + s_home_gap),
                       s_home_y + s_home_h - bh);
    }
}

/* ============================================================
 *  通用「可点控件」
 * ============================================================ */
static void tap_note(const char *name)
{
    int rx, ry, lx, ly;
    bool pr;
    app_touch_get_last(&rx, &ry, &lx, &ly, &pr);
    ESP_LOGI(TAG, "tap: %s  lv=(%d,%d)", name, lx, ly);
}

static void ev_press(lv_event_t *e)
{
    lv_obj_set_style_opa(lv_event_get_target(e), LV_OPA_60, 0);
}

static void ev_release(lv_event_t *e)
{
    lv_obj_set_style_opa(lv_event_get_target(e), LV_OPA_COVER, 0);
}

static void ev_named_tap(lv_event_t *e)
{
    const char *tag = (const char *)lv_event_get_user_data(e);
    tap_note(tag ? tag : "?");
}

/* 播放提示音：不接任何音源也能验证「喇叭会响」 */
static void ev_beep(lv_event_t *e)
{
    (void)e;
    tap_note("播放提示音");
    app_audio_beep_async();
}

/* ============================================================
 *  声音总开关（静音）—— 10-10 补，兰兰真机报「没声音」的真因
 * ============================================================
 *  为什么必须补这一条键：
 *    app_settings.c 的 DEF_MUTE = true（开机静音，兰兰 10-03 定的），
 *    而 apply_audio() 里是 `mute ? 0 : volume` ⇒ **静音时 DAC 增益恒为 0**。
 *    3.5 上解除它的是 ui_shell.c 那个设置面板；2.8 没编 ui_shell.c，
 *    全工程【没有任何地方调用 xs_cfg_toggle_mute()】
 *    ⇒ 默认静音 + 无处解除 = 永远没声音（连开机那三声提示音都是哑的）。
 *
 *  ⚠️ 静音语义：只把 DAC 增益归零，**不碰播放器**（见 app_settings.c）——
 *     静音时电台照常在解码、频谱照常在动，所以「频谱在动却没声音」
 *     看着像硬件坏了，其实只是这一位。
 * ============================================================ */
static lv_obj_t *s_lbl_snd = NULL;      /* 状态页那个键上的文字 */

static void snd_apply_label(void)
{
    if (!s_lbl_snd) return;
    bool m = xs_cfg_muted();
    lv_label_set_text(s_lbl_snd, m ? "已静音" : "有声");
    lv_obj_set_style_text_color(s_lbl_snd, m ? C_AMBER : C_GREEN, 0);
}

static void ev_toggle_mute(lv_event_t *e)
{
    (void)e;
    xs_cfg_toggle_mute();
    ESP_LOGW(TAG, "声音：%s（音量设定 %d）",
             xs_cfg_muted() ? "已静音" : "有声", xs_cfg_volume());
    /* ★ 解除静音时若音量是 0，给它一个能听见的起点 ——
     *   否则「开了」却还是没声，又是一次误判（音量 0 的静音两种成因不同）。*/
    if (!xs_cfg_muted() && xs_cfg_volume() < 30) xs_cfg_set_volume(60);
    snd_apply_label();
    if (!xs_cfg_muted()) app_audio_beep_async();   /* 立刻给一声，证明链路通 */
}

/* ============================================================
 *  关机（10-04 新增）
 * ============================================================
 *  ⚠️ 为什么必须【二次确认】而不是按了直接关：
 *   app_pwr_shutdown() 不返回（进 deep sleep），
 *   状态页这几个按钮是挨着的（提示音 / 手机配网 / 关机），
 *   一次误触就从「有声可玩」变成「黑屏，要按 BOOT 键才回来」——
 *   对一台还没完全熟悉的机器来说体验很差。
 *   ⇒ 第一次点把文字换成「再点一次关机」，5 秒内不点就自动还原。
 *
 *  ★ 关机后怎么开机：**按住 BOOT 键**，等屏幕亮起再松开。
 *    ⚠️ 10-04 兰兰实测「关机后马上又自己开机」——那是 v1.32 的原理性错误
 *    （EXT1 判电平不判边沿，ANY_HIGH 会在进深睡那一刻立刻自唤醒），
 *    已改成 ANY_LOW。代价见 app_pwr.h 顶部：唤醒时正按着 BOOT，
 *    ESP32 采样 IO0 为低会判成下载模式 ⇒ 屏幕亮起前别松手；
 *    万一没刷出来，松开再按一下 RESET 键即可。*/
static lv_timer_t *s_sdconfirm_timer;   /* 二次确认的自动还原计时 */
static lv_obj_t   *s_btn_power;         /* 关机键本体 */
static lv_obj_t   *s_lbl_power;         /* 关机键上的文字 */

static void power_confirm_timeout_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    s_sdconfirm_timer = NULL;      /* 已自己删，置 NULL 防二次删除 */
    if (s_lbl_power) {
        lv_label_set_text(s_lbl_power, "关机");
        lv_obj_set_style_text_color(s_lbl_power, C_RED, 0);
    }
}

static void ev_power(lv_event_t *e)
{
    (void)e;
    (void)s_btn_power;

    /* 已经在「等第二次」⇒ 这次就是确认，直接关。*/
    if (s_sdconfirm_timer) {
        ESP_LOGI(TAG, "UI: 二次确认通过 → 关机");
        tap_note("关机");
        if (s_lbl_power) {
            lv_label_set_text(s_lbl_power, "正在关机…");
            lv_obj_update_layout(s_lbl_power);
        }
        /* 让 LVGL 把「正在关机…」这几个字真的画出来再黑屏。
         * ★ 这里【不能】用 lv_timer —— app_pwr_shutdown() 不返回，
         *   建一个 600 ms 的定时器出来还没等到就进 deep sleep 了，纯浪费。
         *   而且定时器回调在 deep sleep 前必须停掉，留着反而是隐患。
         *   正确做法：在当前（LVGL 任务）上下文里同步延时并让 LVGL 跑一次渲染。*/
        lv_timer_handler();          /* 立刻处理一次布局+渲染 */
        vTaskDelay(pdMS_TO_TICKS(350));
        app_pwr_shutdown();          /* 不返回 */
        return;
    }

    /* 第一次点：换文案 + 5 秒倒计时 */
    tap_note("关机？再点一次");
    if (s_lbl_power) {
        lv_label_set_text(s_lbl_power, "再点一次关机");
        lv_obj_set_style_text_color(s_lbl_power, C_RED, 0);
    }
    s_sdconfirm_timer = lv_timer_create(power_confirm_timeout_cb, 5000, NULL);
}

/* 离开状态页时把「再点一次」状态清掉，
 * 不然用户在别的页逛一圈回来，按钮还写着「再点一次关机」，
 * 一下就真关了 —— 反而更危险。
 * ⚠️ 实际钩子挂在 goto_page() 里（那里才知道页面要换成哪一页），
 *   这个函数只留权责说明，不单独调用。*/

static lv_obj_t *tick(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg, int radius,
                      const char *tag)
{
    lv_obj_t *o = box(parent, x, y, w, h, bg, radius);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, ev_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(o, ev_release, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(o, ev_release, LV_EVENT_PRESS_LOST, NULL);
    if (tag) lv_obj_add_event_cb(o, ev_named_tap, LV_EVENT_CLICKED, (void *)tag);
    return o;
}

/* ★★ 10-11：给「按下即动作」的键用（音量 ±、模式三颗、亮度 ±、跳分钟那四个）。
 *
 *   ★ 与 tick() 的区别：【不挂 ev_press / ev_release】。
 *     tick() 靠改透明度做按压反馈，而 10-04 查出来的真凶正是这个：
 *       「改样式 → 重排 → 指针下的对象变了」，
 *       LVGL 在同一帧里发现指针下的对象变了，就把它转成 scroll 并放弃事件
 *       ⇒ 症状是「要碰几次才灵」。
 *     这些键的动作都在 PRESSED 那一刻完成，不需要按压动画 ——
 *     反馈由别处给出（读数变化 / 高亮移动 / 跳转本身可见）。
 *
 *   ★ 必须不可滚（LV_DIR_NONE）：LVGL 不把可滚动对象当"可点"，
 *     与 s_seek_hit 是同一条理由，见那里注释。
 *   ⚠️ 只挂 PRESSED，不挂 CLICKED —— 等松手才有反应会引入"按住算不算"的窗口。*/
static lv_obj_t *press_btn(lv_obj_t *parent, int x, int y, int w, int h,
                           lv_color_t bg, int radius,
                           lv_event_cb_t cb, void *user)
{
    lv_obj_t *o = box(parent, x, y, w, h, bg, radius);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(o, cb, LV_EVENT_PRESSED, user);
    return o;
}

/* ============================================================
 *  单选组 —— 胶囊 / 列表行共用
 * ============================================================ */
#define GRP_MAX 12
#define GRP_FG  2

typedef struct {
    lv_obj_t *bg[GRP_MAX];
    lv_obj_t *fg[GRP_MAX][GRP_FG];
    int        fg_n;
    int        cnt;
    int        sel;
    lv_color_t bg_on, bg_off;
    lv_color_t fg_on, fg_off;
    const char *name;
    void (*on_change)(int idx);   /* 选中变化后的回调（可选）*/
} radio_grp_t;

typedef struct { radio_grp_t *g; int idx; } grp_arg_t;

static grp_arg_t s_arg[32];
static int       s_arg_n;

static grp_arg_t *arg_of(radio_grp_t *g, int idx)
{
    if (s_arg_n >= (int)(sizeof(s_arg) / sizeof(s_arg[0]))) s_arg_n = 0;
    s_arg[s_arg_n].g = g;
    s_arg[s_arg_n].idx = idx;
    return &s_arg[s_arg_n++];
}

static void grp_apply(radio_grp_t *g)
{
    for (int i = 0; i < g->cnt; i++) {
        bool on = (i == g->sel);
        if (g->bg[i]) lv_obj_set_style_bg_color(g->bg[i], on ? g->bg_on : g->bg_off, 0);
        for (int j = 0; j < g->fg_n; j++) {
            if (g->fg[i][j]) {
                lv_obj_set_style_text_color(g->fg[i][j], on ? g->fg_on : g->fg_off, 0);
            }
        }
    }
}

static void grp_clicked(lv_event_t *e)
{
    grp_arg_t *a = (grp_arg_t *)lv_event_get_user_data(e);
    if (!a || !a->g) return;
    a->g->sel = a->idx;
    grp_apply(a->g);
    tap_note(a->g->name ? a->g->name : "选项");
    if (a->g->on_change) a->g->on_change(a->idx);
}

static lv_obj_t *grp_chip(radio_grp_t *g, lv_obj_t *parent, int x, int y, int w, int h,
                          int radius, const lv_font_t *font, const char *text)
{
    int i = g->cnt++;
    if (i >= GRP_MAX) return NULL;
    lv_obj_t *o = tick(parent, x, y, w, h, g->bg_off, radius, NULL);
    lv_obj_add_event_cb(o, grp_clicked, LV_EVENT_CLICKED, arg_of(g, i));
    lv_obj_t *l = label(o, 0, 0, font, g->fg_off, text);
    lv_obj_center(l);
    g->bg[i] = o;
    g->fg[i][0] = l;
    return o;
}

static void grp_init(radio_grp_t *g, int fg_n, lv_color_t bg_on, lv_color_t bg_off,
                     lv_color_t fg_on, lv_color_t fg_off, const char *name)
{
    memset(g, 0, sizeof(*g));
    g->fg_n = fg_n;
    g->bg_on = bg_on;
    g->bg_off = bg_off;
    g->fg_on = fg_on;
    g->fg_off = fg_off;
    g->name = name;
}

/* ★ 10-03 第九次：首页分类（s_grp_cat）和发现页地区（s_grp_prov）
 *   这两个单选组拆掉了 —— 栏目/省份条改成了「横向可滑的胶囊 + 点一下
 *   进全屏页」，不再需要 grp_apply() 那种「选中态跟着切」的单选语义。
 *   夜间页的 s_grp_sleep 还在用。*/
static radio_grp_t s_grp_sleep;  /* 夜间页定时 */

/* ============================================================
 *  虚拟双列网格 —— 几百个电台不可能一次全建成控件
 * ============================================================
 *  ★ 为什么必须「虚拟」：
 *    LVGL 的内置池只有 112 KB（sdkconfig CONFIG_LV_MEM_SIZE），
 *    而台单现在有 686 个台。就算一行只放一个 label（label + 文本约
 *    120~200 B），686 行也要 80~140 KB —— 直接把池抽干，
 *    release 下 lv_malloc 返回 NULL 而 LV_ASSERT_MALLOC 是空操作 ⇒ 往
 *    NULL 写 ⇒ 无限重启（10-02 踩过）。
 *
 *  ★ 做法（和手机列表一样）：
 *    只建「可视行数 × 2」个格子当作回收站（首页 3 行 6 个、
 *    全屏页 7 行 14 个），跑再多台内存也不变。
 *    再放一个高 = 总行数 × 行高 的 spacer 把内容撑起来，
 *    靠 LV_EVENT_SCROLL 读 scroll_y 算出「现在是第几行」，
 *    把那 10 个格子重新填成可见的那几行。内存恒定，与台数无关。
 *
 *  ★ 为什么不能靠 LVGL 自己的布局：
 *    LVGL 不虚拟化 —— 不会自动回收屏幕外的子控件。
 */
/* ★★★ 10-05 v2.0：三列（兰兰拍板「电台页双列比较好，能三列最好，毕竟电台太多了」）
 *   依据 _design/v2-07-列表页.png：三列 × 8 行 = 一屏 24 台。
 *   几何：屏 480 宽 − 列表页左右边距(6×2) = 468，格间距 6 ⇒
 *         cell_w = (468 − 6×2) / 3 = 152 px。
 *   ★ 连带影响：列宽从 230 掉到 152 ⇒ 台名截断必须从 13 字降到 8 字
 *     （12px 中文字宽约 12 px，152 宽减去内边距只放得下 8 个汉字 + 留白）。
 *     改 utf8_copy_n 的第二个参数时【必须同步改这一行的 cell_w 计算】，
 *     否则就是台名撞出格子边框。真机照片一眼能看出来。*/
#define GRID_COLS  3      /* 默认列数；★ 2.8 起改成【运行期可传】（g->cols）*/
#define GRID_ROWS  8      /* 上限：全屏页 7 行 + 一点余量 */
#define GRID_VIS   (GRID_COLS * GRID_ROWS)   /* 槽位数组上限，不随 cols 变 */
#define GRID_MAX   512          /* 一个筛选结果最多记 512 个台下标 */

/* ============================================================
 *  ★ 10-11：网格里怎么区分「电台格」和「本地音频格」
 * ============================================================
 *  兰兰：「起码在历史播放里面有本地音频记录是可以这样续播的」——
 *  ⇒ 历史页在电台条目后面接上「最近听过的本地音频」。
 *  两边数据源不同（台名在 app_hist、进度在 app_resume），
 *  但网格只有一个 int pool[]，所以要能在同一个 int 里编码两件事。
 *
 *  编码：值 >= HIST_LOCAL_BASE ⇒ 本地条目，序号 = 值 - HIST_LOCAL_BASE
 *        （序号查 app_resume_path_at()）；否则按老规矩 = 台下标。
 *
 *  ⚠️ 为什么用【大正数】而不是负数：
 *     grid_fill/grid_show_cell 里 `station_idx < 0` 已经被"空格子"占用了
 *     （见 grid_show_cell 第一段）。用负数编码本地条目，
 *     它们会被当成空格子直接跳过 —— 表现为"历史页里本地音频一条都不显示"，
 *     而且完全不报错。
 *  ⚠️ 也不能用小的正数（比如把它排在台下标之后）：越界读 app_st_get()
 *     就是读别人的内存。100000 远大于任何台数，一眼就能看出是编码值。*/
#define HIST_LOCAL_BASE   100000

typedef void (*grid_fill_fn)(int station_idx);   /* 把格子填成这一台 */

typedef struct {
    lv_obj_t *cont;                 /* 可滚动容器 */
    lv_obj_t *cell[GRID_VIS];       /* 复用的格子 */
    lv_obj_t *nm[GRID_VIS];         /* 格子里的台名 label */
    lv_obj_t *spacer;               /* 撑内容高度用的透明对象 */
    int       idx[GRID_VIS];        /* 每个格子当前显示的台下标，-1 = 空 */
    int       pool[GRID_MAX];       /* 当前筛选结果的下标池 */
    int       total;                /* 池里几个台 */
    int       row_h;                /* 一行高（含缝） */
    int       cell_h;               /* 格子自身高 */
    int       cell_w;               /* 格子宽 */
    int       first;                /* 当前窗口第一个是第几行 */
    int       vis_rows;             /* 可视几行（运行期定：发现 4 / 全屏 6）*/
    int       cols;                 /* ★ 2.8 新增：运行期列数。
                                     *   3.5 固定三列（480 宽），2.8 内容区只有
                                     *   280 ⇒ 三列每格 84 px，16px 台名只放得下
                                     *   5 个字（台名普遍 6~8 字）⇒ 满屏省略号。
                                     *   改成两列 129 px（8 个字），可读性优先。*/
    int       nslot;                /* 实际建了几个格子 = vis_rows * cols */
    lv_color_t bg, bg_sel;
    lv_color_t fg;
    const lv_font_t *font;
    void (*on_play)(int station_idx);    /* 点格子 */
    void (*on_fav)(int station_idx);     /* 长按格子 = 收藏/取消 */
    int       from_page;                 /* 这层网格属于第几页：
                                           * 播放页的返回键就回这一页。
                                           * 兰兰 10-03：「要做到从哪里来能够回到哪里」*/
    int       mode;                      /* 0 = 显示台名；1 = 显示【海外地区块】
                                           *（pool 里存的也变成地区下标）*/
    void (*on_region)(int slot, int idx);    /* mode==1 时点格子的回调 */

    /* ★ 10-11：长按 = 【从列表里删掉】而不是收藏。
     *   只给「收藏页 / 历史页」这两层置 1（见 list_page_refresh）。
     *   为什么不在 grid_slot_long 里直接读 s_list_mode：
     *     那会把「哪一个网格」和「全局当前在第几页」耦死 ——
     *     本文件有 4 个网格共用一张代码，全局变量描述不了每个网格的身份。
     *     挂在网格自己身上最准（和 mode / on_region 同一个思路）。*/
    int       del_mode;
    void (*on_del)(int idx);                 /* del_mode==1 时长按格子的回调 */
} st_grid_t;

/* ============================================================
 *  横向虚拟胶囊条 —— 栏目（11 个）/ 省份（31 个）共用
 * ============================================================
 *  ★ 为什么也是虚拟的：31 个省份每个一颗胶囊（胶囊 + 文字 = 2 个
 *    lv_obj，各约 250~400 B）就是 60~120 B×62 ≈ 25 KB。
 *    LVGL 内置池只有 98 KB，而同屏还有三套网格 —— 直接把池抽干，
 *    渲染线程申请不到缓冲就崩在 lv_draw_sw_mask（10-03 踩过）。
 *
 *  ★ 做法：横向也只建 STRIP_N 颗胶囊，靠 LV_EVENT_SCROLL 读 scroll_x
 *    算出「现在是第几颗」，把那几颗循环填上内容。内存与条目数无关。
 *    跟手机上的横滑 tab bar 是一个道理。
 */
#define STRIP_N 7          /* 同屏可见几颗（480 宽，一颗 64+5 缝 ⇒ 7 颗刚好）*/

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *chip[STRIP_N];
    lv_obj_t *txt[STRIP_N];
    lv_obj_t *spacer;
    int       n;           /* 总共几项 */
    int       first;        /* 当前窗口第一颗是第几项 */
    int       cw;           /* 一颗宽 */
    int       vis;          /* ★ 第二十次：实际同屏可见几颗（按宽度算，不是写死 7）*/
    lv_obj_t *btn_prev;      /* 第十九次：左翻页按钮（放在 cont 外面） */
    lv_obj_t *btn_next;      /* 第十九次：右翻页按钮 */
    void (*on_pick)(int idx);
} hstrip_t;

static hstrip_t s_strip_cat;    /* 首页栏目条 */
static hstrip_t s_strip_prov;   /* 发现页省份条 */

/* 栏目胶囊的短名 —— 13 个栏目名有 4 个字（「新闻综合」「怀旧老歌」），
 * 胶囊只有 57 px 宽放不下，用短名；进全屏页后显示全名。
 *
 * ★★★ 2026-10-04 第二十一次：补齐到 NET_CAT_N + 1 = 14 项。
 *   之前这份数组只写了 11 项，但声明是 [NET_CAT_N + 1]（14）⇒
 *   下标 11/12/13 是【未初始化的野指针】，hstrip_show() 直接
 *   lv_label_set_text() 拿它当 C 字符串 ⇒ 读到相邻内存里的东西。
 *   兰兰看到的「栏目最右边有两个全部按键」就是这里来的 ——
 *   第 11 项写的是「全部」，第 12、13 项读到的是「全部」附近的残值。
 *   ⇒ 教训：定长数组的「声明长度」与「初始化项数」必须一致，
 *      这类错编译器不报、运行起来也只是「显示怪怪的东西」，最难查。
 *
 * ★★ 2026-10-04 第二十一次：栏目从 13 档缩到 12 档（删掉空着的「综合」），
 *   这里同步删掉它的短名「综合」，剩 12 + 「全部」= 13 项。
 *
 * ★★★ 2026-10-04 第二十八次：前面插 3 个【自定义入口】（兰兰定）：
 *   收藏 / 历史 / 音乐库，然后才是 12 个栏目 + 全部。
 *   下标约定（下标是 hstrip 与 ev_pick_cat 之间唯一的契约，别乱改）：
 *       0      收藏    → 进全屏收藏页
 *       1      历史    → 进全屏历史页
 *       2      音乐库  → 进 SD 本地音频页（第 5 页）
 *       3..14  12 个栏目
 *       15     全部
 *   ⚠️ 这三项【不是栏目】（cat index 越界），ev_pick_cat 里必须先分流，
 *     否则 s_list_cat=0 会被当成「新闻」列出一堆无关的台。
 *
 * ★★ 标签名「音乐库」在标签条上写不下：
 *   hstrip 胶囊宽只有 51 px（= 340/6 - 5），「音乐库」三个 14px 汉
 *   要 42 + 内边距 8 = 50 px ⇒ 只余 1 px，一个字形宽度都可能挤掉。
 *   ⇒ 标签条上用两字「卡内」；首页那张大卡片才写全名「音乐库」
 *     （卡片宽 152，放得下）。同一页两个地方叫不同名字会有点跳，
 *     但 51 px 是物理上限，硬塞会截字。*/
#define HCAT_FAV  0
#define HCAT_HIST 1
#define HCAT_SD   2
#define HCAT_REAL 3          /* 第一个真栏目的下标 */
#define HCAT_ALL  (HCAT_REAL + NET_CAT_N)   /* = 15 */

/* ★★ 10-11 兰兰：「首页的宗教换成继续收听」。
 *   ⇒ 第 11 格（= HCAT_REAL + 10，原先正是「宗教」那一格）改作「继续收听」入口：
 *      点它进第 6 页，列的是【有播放进度、还没听完】的本地音频，点一条接着放。
 *
 *   ⚠️ 三个必须说清的边界（都在这里一次性钉死，免得以后又踩）：
 *     ① 【只换这一格的显示与点击行为，不动栏目表】——
 *        g_cat_name 仍是 12 项，HCAT_ALL 仍是 15，
 *        所以「全部电台」网格里宗教台照样找得到，一个台都不会丢。
 *     ② 【不能写成 cat - HCAT_REAL 那条通用路径】——
 *        通用路径会把 13 映射成 g_cat_name[10] = 宗教，
 *        于是点「续听」列出一整页宗教台（静默错、编译零报错，
 *        同 ev_pick_cat 抬头那条「点收藏却列出一页新闻」的坑）。
 *        ⇒ 必须在通用分支【之前】单独拦掉。
 *     ③ 【文案只能 2 个字】—— 胶囊/格子都窄，
 *        「继续收听」4 个 14px 汉字 = 56 px 塞不进 51 px 的胶囊，
 *        所以格子上用「续听」，进页后的大标题才写全「继续收听」。*/
#define HCAT_RESUME  (HCAT_REAL + 10)       /* = 13，原「宗教」那一格 */

static const char *const k_cat_short[HCAT_ALL + 1] = {
    /* 3 个自定义入口（标签条上用短名，理由见上）*/
    "收藏", "历史", "卡内",
    /* 12 个栏目（顺序与 g_cat_name 一致） */
    "新闻", "交通", "音乐", "文艺", "说书", "戏曲", "怀旧",
    "网络", "教育", "伴音",
    "续听",     /* ← 第 10 项：原「宗教」，10-11 换成「继续收听」入口（见 HCAT_RESUME） */
    "境外",
    /* 全部 */
    "全部",
};

static void hstrip_show(hstrip_t *h, int slot, int idx)
{
    if (idx < 0 || idx >= h->n || slot >= h->vis) {
        lv_obj_set_style_bg_opa(h->chip[slot], LV_OPA_TRANSP, 0);
        lv_label_set_text(h->txt[slot], "");
        return;
    }
    lv_obj_set_style_bg_opa(h->chip[slot], LV_OPA_COVER, 0);
    const char *s = (h == &s_strip_cat) ? k_cat_short[idx]
                                        : g_prov_name[idx % NET_PROV_N];
    lv_label_set_text(h->txt[slot], s);
    /* ★ 海外地区（第 31 项起）用琥珀色胶囊，一眼能认出「这边是海外」。
     *   兰兰 10-03 问「海外台没有做进去？」—— 做了（194 台），
     *   但它们排在 31 个国内省之后，滑到底才看得到，看着像没做。
     * ⚠️ 循环复用：胶囊是 7 颗轮流填内容的，所以每次都要【两个分支都显式写】，
     *   不能只在海外分支设色 —— 否则从海外滑回国内那一格还带着琥珀底。*/
    bool ovs = (h == &s_strip_prov) && (idx >= NET_PROV_CN_N);
    if (ovs) {
        lv_obj_set_style_bg_color(h->chip[slot], lv_color_hex(0x3A2E12), 0);
        lv_obj_set_style_text_color(h->txt[slot], C_AMBER, 0);
    } else if (h == &s_strip_prov) {
        lv_obj_set_style_bg_color(h->chip[slot], C_PANEL, 0);
        lv_obj_set_style_text_color(h->txt[slot], C_TEXT, 0);
    }
}

/* 槽位 → 它当前代表第几项（用 idx 数组存，模块少一点）*/
static int hstrip_idx_of(hstrip_t *h, lv_obj_t *chip)
{
    for (int i = 0; i < STRIP_N; i++) {
        if (h->chip[i] == chip) return i;
    }
    return -1;
}

static void hstrip_fill(hstrip_t *h)
{
    for (int i = 0; i < STRIP_N; i++) {
        int idx = h->first + i;
        /* ★ 环绕：末尾后面接回开头，这样往右滑到底能直接看到第一项，
         *   不用「松手弹回去」那一下 —— 兰兰说的「向右边滑动选择」。*/
        if (h->n > 0) idx = (h->first + i) % h->n;
        hstrip_show(h, i, idx);
    }
}

static void hstrip_pick(lv_event_t *e)
{
    hstrip_t *h = (hstrip_t *)lv_event_get_user_data(e);
    if (!h) return;
    int slot = hstrip_idx_of(h, lv_event_get_target(e));
    if (slot < 0 || slot >= h->vis) return;
    int idx = (h->n > 0) ? (h->first + slot) % h->n : -1;
    if (idx < 0) return;
    if (h->on_pick) h->on_pick(idx);
}

/* ============================================================
 *  ★★★ 第二十次：左「←」右「→」翻页按钮（重写，位置是本轮最大 bug）
 * ------------------------------------------------------------
 *  兰兰 10-04 原话：「主页中间有个向右的箭头按键挡住了滑动条栏目中间的按键，
 *                    栏目左右滑动无效，所以无法滑动到伴音这个项目进行试验」
 *
 *  —— 这两条是**同一个 bug 的两面**，而且是我 10-04 上一版亲手造的：
 *
 *  ❶ 【箭头压在胶囊上】上一版把按钮建在 cont 【外面】的同一个坐标带
 *      （x 与 x+w-30，各 30 宽），而胶囊也在这个带里、还更宽。
 *      ⇒ 兰兰看到的「中间那个向右箭头」就是 btn_next，
 *      它正好压住第 5 颗胶囊 ⇒ 点不到。而 btn_prev 压住第 1 颗。
 *
 *      屏幕上只有 7 颗可见（STRIP_N），去掉被压的两颗 → 可点的只剩 5 颗，
 *      「伴音」在第 10 项，滑不到。
 *
 *  ❷ 【两页共用一个 s_page_h】上一版用一个文件级 `s_page_h` 存「当前是哪条」，
 *      而 hstrip_create 被调用了**两次**（首页栏目条 + 发现页地区条），
 *      后一次把前一次覆盖 ⇒ **首页的箭头翻的是发现页的地区条**。
 *      所以首页按箭头，画面上毫无反应。
 *      ⇒ 这就是「栏目左右滑动无效」的第一层原因，跟手势一点关系都没有。
 *
 *  ⇒ 本轮三处一起改，缺一不可：
 *     ① 方向改用 user_data 携带（把 step 塞进一个小结构），不再靠全局变量
 *     ② 按钮【移出胶囊带】：胶囊整体右移 34 px，让出左侧一条 34 宽的翻页区
 *     ③ 翻页后强制把 cont 的 scroll 归零（虚拟条不用真滚动，见下）
 */
typedef struct { hstrip_t *h; int step; } hpage_t;
static void hstrip_sync_scroll(hstrip_t *h);   /* 前向声明：下面两个回调会调它 */

static void hstrip_page(lv_event_t *e)
{
    hpage_t *pp = (hpage_t *)lv_event_get_user_data(e);
    if (!pp || !pp->h || pp->h->n <= 0) return;
    hstrip_t *h = pp->h;
    h->first = (h->first + pp->step + h->n * 2) % h->n;
    hstrip_fill(h);
    hstrip_sync_scroll(h);
}

/* 按钮翻完页后把容器 scroll 拨到 0。
 * ★ 为什么强制归零、而不是拨到 h->first*step：
 *   这条是【虚拟条】—— 屏上永远只有 STRIP_N 颗胶囊，靠 h->first 换内容。
 *   cont 本身【不需要真的滚出去】；让它保持 scroll_x = 0，
 *   才能保证「胶囊 x=0 起排」这条布局假设永远成立。
 *   上一版拨到 h->first*step，等于自己把胶囊推出可视区，
 *   于是「翻页后看起来更空了」—— 兰兰说的「箭头在中间」也有这个成因。
 *   滚动条想表达进度，改用首项高亮（选中态）来表达，见 hstrip_fill。*/
static void hstrip_sync_scroll(hstrip_t *h)
{
    if (h->cont) lv_obj_scroll_to_x(h->cont, 0, LV_ANIM_OFF);
}

static void hstrip_scroll_cb(lv_event_t *e)
{
    hstrip_t *h = (hstrip_t *)lv_event_get_user_data(e);
    if (!h || h->n <= 0) return;
    int step = h->cw + 5;
    /* ★ 第二十次：滚动中只做一件事 —— 累计位移超过半颗就翻一页，
     *   然后【立刻把 scroll 拨回 0】。
     *   上一版是 f = scroll_x / step 直接当 first，但 scroll_x 是弹性动画的量，
     *   与「一颗 = step」并不严格对应（一屏放不下 7 颗时更不成立）
     *   ⇒ first 算出来经常是 0，看起来就是滑不动。
     *   ⚠️ 防抖：拨回 0 会再触发一次 SCROLL，靠 sx==0 直接 return 挡住。*/
    int sx = lv_obj_get_scroll_x(h->cont);
    if (sx > -step && sx < step) return;      /* 不足半颗，不动 */
    int dir = (sx > 0) ? +1 : -1;              /* 右滑（内容右移）= 看后面 */
    h->first = (h->first + dir + h->n) % h->n;
    hstrip_fill(h);
    lv_obj_scroll_to_x(h->cont, 0, LV_ANIM_OFF);
}

/* ============================================================
 *  hstrip_create —— 建一条【虚拟胶囊条】
 * ------------------------------------------------------------
 *  ★★ 第二十次重写（10-04）：从「真滚动容器」改成「定点翻页」
 *
 *  为什么彻底不靠手势了（兰兰三轮反馈：地区 8 个 / 栏目滑不动 / 箭头挡胶囊）：
 *   这条屏是 480×320 + FT6336，横滑在这套组合上**时灵时不灵**，
 *   而我又拿不到「到底为什么」的日志（LV_ASSERT 在 release 下是空的）。
 *   ⇒ 改成**只靠确定性按钮**：左「←」右「→」，点一下内容整体移一格。
 *      手势仍保留（想滑就滑），但不再是指望它。
 *
 *  ★★ 版面账（10-04 真机日志逼出来的，原设计是错的）：
 *   上一版把两个箭头放在 cont 外面但【坐标落在胶囊带里】，
 *   正好压住第 5 颗 ⇒ 兰兰看到的「主页中间那个向右箭头」。
 *   本轮箭头【独占左侧 HSTRIP_ARROW_W】，胶囊带整体右移。
 *   ⚠️ 但真机日志立刻报出副作用：
 *       hstrip: n=14 cw=80 vis=4   ← 栏目条只剩 4 颗（原来 7）
 *       hstrip: n=42 cw=64 vis=5   ← 地区条只剩 5 颗
 *   原因：cw 是写死的 80/64，扣掉箭头区就放不下 7 颗了。
 *   ⇒ 修法：**胶囊宽度按可用宽反算**，而不是写死。
 *      cw = (可用宽 / 目标颗数) - 缝，目标 6 颗 → cw 57。
 *      「新闻综合」四个字 14px 约 56 px，57 刚好放得下（LV_LABEL_LONG_DOT
 *      兜底），比原来 80 少 23 px，换来多两颗。
 */
/* ★★ 2026-10-04 第二十一次「箭头挡住一个栏目/地区」的修法与账：
 *
 *   兰兰报「首页和发现页的箭头都挡住了一个按键，很容易误触到栏目」。
 *   真因是【声明的宽度 ≠ 实际画出来的总宽】：
 *     上一版 HSTRIP_ARROW_W 写 34，但实际画了两个 30 宽的按钮（x 和 x+32）
 *     ⇒ 实际占 62 px；而胶囊带只从 x+36 开始
 *     ⇒ 右边那个箭头有 26 px 压在胶囊带上。
 *     开机日志 `vis=6` 看着完全正常，所以拖了几轮才发现。
 *
 *   本轮的取舍（480 宽放不下所有想要的东西，只能挑）：
 *     箭头 26×26（放得下 14px 箭头字，好按）⇒ 预留 26×2+2+2 = 56 px
 *     胶囊带 cont = 408－56 = 352；step = 352/6 = 58；cw = 58－5 = **53**
 *     同屏 **6 颗**（6×58 = 348 ≤ 352，余 4 px）⇒ 不因修箭头而少一颗。
 *     cw_min 由 56 降到 **48**：栏目短名最长 2 字（「新闻」「境外」），
 *     14px 两字约 28 px，48 绰绰有余；「新闻综合」这种全名只在全屏页出现。
 *   ⇒ 另加运行期断言日志（arrows occupy / reserved / OK），下次再撞直接看日志。
 *
 * ★★ 2026-10-04 第二十二次：箭头改【绿色 + 加大到 32 px + 18 px 箭头字】
 *
 *   兰兰：「首页栏目箭头按键太小，不容易发现，可以设置绿色箭头显示直观点」
 *
 *   为什么原来「小」不只是尺寸问题：11 px 的 ← → 是【次要灰白】，
 *   视觉权重跟胶囊上的文字一模一样，一眼扫过去就滑过去了 ——
 *   尺寸只是表象，【颜色不显眼】才是「不容易发现」的根因。
 *   ⇒ 三个维度一起改：
 *     ① 尺寸 26 → 32（更好按）
 *     ② 字  11 px(F_TINY) → 18 px(F_H2)
 *     ③ 色  C_TEXT(白) → C_GREEN(拾声绿)，与所有可交互元素同色系
 *
 *   ★ 横向账（480 宽，CONT 424，写死坐标前先算，别叠）：
 *     箭头区 = 32×2 + 2（缝）+ 2（余） = **68**
 *     胶囊带 cont = 408 － 68 = 340
 *     step = 340 / 6 = 56　　cw = 56 － 5 = **51**
 *     同屏 6 颗：6×56 = 336 ≤ 340（余 4 px）✓ 不因加大箭头而少一颗
 *   ⚠️ 箭头字必须用 F_H2(18px)：32 px 的方块配 11 px 字会显得头重脚轻。
 *   ⚠️ ◀▶ 实测 SimHei/Deng 都没有这两个字形（会变方块），
 *     所以只能用 ← →，而 F_H2 走主字库，这两个字肯定有。*/
#define HSTRIP_ARROW_W  26      /* 单个箭头边长（★ 2.8：32→26，屏窄要省）*/
#define HSTRIP_ARROW_GAP 2      /* 两箭头之间的缝 */
#define HSTRIP_BAND_X   (HSTRIP_ARROW_W * 2 + HSTRIP_ARROW_GAP + 2)  /* = 56 */
#define HSTRIP_TARGET_N  4      /* 目标同屏颗数（★ 2.8：5→4）
                                 *   为什么不是 5：内容区 272 减去箭头区 56 = 216，
                                 *   5 颗 ⇒ 每颗 38 px（扣缝只余 32），而「内蒙古」「黑龙江」
                                 *   是 3 个汉字 = 42 px ⇒ 必被 DOT 截成「内…」。
                                 *   4 颗 ⇒ 每颗 49 px（可用 43），3 字省名放得下。
                                 *   箭头仍在，翻页只是多点一下 —— 可读性优先。*/

static void hstrip_create(hstrip_t *h, lv_obj_t *parent, int x, int y,
                          int w, int hgt, int cw_min, int n, void (*on_pick)(int))
{
    memset(h, 0, sizeof(*h));
    h->n = n;
    h->on_pick = on_pick;

    /* 胶囊带：从箭头右侧开始，宽度扣掉箭头区 */
    int cx = x + HSTRIP_BAND_X;
    int cw_total = w - HSTRIP_BAND_X;

    /* ★ 胶囊宽度按可用宽反算：先按目标颗数算 step，再留 5 px 缝给每颗 */
    int step  = cw_total / HSTRIP_TARGET_N;
    int cw    = step - 5;
    if (cw < cw_min) {                 /* 太窄就退而求其次，按最小宽度排 */
        cw    = cw_min;
        step  = cw + 5;
    }
    h->cw = cw;

    h->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(h->cont);
    lv_obj_set_pos(h->cont, cx, y);
    lv_obj_set_size(h->cont, cw_total, hgt);
    lv_obj_set_style_bg_opa(h->cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(h->cont, 0, 0);
    /* ★ 保留 SCROLLABLE 让手势仍可用，但 scroll_x 由 sync_scroll 钉在 0 */
    lv_obj_add_flag(h->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(h->cont, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(h->cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(h->cont, hstrip_scroll_cb, LV_EVENT_SCROLL, h);

    /* 实际能放几颗：算出来，不写死（少一颗也不能画出 cont 外）*/
    int fit = (cw_total + 5) / step;
    if (fit > STRIP_N)     fit = STRIP_N;
    if (fit < 1)           fit = 1;
    h->vis = fit;

    for (int i = 0; i < STRIP_N; i++) {
        lv_obj_t *c = box(h->cont, i * step, 2, cw, hgt - 4, C_PANEL, (hgt - 4) / 2);
        lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
        /*★★★ 10-04 第二十六次：这两行 clear_flag 就是「触摸滑不动」的根因，
         *   兰兰报「首页栏目触摸滑动，发现页触摸滑动」——两条全都不响应，
         *   而 ← → 箭头一直好用（他没抱怨箭头）。
         *
         * 【为什么箭头能用、滑动不能】这个组合曾是最大的谜。查 LVGL 源码才定案：
         *   lv_indev_scroll.c:289~340  lv_indev_find_scroll_obj()
         *       obj_act = 按到的那个对象（胶囊 c，不是 cont）
         *       while(obj_act) {
         *           if(!lv_obj_is_scrollable(obj_act)) {
         *               if(!lv_obj_is_scroll_chain_hor(obj_act) && hor_en) break; ← ★
         *               obj_act = lv_obj_get_parent(obj_act); continue;
         *           }
         *           ...
         *       }
         *   ★ 第 335 行：胶囊不可滚动时，若 scroll_chain_hor 为 false 就【break】，
         *     【不再往上找父容器】⇒ cont 永远拿不到 scroll_obj ⇒ 手势 100% 失效。
         *   而箭头是 box() 建的、没清 chain_hor、也不依赖滚动 ⇒ 一直好用。
         *
         * 【初值】lv_obj.c:1505  obj->scroll_chain_hor = 1;  ← LVGL 默认【开】
         * 【我做错的】lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
         *            把默认的 1 清成了 0。
         *
         * ★ 我在 container 层（LV_DIR / spacer / scroll 回调）来回查了三轮都没查到，
         *   因为根因在【子对象的一个标志位】上 —— 层级选错了。
         *   速查卡铁律 21 记的「LV_DIR_NONE 不抢滚动」方向是对的，
         *   但那只覆盖了容器自己的 scroll_dir，漏了「子对象能否把滚动交回父容器」这一层。
         *
         * ⚠️ 铁律补充：**清 LVGL 的「默认开启」标志之前，先去源码确认它的默认值。
         *   默认 1 的东西被你 clear 成 0，症状是「某个手势整类失效」，
         *   而日志里什么异常都看不到。*/
        lv_obj_set_style_shadow_width(c, 0, 0);
        if (i < h->vis) {
            lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(c, hstrip_pick, LV_EVENT_CLICKED, h);
        } else {
            /* 放不下的胶囊彻底不可点、不占视觉 —— 不靠透明躲，
             * 因为透明对象照样吃触摸（10-04 又踩过一次这个坑）。*/
            lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        }
        h->chip[i] = c;
        h->txt[i]  = label(c, 0, 0, F_BODY, C_TEXT, "");
        lv_obj_set_width(h->txt[i], cw - 6);
        lv_label_set_long_mode(h->txt[i], LV_LABEL_LONG_DOT);
        lv_obj_center(h->txt[i]);
    }
    /* spacer 只作内容宽度的量尺：宽 = 全部 n 项，x 从 0 起，透明且不可点。*/
    h->spacer = box(h->cont, 0, 0, n * step, hgt - 4, C_BG, 0);
    lv_obj_set_style_bg_opa(h->spacer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(h->spacer, LV_OBJ_FLAG_CLICKABLE);

    hstrip_fill(h);

    /* ---- 左「←」右「→」：**独占左侧一条**，胶囊带整体右移，绝不重叠 ----
     *  ⚠️ 箭头用 ← →（SimHei/Deng 有字形）；◀▶ 实测没有，会变方块。
     *
     *  ★★ 2026-10-04 第二十一次修「箭头挡住一个栏目/地区」：
     *   兰兰报「首页和发现页的箭头都挡住了一个按键，很容易误触」。
     *   真因是【算账漏了】：
     *     上一版把 HSTRIP_ARROW_W 写成 34，但实际画了两个 30 宽的按钮
     *     （x 和 x+32）⇒ 实际占用 62 px；而胶囊带只从 x+36 开始
     *     ⇒ 右边那个箭头有 26 px 压在胶囊带【上面】。
     *     日志里 vis=6 看着正常，所以一直没发现。
     *   ⇒ 修法：HSTRIP_BAND_X = 箭头真实总宽 + 缝 + 余量 = 26×2+2+4 = 58，
     *     胶囊带从 x+58 起，两个箭头严格落在 x..x+56 内，永不重叠。*/
    static hpage_t s_pp[4];      /* 每条两个按钮各一份上下文 */
    static int     s_pp_n = 0;
    if (s_pp_n + 2 > 4) s_pp_n = 0;
    hpage_t *p_prev = &s_pp[s_pp_n++];
    hpage_t *p_next = &s_pp[s_pp_n++];
    p_prev->h = h; p_prev->step = -1;
    p_next->h = h; p_next->step = +1;

    int by = y + (hgt - HSTRIP_ARROW_W) / 2;
    h->btn_prev = box(parent, x, by, HSTRIP_ARROW_W, HSTRIP_ARROW_W, C_HILITE, 6);
    h->btn_next = box(parent, x + HSTRIP_ARROW_W + HSTRIP_ARROW_GAP, by,
                      HSTRIP_ARROW_W, HSTRIP_ARROW_W, C_HILITE, 6);
    /* ★ 第二十二次：箭头字 11 px → 18 px、颜色 白 → 绿（兰兰：「箭头太小不容易发现，
     *   可以设置绿色箭头显示直观点」）。字号与边长一起算：18 px 的箭头字放进
     *   32 px 的方块里居中，视觉上才「像按钮」而不是「像一个字符」。*/
    lv_obj_t *t1 = label(h->btn_prev, 0, 0, F_H2, C_CYAN, "←");
    lv_obj_t *t2 = label(h->btn_next, 0, 0, F_H2, C_CYAN, "→");
    lv_obj_center(t1);
    lv_obj_center(t2);
    /* 绿字在深灰底（C_HILITE）上对比度够，但为了「一眼看见」，
     * 再给方块描一圈绿边 —— 边界清楚了才知道那是个可按的东西。*/
    lv_obj_set_style_border_width(h->btn_prev, 1, 0);
    lv_obj_set_style_border_color(h->btn_prev, C_CYAN, 0);
    lv_obj_set_style_border_width(h->btn_next, 1, 0);
    lv_obj_set_style_border_color(h->btn_next, C_CYAN, 0);
    lv_obj_add_flag(h->btn_prev, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(h->btn_next, LV_OBJ_FLAG_CLICKABLE);
    /* ★ 箭头不挂 ev_press/ev_release（按下改 opa 会触发重排，
     *   而重排会让 LVGL 把这次按下判成滚动 —— 20-04 跳转键「要碰几次」的同款坑）*/
    lv_obj_set_scroll_dir(h->btn_prev, LV_DIR_NONE);
    lv_obj_set_scroll_dir(h->btn_next, LV_DIR_NONE);
    lv_obj_add_event_cb(h->btn_prev, hstrip_page, LV_EVENT_CLICKED, p_prev);
    lv_obj_add_event_cb(h->btn_next, hstrip_page, LV_EVENT_CLICKED, p_next);
    /* ★ 记账断言：箭头实际占的宽度必须 ≤ 预留区，否则一定压胶囊。
     *   这条是本轮 bug 的教训 —— 别再手算版面了，让机器算。*/
    int arrows_real = HSTRIP_ARROW_W * 2 + HSTRIP_ARROW_GAP;
    ESP_LOGW(TAG, "hstrip: arrows occupy %d px, reserved %d px (margin %d) %s",
             arrows_real, HSTRIP_BAND_X, HSTRIP_BAND_X - arrows_real,
             (arrows_real <= HSTRIP_BAND_X) ? "OK" : "!! OVERLAP !!");
    ESP_LOGI(TAG, "hstrip: n=%d cw=%d(min %d) vis=%d step=%d cont=%d wide",
             n, cw, cw_min, h->vis, step, cw_total);
}

/* ============================================================
 *  切页
 * ============================================================ */
/* 导航栏文字：选中亮绿 / 未选暗绿（第二十二次，见 nav_build 的注释）*/
#define C_NAV_DIM  lv_color_hex(0x4E5A66)   /* 未选中导航（中性暗灰）*/

static const uint32_t s_page_bg[PAGES_N] = {
    0x0B0E12, 0x0B0E12, 0x0B0E12, 0x0A0906, 0x0B0E12, 0x0B0E12, 0x0B0E12,
    0x0B0E12
};

/* ★ 第 8 页 WiFi 设置的建/销（定义在文件后段 3500 多行）。
 *   goto_page 在下面就要调 wifi_page_destroy，所以声明必须放在这里 ——
 *   放在 1700 多行的声明段里就晚了，gcc 会报 implicit declaration。*/
static void wifi_page_destroy(void);
static void night_page_destroy(void);   /* v1.35：定义在 page_night_build 之后 */
static void night_page_ensure(void);
static void grid_pages_destroy(int page_idx);   /* v1.40：三套网格按需建/离开删 */
static void grid_pages_ensure(int page_idx);
/* ★ v1.40：下面这三个是 grid_pages_ensure 的实现要用到的，
 *   但它们都定义在 goto_page 之后（1719/2762 行往后）。
 *   本工程 -Werror ⇒ 缺声明会直接编译失败，注释得写清楚为什么。*/
static st_grid_t *grid_create_ex(lv_obj_t *parent, int x, int y, int w, int h,
                                 int vis_rows, int cols);
static void on_grid_play(int idx);
static void on_grid_fav(int idx);
static void home_grid_refresh(void);
static void find_grid_refresh(void);
static void list_page_refresh(void);   /* v1.40：case 6 建完网格要刷内容 */
static void clock_cb(lv_timer_t *t);       /* 同上：定义在文件后段 */
static void clock_date_cb(lv_timer_t *t);
static void sleep_left_cb(lv_timer_t *t);
static void night_state_cb(lv_timer_t *t);

static void goto_page(int idx)
{
    if (idx < 0 || idx >= PAGES_N || idx == s_cur_page) return;
    /* ★ 离开状态页就把关机键的「再点一次」清掉（10-04）：
     *   否则用户逛一圈回来，按钮还写着「再点一次关机」，
     *   一下就真关了 —— 二次确认反而变成更危险的坑。*/
    if (s_cur_page == 4 && idx != 4 && s_sdconfirm_timer) {
        lv_timer_del(s_sdconfirm_timer);
        s_sdconfirm_timer = NULL;
        if (s_lbl_power) {
            lv_label_set_text(s_lbl_power, "关机");
            lv_obj_set_style_text_color(s_lbl_power, C_RED, 0);
        }
    }
    /* ★★ 离开第 8 页（WiFi 设置）就把整页对象删掉，把 LVGL 池还回去。
     *   第二十二次真机崩在这：常驻时建页开销 17 KB，池 free 从 26.9 KB
     *   掉到 9.5 KB（used 91%），splash 一撤、main 屏开始渲染就崩在
     *   lv_draw_add_task —— 那里 lv_malloc 返 NULL，而 release 构建下
     *   LV_ASSERT_MALLOC 是空操作，于是往 NULL 里写，EXCVADDR=0x08。
     *   ⚠️ 钩子必须挂在 goto_page 里而不是返回键上：万一用户是从左边
     *   导航栏切走的，只挂返回键就会漏掉，池会一直缺这一块。*/
    if (s_cur_page == 7) wifi_page_destroy();
    /* ★★ v1.35：夜间页（第 4 页）同样「离开就删」。
     *   兰兰真机报「点击睡眠页就重启」＝ 这一页一显示就把池抽干，
     *   崩在 lv_draw_add_task（lv_malloc 返 NULL，release 下断言是空操作）。
     *   它比第 8 页更重，所以这轮【必须】做，不能拖。*/
    if (s_cur_page == 3) night_page_destroy();
    /* ★★★ v1.40：三套电台网格也改成「进页才建、离开就删」。
     *
     *   10-05 兰兰报「测试中经常重启」。抓日志抓到确切现场：
     *     Guru Meditation: StoreProhibited, EXCVADDR=0x00000008
     *     Backtrace: lv_draw_add_task ← lv_draw_label ← draw_main ← ...
     *     崩前最后两行正是「XSUI: page -> 2」「XSUI: tap: 导航」
     *   ★ 这与第 1502 行注释里记的【第二十二次】是【同一个病】，
     *     只是这次触发点从第 8 页换成了首页 —— 症状一字不差
     *     （lv_malloc 返 NULL，release 下 LV_ASSERT_MALLOC 是空操作，
     *      紧接着往 NULL 里写第一个字段 ⇒ 偏移 8）。
     *
     *   为什么必须动网格：开机实测池 free 只剩 14728 B（used 86%），
     *     而三套网格 9+12+24 = 45 格 × 2 对象 = 90 个 LVGL 对象，
     *     常驻约 21 KB ＝ 池的 21%。绘制任务（draw task）是在
     *     lv_display_refr_timer 里【临时】分配的，渲染完才释放，
     *     所以「对象建得下」不等于「重绘时分配得下」——
     *     切页那几毫秒里大批对象同时标脏 + 重绘范围大，就撞上了。
     *     ⇒ 表现为「偶尔」而不是「每次」，是概率性的。
     *
     *   修法与夜间页/WiFi 页同一套机制：离开即删、进页再建。
     *   顺序同样关键：先删旧页把池还回来，再建新页。
     */
    /* ★★ 10-10：首页（0）已从这套「进页才建、离开就删」的名单里摘掉 ——
     *   它不再有电台网格了（见 grid_pages_ensure 的 case 0 注释）。*/
    if (s_cur_page == 1 || s_cur_page == 6)
        grid_pages_destroy(s_cur_page);
    /* 建页放在清完页之后、显示之前：确保 s_pages[idx] 刚建出来就渲染。
     * ⚠️ 顺序有讲究：先删旧页把池还回来，再建新页 —— 反过来会短时间
     *   同时持有两页，正是最容易被池卡死的时刻。*/
    if (idx == 3) night_page_ensure();
    if (idx == 1 || idx == 6) grid_pages_ensure(idx);   /* ★ 10-10：首页（0）已摘 */
    for (int i = 0; i < PAGES_N; i++) {
        lv_obj_set_style_bg_color(s_pages[i], lv_color_hex(s_page_bg[i]), 0);
        lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(s_pages[idx], LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < NAV_N; i++) {
        bool on = (i == idx);
        lv_obj_set_style_bg_opa(s_nav_btns[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(s_nav_btns[i], C_HILITE, 0);
        lv_obj_set_style_opa(s_nav_marks[i], on ? LV_OPA_COVER : LV_OPA_40, 0);
        /* ★ 文字颜色也要跟着切。原来只在建的时候设一次，
         *   于是「首页」永远是亮绿、「状态」永远是灰的 —— 切页后不更新。*/
        if (s_nav_labels[i]) {
            lv_obj_set_style_text_color(s_nav_labels[i], on ? C_RED : C_NAV_DIM, 0);
        }
    }
    s_cur_page = idx;
    ESP_LOGI(TAG, "page -> %d", idx);
}

/* 由格子控件反查它是第几号槽位。
 * ★ 为什么不把槽位号塞进 user_data：LVGL 一个 user_data 只能带一个指针，
 *   而这里指针已经被 st_grid_t* 占了。直接拿事件目标比对指针最省事。*/
static int grid_slot_of(st_grid_t *g, lv_obj_t *cell)
{
    for (int i = 0; i < g->nslot; i++) {
        if (g->cell[i] == cell) return i;
    }
    return -1;
}

/* 一格显示的台；station_idx < 0 = 空 */
static void grid_show_cell(st_grid_t *g, int slot, int station_idx)
{
    g->idx[slot] = station_idx;
    lv_obj_t *c = g->cell[slot];
    if (station_idx < 0) {
        /* ★★★★★★ 10-05 v1.39 修【背后露出上一页电台名】
         *   兰兰原话：「点海外 10 个地区页应该后面不要出现其他电台名称，
         *     会有其他页面的电台露出，点击具体海外地区电台页，也会露出背后其他地区电台」
         *
         *   根因：清空格子时**只把底色设为透明，没清 label 的文字**。
         *     原来这里只有一句 `lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);`
         *     就 return 了 —— 底子透明了，可 nm[slot] 里上一屏的
         *     台名/地区名还留着，孤零零地浮在背景上。
         *   为什么「每页都露」而不是偶尔露：
         *     这一层是【虚拟网格】，nslot 固定 24 格（8 行×3 列），
         *     而 pool 只有 11~20 条 ⇒ grid_fill 里 pos >= total 的格子
         *     全部走 station_idx < 0 这条分支 ⇒ 每次切换都有十几格残留。
         *     海外地区页 11 格 → 进「欧洲」可能只有 8 个台 ⇒ 后面 16 格全露。
         *   ⇒ 修法：底色透明 **＋ 文字清空 ＋ 恢复默认文字色**
         *     （文字色也要重置：地区块模式写过 C_AMBER，
         *       不还原的话空格子留着琥珀色，下一屏正常台名会变琥珀）。
         *   ⚠️ 必须清 nm[slot]，不能用 lv_obj_get_child(c,0)：
         *     格子第一个子对象是 box 不是 label（见下方 mode==0 的注释）。*/
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        if (g->nm[slot]) {
            lv_label_set_text(g->nm[slot], "");
            lv_obj_set_style_text_color(g->nm[slot], g->fg, 0);
            /* ★ 10-11：字体也要复位。本地音频格会把字体临时切成 F_SD
             *   （大字符集），而格子是循环复用的 —— 不复位的话，
             *   「翻到上一页的本地条目、这一页第一个格子是空的」这种
             *   搭配会让空格的字体留到大字符集上，下一屏台名跟着变样。
             *   这跟上面复位颜色是同一个理由（10-05 踩过色的那一半）。*/
            lv_obj_set_style_text_font(g->nm[slot], g->font, 0);
        }
        return;
    }
    /* ★ 用建时存下的句柄，不要 lv_obj_get_child(c, 0) ——
     *   格子里的第一个子对象是「收藏小点」(box)，对它调 lv_label_set_text
     *   会按 label 的布局结构解释一个 box 的指针 ⇒ LoadProhibited 崩在
     *   lv_free_core。（10-03 踩过，回溯：grid_show_cell → set_text_internal）*/
    char buf[80];
    /* ★★★ 10-11：本地音频格（编码见 HIST_LOCAL_BASE 的注释）。
     *  ⚠️ 必须放在 mode==1 判断【之前】、`station_idx < 0` 判断【之后】：
     *     前者是因为编码值同时也是正数，后者是因为空格外只认负数。
     *  ⚠️ 这一段里【绝不能】调 app_st_get() / app_fav_has()：
     *     那都是拿"本地条目序号"当台下标去查台单 —— 轻则恒 false，
     *     重则越界读 g_stations[]。*/
    if (station_idx >= HIST_LOCAL_BASE) {
        const char *fp = app_resume_path_at(station_idx - HIST_LOCAL_BASE);
        const char *bs = (fp != NULL) ? strrchr(fp, '/') : NULL;
        bs = (bs != NULL) ? bs + 1 : fp;          /* 只显示文件名，不显示路径 */
        utf8_copy_n(buf, sizeof(buf), (bs && bs[0]) ? bs : "?", 8);
        if (g->nm[slot]) {
            lv_label_set_text(g->nm[slot], buf);
            /* ★★ 字体必须临时切到 F_SD（xs_font_cjk12，GB2312 一级 3755 字）。
             *   网格默认字体是 F_ST14 → 2.8 上就是 xs_font_st16，
             *   那是【按台单用字烘的 871 码点子集】—— 本地播客文件名里的字
             *   基本都不在里面，不切字体就是一整排方块（铁律 22）。*/
            lv_obj_set_style_text_font(g->nm[slot], F_SD, 0);
            /* 青色 = 「这条是本机内存卡上的音频」，跟电台格（白/绿）区分开 */
            lv_obj_set_style_text_color(g->nm[slot], C_CYAN, 0);
        }
        /* 正在放的就是它 ⇒ 高亮。
         * 本地文件没有"台下标"，只能用显示名比 —— app_radio_now_playing()
         * 对本地文件给的就是文件名，正好和这里显示的同一个串。*/
        bool cur = (!app_radio_is_stream() && app_radio_is_playing() &&
                    bs != NULL && strcmp(bs, app_radio_now_playing()) == 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(c, cur ? g->bg_sel : g->bg, 0);
        return;
    }
    if (g->mode == 1) {
        /* ★ mode==1：这一格不是台，是【地区块】（海外二级分类用）。
         *   pool 里存的是地区下标，idx 也是。这里【不能调 app_fav_has()】——
         *   拿地区下标当台下标去问收藏，轻则恒 false，重则越界读。*/
        int pv = station_idx;
        if (pv >= 0 && pv < NET_PROV_N) {
            int cnt = 0, tot = app_radio_station_count();
            for (int i = 0; i < tot; i++) {
                if (app_st_get(i)->prov == pv) cnt++;
            }
            snprintf(buf, sizeof(buf), "%.12s · %d", g_prov_name[pv], cnt);
        } else {
            snprintf(buf, sizeof(buf), "?");
        }
        if (g->nm[slot]) lv_label_set_text(g->nm[slot], buf);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(c, g->bg, 0);
        /* 地区块用琥珀字，跟地区条里那些海外胶囊同一个色系 */
        if (g->nm[slot]) {
            lv_obj_set_style_text_color(g->nm[slot], C_AMBER, 0);
            /* ★ 10-11：字体复位（同上，格子是复用的）*/
            lv_obj_set_style_text_font(g->nm[slot], g->font, 0);
        }
        return;
    }
    /* ★ 10-05 三列：台名截断 13 → 8 字。★★ 2.8 再降到 7 字：
     *   cell_w=129 px、字号 16px（st16）⇒ 一个汉字 16 px，8 字 = 128 px
     *   正好顶满格宽（一个字形的空隙都不剩，遇到「广」「圆」这类
     *   稍微宽一点的字就会被切边）。7 字 = 112 px，留 17 px 呼吸。*/
    utf8_copy_n(buf, sizeof(buf), app_st_get(station_idx)->name, 7);
    if (g->nm[slot]) lv_label_set_text(g->nm[slot], buf);
    /* 正在播的那台高亮 */
    bool cur = (station_idx == app_radio_station_current());
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, cur ? g->bg_sel : g->bg, 0);
    /* ★ 收藏标记用【台名颜色】表示，不另建一个小方块。
     *   绿 = 已收藏。30 个格子省下 30 个 lv_obj（每个约 250~400 B）——
     *   LVGL 池只有 98 KB，这一项就是 10 KB。*/
    if (g->nm[slot]) {
        lv_obj_set_style_text_color(g->nm[slot],
                                    app_fav_has(station_idx) ? C_GREEN : g->fg, 0);
        /* ★ 10-11：字体复位（格子循环复用，上一屏可能是本地音频格用的大字符集）*/
        lv_obj_set_style_text_font(g->nm[slot], g->font, 0);
    }
}

/* 按当前 first 重填所有格子 */
static void grid_fill(st_grid_t *g)
{
    for (int i = 0; i < g->nslot; i++) {
        int row = g->first + i / g->cols;
        int col = i % g->cols;
        int pos = row * g->cols + col;
        int st  = (pos < g->total) ? g->pool[pos] : -1;
        lv_obj_set_pos(g->cell[i], col * (g->cell_w + 6), row * g->row_h);
        grid_show_cell(g, i, st);
    }
}

static void grid_slot_clicked(lv_event_t *e)
{
    st_grid_t *g = (st_grid_t *)lv_event_get_user_data(e);
    if (!g) return;
    int slot = grid_slot_of(g, lv_event_get_target(e));
    if (slot < 0) return;
    int st = g->idx[slot];
    if (st < 0) return;
    /* ★ 记住「是从哪一层点进来的」：播放页的返回键要回这一页，
     *   而不是固定回首页（兰兰 10-03：「要返回刚刚在发现页选择的地区栏」）。*/
    s_play_from = g->from_page;
    /* mode==1：这一格是地区块，走地区回调（海外二级分类）*/
    if (g->mode == 1) {
        if (g->on_region) g->on_region(slot, st);
        return;
    }
    if (g->on_play) g->on_play(st);
}

/* 长按 = 收藏/取消。★ 必须自己把滚动按住不放时的高亮收掉，
 *   LVGL 的 LONG_PRESSED 在滑动时不会发，但按下时的压暗效果还在。*/
static void grid_slot_long(lv_event_t *e)
{
    st_grid_t *g = (st_grid_t *)lv_event_get_user_data(e);
    if (!g) return;
    int slot = grid_slot_of(g, lv_event_get_target(e));
    if (slot < 0) return;
    int st = g->idx[slot];
    if (st < 0) return;
    /* ★ 10-11：本地音频格没有"台下标"，收藏它没有意义（收藏表按台名存）。
     *   直接忽略，别让它走到 on_fav → app_fav_toggle(100000+i) 越界。*/
    if (st >= HIST_LOCAL_BASE) {
        lv_obj_set_style_opa(g->cell[slot], LV_OPA_COVER, 0);
        return;
    }
    /* mode==1：这一格是【地区块】不是台，收藏没有意义 ——
     *   而且 on_fav 会拿 idx 当【台下标】用，越界。必须在这里拦住。*/
    if (g->mode == 1) return;
    /* ★ 10-11：收藏页 / 历史页里长按 = 删掉这一条（兰兰要的「删单条」）。
     *   ⚠️ 删完【不能再往下走 grid_show_cell】：那一句是按「旧的 slot→st」
     *     去重画，而 on_del 已经把列表重排了，旧下标可能指向另一个台，
     *     画完屏幕上会留下一个「幽灵高亮」。⇒ 删这条路径直接 return，
     *     由 on_del 自己调 list_page_refresh() 整页重画。*/
    if (g->del_mode) {
        if (g->on_del) g->on_del(st);
        return;
    }
    if (g->on_fav) g->on_fav(st);
    grid_show_cell(g, slot, st);      /* 收藏标记要立刻变 */
}

/* 滚动：算出现在是第几行，变了才重填（否则每滚一个像素就重排 10 个 label）*/
static void grid_scroll_cb(lv_event_t *e)
{
    st_grid_t *g = (st_grid_t *)lv_event_get_user_data(e);
    if (!g || g->total <= 0) return;
    int rows = (g->total + g->cols - 1) / g->cols;
    int max_first = rows - g->vis_rows;
    if (max_first < 0) max_first = 0;
    int f = lv_obj_get_scroll_y(g->cont) / g->row_h;
    if (f < 0) f = 0;
    if (f > max_first) f = max_first;
    if (f == g->first) return;
    g->first = f;
    grid_fill(g);
}

/* 建一个网格。x/y/w/h = 容器在页面里的位置与可视区大小。
 * ★ 2.8：新增 cols 参数（3.5 固定三列，2.8 用两列，见 st_grid_t.cols 的注释）。
 *   cell_w 的算法也跟着改：3.5 是 `(w-6)/cols`（边距按 6 算一次），
 *   那只在 cols==3 时凑巧不溢出；正确算法是【先把列间缝扣掉再等分】：
 *     cell_w = (w - 缝×(cols-1)) / cols
 *   ⇒ 末列右缘 = (cols-1)×(cell_w+缝) + cell_w ≤ w，恒成立。*/
static st_grid_t *grid_create_ex(lv_obj_t *parent, int x, int y, int w, int h,
                                 int vis_rows, int cols)
{
    static st_grid_t s_grids[4];      /* 4 处用到：首页 / 发现 / 分类页（收藏共用它）*/
    static int s_gn = 0;
    if (s_gn >= 4) s_gn = 0;
    st_grid_t *g = &s_grids[s_gn++];

    memset(g, 0, sizeof(*g));
    if (vis_rows < 1) vis_rows = 1;
    if (vis_rows > GRID_ROWS) vis_rows = GRID_ROWS;
    if (cols < 1) cols = 1;
    if (cols > GRID_COLS) cols = GRID_COLS;
    g->vis_rows = vis_rows;
    g->cols     = cols;
    g->nslot    = vis_rows * cols;
    g->row_h    = h / vis_rows;
    g->cell_h   = g->row_h - 4;
    g->cell_w = (w - 6 * (cols - 1)) / cols;
    g->font   = F_ST14;
    g->bg     = C_PANEL;
    g->bg_sel = C_HILITE;
    g->fg     = C_TEXT;
    g->first  = 0;

    g->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(g->cont);
    lv_obj_set_pos(g->cont, x, y);
    lv_obj_set_size(g->cont, w, h);
    lv_obj_set_style_bg_opa(g->cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g->cont, 0, 0);
    lv_obj_add_flag(g->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g->cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g->cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(g->cont, C_LINE, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(g->cont, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(g->cont, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(g->cont, 2, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(g->cont, grid_scroll_cb, LV_EVENT_SCROLL, g);

    for (int i = 0; i < g->nslot; i++) {
        int col = i % g->cols;
        int row = i / g->cols;
        lv_obj_t *c = box(g->cont, col * (g->cell_w + 6), row * g->row_h,
                          g->cell_w, g->cell_h, g->bg, 8);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, ev_press,  LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(c, ev_release, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(c, grid_slot_clicked, LV_EVENT_CLICKED, g);
        lv_obj_add_event_cb(c, grid_slot_long, LV_EVENT_LONG_PRESSED, g);
        g->cell[i] = c;
        g->nm[i]   = label(c, 0, 0, g->font, g->fg, "");
        lv_obj_center(g->nm[i]);
    }

    /* spacer：LVGL 用子对象包围盒算可滚范围，而格子会随滚动被搬回顶部，
     * 所以必须有一个「永远在底部」的高对象把内容高度顶起来。*/
    g->spacer = box(g->cont, 0, 0, 1, 1, C_BG, 0);
    lv_obj_set_style_bg_opa(g->spacer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g->spacer, LV_OBJ_FLAG_CLICKABLE);
    return g;
}

/* 设定网格内容（pool 里是台下标），并回到顶部 */
static void grid_set(st_grid_t *g, const int *pool, int n)
{
    if (n > GRID_MAX) n = GRID_MAX;
    g->total = n;
    for (int i = 0; i < n; i++) g->pool[i] = pool[i];
    g->first = 0;
    lv_obj_scroll_to_y(g->cont, 0, LV_ANIM_OFF);

    int rows = (n + g->cols - 1) / g->cols;
    int content_h = rows * g->row_h;
    int min_h = g->vis_rows * g->row_h;
    if (content_h < min_h) content_h = min_h;
    lv_obj_set_size(g->spacer, 1, content_h);
    grid_fill(g);
}


/* ============================================================
 *  开机画面（splash）
 *
 * ★★ 2026-10-04 第二十八次：加「呼吸光带 + 颜色流转 + 问候语」，
 *   停留 1.2 s → 3.5 s。兰兰：「开机海报页显示时间稍微长一点，
 *   最好是动态的或者有颜色光影或者动态的变化，吸引年轻人使用」，
 *   但「UI 还是要保持深色底色」。
 *
 * ★★★ 为什么底色坚持纯黑（0x000000），不加任何深蓝/深紫：
 *   兰兰说的「深色底」在这块 ST77922 上有个硬约束 ——
 *   ★ 屏本身是 DIO 模式 + invert_color(true)（见 main 里的面板配置），
 *     颜色的「亮/暗」在 LCD 控制器里是反相算过的。
 *     在软件里调一个「很好看的深蓝」，到屏上可能被算成一片脏灰。
 *     纯黑是唯一可预测的深色：它反相后仍是最干净的边界。
 *   ⇒ 光影全部靠【前景元素】（环形光带、logo、字）去做，
 *     底色一个字节都不动。这是「动前景、不动背景」的做法。
 *
 *  三层动效，从慢到快：
 *   ① 3 个同心环：透明度呼吸（周期 2.4 s，相位错开 ⇒ 看起来像涟漪）
 *   ② 环的描边色在三个品牌色之间缓慢流转（周期 9 s）
 *   ③ logo 五根声波柱：高度轻微起伏（周期 1.1 s）
 *   ★ 三层都用同一个 60 ms 定时器推，不各自建定时器 ——
 *     省 3 个 lv_timer 对象，splash 期间 LVGL 池本来就紧。
 * ============================================================ */

/* 呼吸光带的几何（★ 屏是横版 480×320，但 LVGL 软旋转后逻辑分辨率
 *   仍是 LCD_H_RES=480 宽 / LCD_V_RES=320 高）*/
#define SPL_RING_CX   (LCD_H_RES / 2)     /* 240 */
#define SPL_RING_CY   118                 /* 环心 y：让下方文字排得下 */
#define SPL_RING_N    3
#define SPL_RING_MS   60                  /* 推一次动画的间隔 */
#define SPL_DWELL_MS  3500                /* ★ 停留 3.5 s（原 1200） */

/* 品牌三色（流转用）：绿 → 琥珀 → 青 → 绿
 * ★ 存 uint32_t 原始色值、用 lv_color_hex() 转，不直接写 lv_color_t 数组：
 *   lv_color_t 是个结构体（带 f_pred / f_succ 之类的附加字段），
 *   花括号逐字段初始化既容易漏字段、被编译器静默补 0，
 *   也会被 _c_lint.py 的「定长数组项数」检查数错。
 * ★★ 自查教训（写这段时被门禁拦了两次）：
 *   两次都是我的【注释里写了示例数组】，被门禁当成真数组去数项数。
 *   第一次还顺带暴露了 _c_lint.py 自己不剥注释的 bug（已修脚本）。
 *   ⇒ 规矩：注释里不要出现「名字[数字] = {」这种形状。 */
static const uint32_t k_spl_cycle_hex[3] = {
    0x1B9E77,     /* 拾声绿 */
    0xF0B429,     /* 琥珀   */
    0x3AA6C8,     /* 青     */
};

static lv_obj_t *s_spl_ring[SPL_RING_N];
static lv_obj_t *s_spl_bar[5];          /* logo 的五根柱 */
static lv_obj_t *s_spl_greet;           /* 问候语那一行 */
static uint32_t  s_spl_tick;            /* 累计的 tick 数（推导相位）*/
static lv_timer_t *s_spl_timer;

/* logo 五根柱的基准高度百分比（复用 logo_mark 那组 wgt）*/
static const int s_spl_wgt[5] = {40, 70, 100, 66, 46};

/* 问候语：按小时给一句。★ 全部用【现有字库一定有的字】——
 *   F_BODY 主字库 551 字，而这些字都在「早/上/好/午/晚/夜/间/听/个/音/乐/电/台」
 *   这类常用范围里。写生僻字会变方块（铁律 22）。*/
static const char *greeting_for_hour(int h)
{
    if (h < 5)  return "夜深了 · 听点什么";
    if (h < 9)  return "早上好 · 今天从声音开始";
    if (h < 12) return "上午好 · 慢一点";
    if (h < 14) return "中午好 · 歇一会儿";
    if (h < 18) return "下午好 · 来电台吧";
    if (h < 22) return "晚上好 · 夜里还长";
    return "夜深了 · 听点什么";
}

/* 一拍动画。所有对象都在 splash 上，splash 删了就停。*/
static void splash_anim_cb(lv_timer_t *t)
{
    (void)t;
    s_spl_tick++;

    /* ---- ① 环：透明度呼吸 ----
     * 用三角波而不是 sin：sin 要浮点运算，ESP32 上算 3 个环 × 60 Hz
     * 纯属浪费；三角波用取模 + 折返，几次整数运算就够，视觉差别看不出来。
     * 相位错开 r 档 ⇒ 三层像涟漪往外扩，而不是一起亮灭。*/
    const uint32_t PH = 40;              /* 一个呼吸周期 = 40 拍 = 2.4 s */
    for (int i = 0; i < SPL_RING_N; i++) {
        if (!s_spl_ring[i]) continue;
        uint32_t ph = (s_spl_tick + (uint32_t)i * (PH / SPL_RING_N)) % PH;
        int tri = (ph < PH / 2) ? (int)(ph * 2) : (int)((PH - ph) * 2);
        /* 透明度 20 ~ 85 呼吸 */
        int opa = 20 + (tri * 65) / (int)PH;
        lv_obj_set_style_border_opa(s_spl_ring[i], (lv_opa_t)opa, 0);
    }

    /* ---- ② 描边色流转（周期 9 s = 150 拍，30 拍一档）---- */
    uint32_t cslot = (s_spl_tick / 30) % 3;
    lv_color_t cc = lv_color_hex(k_spl_cycle_hex[cslot]);
    for (int i = 0; i < SPL_RING_N; i++) {
        if (!s_spl_ring[i]) continue;
        /* ★ 内环最亮、外环最暗 ⇒ 有纵深感。
         *   实现方式：内层用本色，外层用【底色与本色各半的混色】
         *   （lv_color_mix），而不是找一个「更暗的本色」——
         *   ★★ LVGL v9 没有 lv_obj_set_style_border_opa_dsc 这个函数
         *      （我写了才被 gcc 拦下来；只有 get_style_border_opa_internal）。
         *      想单独调描边透明度就用 lv_obj_set_style_border_opa()，
         *      但那个会和第 ① 层的呼吸动画互相覆盖 —— 两处都在写
         *      同一个属性，后写的赢 ⇒ 呼吸效果会消失。
         *   ⇒ 结论：呼吸走 opacity、纵深走颜色深浅，两个属性分开管。*/
        lv_color_t dim = lv_color_mix(cc, lv_color_hex(0x000000), LV_OPA_50 + (lv_opa_t)(i * 30));
        lv_obj_set_style_border_color(s_spl_ring[i], dim, 0);
    }

    /* ---- ③ logo 柱起伏（周期 1.1 s ≈ 18 拍）----
     * ★★ 幅度用【绝对像素】不用百分比 —— 10-04 差点写错：
     *   原本写「基准 ±22%」，离线验算出来实际幅度是 30~43%：
     *   三角波 0..(BP-1) 折返后 base+22%*tri/9-11% 并不是 ±11%，
     *   最矮那根（基准 13）被顶到 16，最高的（基准 34）冲到 **45** ——
     *   而 logo 容器只有 34 px，柱子会【顶出容器被裁掉】。
     *   ⇒ 改成「基准 ± 4 px」：幅度封顶 8 px，任何一根都不越界。
     *     容器高度也给到 40，34+4 有地方放。
     *   为什么不是 ±2 px：13 px 的柱动 2 px 只有 15% 变化，
     *   1.1 s 一周期在 60 ms 一帧下只差 1 px，看着像没动。
     *   4 px 是「看得见又不夸张」。*/
    const uint32_t BP = 18;
    for (int i = 0; i < 5; i++) {
        if (!s_spl_bar[i]) continue;
        uint32_t ph = (s_spl_tick + (uint32_t)i * 3) % BP;
        int tri = (ph < BP / 2) ? (int)(ph * 2) : (int)((BP - ph) * 2);
        int amp = (tri * 8) / (int)BP - 4;      /* -4 ~ +3 */
        int h = 34 * s_spl_wgt[i] / 100 + amp;
        if (h < 4)  h = 4;
        if (h > 40) h = 40;
        lv_obj_set_size(s_spl_bar[i], 4, h);
        lv_obj_set_pos(s_spl_bar[i], i * 6, (40 - h) / 2);
    }
}

static void splash_skip_cb(lv_event_t *e)
{
    (void)e;
    /* 触摸提前跳过：兰兰要「吸引人」但不能逼人看广告。
     * 延迟满 3.5 s 自动走；点一下立刻走。*/
    if (s_splash_timer) {
        lv_timer_del(s_splash_timer);
        s_splash_timer = NULL;
    }
    splash_done_cb(NULL);
}

static void splash_build(void)
{
    s_splash_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_splash_scr);
    lv_obj_set_style_bg_color(s_splash_scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_splash_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_splash_scr, LV_OBJ_FLAG_SCROLLABLE);
    /* ★ 整页可点 = 点击提前跳过。splash_scr 本身没有 CLICKABLE 标志，
     *   不加的话只有点到具体子对象才收得到事件。*/
    lv_obj_add_flag(s_splash_scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_splash_scr, splash_skip_cb, LV_EVENT_CLICKED, NULL);

    /* ---- 呼吸光带：3 个同心环 ---- */
    for (int i = 0; i < SPL_RING_N; i++) {
        /* ★ 环径 120/168/216，不是老版的 150/212/274。
         *   老版最大环 274 px、环心 y=118 ⇒ 上边落在 118-137 = **-19**
         *   ⇒ 被屏幕顶边裁掉一截，一直没人发现是因为它「空心描边」，
         *   少掉的那截在深色底上看着像本来就是那样。
         *   新账：最大环 216 ⇒ 纵向 10~226、横向 132~348，都在屏内。*/
        int d = 120 + i * 48;
        lv_obj_t *ring = box(s_splash_scr, SPL_RING_CX - d / 2,
                             SPL_RING_CY - d / 2, d, d,
                             lv_color_hex(0x0F1A18), d / 2);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(k_spl_cycle_hex[0]), 0);
        lv_obj_set_style_border_opa(ring, LV_OPA_50, 0);
        s_spl_ring[i] = ring;
    }

    /* ---- logo：自己建五根柱（要单独控制每一根的高度，所以不用 logo_mark）---- */
    {
        lv_obj_t *c = lv_obj_create(s_splash_scr);
        lv_obj_remove_style_all(c);
        /* 容器高 40，不是 34 —— 柱子在动画里会最高到 34+3=37，
         *   容器只给 34 的话柱子顶边会被裁掉一截，看着像断了一节。*/
        lv_obj_set_pos(c, SPL_RING_CX - 15, 86);
        lv_obj_set_size(c, 30, 40);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        for (int i = 0; i < 5; i++) {
            int bh = 34 * s_spl_wgt[i] / 100;
            s_spl_bar[i] = box(c, i * 6, (40 - bh) / 2, 4, bh, C_GREEN, 2);
        }
    }

    lv_obj_t *t = label(s_splash_scr, 0, 130, F_TITLE, C_RED, "拾声");
    lv_obj_set_width(t, LCD_H_RES);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *en = label(s_splash_scr, 0, 188, F_TINY, C_CYAN, "X I A N   D I A L");
    lv_obj_set_width(en, LCD_H_RES);
    lv_obj_set_style_text_align(en, LV_TEXT_ALIGN_CENTER, 0);

    /* ★ 问候语：按开机时刻给一句。放在英文名下面，
     *   比放在最下面（原来「正在准备…」那行）更醒目 ——
     *   兰兰要的是「吸引年轻人」，问候是整页唯一一句【对人说的话】。*/
    {
        int h = 12;
        int m = now_minutes();
        if (m >= 0) h = (m / 60) % 24;
        s_spl_greet = label(s_splash_scr, 0, 206, F_BODY, C_AMBER,
                            greeting_for_hour(h));
        lv_obj_set_width(s_spl_greet, LCD_H_RES);
        lv_obj_set_style_text_align(s_spl_greet, LV_TEXT_ALIGN_CENTER, 0);
    }

    box(s_splash_scr, (LCD_H_RES - 120) / 2, 234, 120, 1, C_LINE, 0);

    /* ★ 开机页署名也放大提亮（兰兰：「毕竟是推广作用的展示」）*/
    /* ★ 10-05 兰兰反馈「作者署名字体太大，挡住下面字」：
     *   原来用 F_H2(24px)，而下面 266 处还有一行英文副标题（y=266），
     *   24px 字高就吃掉 24px，244+24=268 ⇒ 两行压在一起。
     *   改F_MID(16px)，字高 16px，244~260，与 266 那行留 6px 间隙。 */
    lv_obj_t *sign = label(s_splash_scr, 0, 244, F_MID, C_MUTED, "© 永远的兰兰");
    lv_obj_set_width(sign, LCD_H_RES);
    lv_obj_set_style_text_align(sign, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *en2 = label(s_splash_scr, 0, 266, F_TINY, lv_color_hex(0x55606C),
                          "L A N L A N   E T E R N A L");
    lv_obj_set_width(en2, LCD_H_RES);
    lv_obj_set_style_text_align(en2, LV_TEXT_ALIGN_CENTER, 0);

    /* ★ 底部提示改成「点一下跳过」—— 加了跳过功能就必须说清楚，
     *   否则用户盯着不放会以为卡住。原来的「正在准备拾声…」删掉：
     *   它描述的是内部进度，而 splash 只显示固定 3.5 s，
     *   这个文案本身就是假的（同夜间页那三行假数据一个毛病）。*/
    lv_obj_t *hint = label(s_splash_scr, 0, 292, F_TINY, C_MUTED, "点一下立即进入");
    lv_obj_set_width(hint, LCD_H_RES);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    lv_screen_load(s_splash_scr);

    /* ★ 动画定时器：60 ms 一拍，推进去 Splash 的生命周期绑定 ——
     *   splash_done_cb 里会 del 它。*/
    s_spl_tick = 0;
    s_splash_done = false;                     /* 只允许撤一次（见 splash_done_cb 注释）*/
    s_splash_timer = lv_timer_create(splash_done_cb, SPL_DWELL_MS, NULL);
    s_spl_timer = lv_timer_create(splash_anim_cb, SPL_RING_MS, NULL);
}

/* ============================================================
 *  顶栏
 * ============================================================ */
static void topbar_build(lv_obj_t *scr)
{
    lv_obj_t *bar = box(scr, 0, 0, LCD_H_RES, TOPBAR_H, C_BG, 0);
    box(scr, 0, TOPBAR_H - 1, LCD_H_RES, 1, C_LINE, 0);
    /* ★★ 2.8：品牌双色装饰条的【位置】改了 —— 从「拾声」字下沿
     *   挪到【顶栏地脚线】上（就是把底线那一段换成品牌色）。
     *   为什么非改不可：3.5 顶栏 38 高，18px 的「拾声」放 10 之后
     *   还剩 10px，塞一条 2px 装饰条绰绰有余；2.8 顶栏只有 28 高，
     *   18px 字就占掉 18 ⇒ 装饰条怎么摆都会糊在字脚上。
     *   实测两版都一样（10-10 两次截图），是几何上就没地方，
     *   不是差 1px 的问题 —— 所以【别再去微调 y】，直接换位置。
     *   ⚠️ 地脚线这里用 1px 而不是 2px：底栏总高只有 1px，
     *     写 2px 会溢到内容区第一行上。*/
    box(scr, 42, TOPBAR_H - 1, 12, 1, C_RED, 0);
    box(scr, 56, TOPBAR_H - 1, 10, 1, C_CYAN, 0);

    /* ★★★ 2.8 顶栏重排（10-10）—— 这一整段的坐标全是按【480 宽 / 38 高】算的：
     *   · 纵向：22px 高的 logo 放 y=8 ⇒ 底边 30 > 新顶栏高 28，直接顶出去
     *   · 横向：SD(392)/容量(404)/WiFi(450)/电池(352) 全在 x>320，
     *     即【顶栏右半边整片看不见】——截图里右上角只剩一点点残影
     *   新账（320 宽 × 28 高，元素从左到右一个不删）：
     *      8 → 38   logo 声波柱（30 宽，y 3，高 22）
     *     42 → 81   「拾声」18px + 3px 字距 = 39
     *     42 → 66   装饰条（y 24，高 2：亮绿 12 + 青 10）
     *     86        竖分隔线（y 7，高 14）
     *     92 → 170  署名「© 永远的兰兰」14px = 78
     *    174 → 212  时钟（18px，右对齐，框宽 38 ⇒ 字落 174~212）
     *    218 → 232  电池图标 (14px)
     *    236 → 250  SD 卡图标 (14px)
     *    254 → 284  容量文字（14px，「29G」≈ 25，框宽 30）
     *    292 → 306  WiFi 图标 (14px)
     *   右边余 14px。
     *   ⚠️ 账是「先加总再落位」：10-04 那次就是因为只算自己那一个、
     *     把电量图标塞进了本来已满的右边而叠字（见下面旧注释）。
     *     改这里任何一个坐标，请把上面 9 行一起重算。*/
    logo_mark(bar, 8, 3, 22, C_RED);

    /* ★★★★★★ 10-05 第七次：「拾声」当【硬件商标】重做（v1.39）
     *
     *   兰兰原话：「拾声这个名称字体作为硬件商标的字体不好看，
     *     白色字体像素感很严重，商标展示很重要，可以重点美化一点，
     *     拾声字体大小应该和署名一样即可」
     *
     *   —— 病根不是「字小」，是【24px 白字】这个组合本身难看：
     *     Ark Pixel 是像素字体，24px 是 12px 的 2 倍整数缩放（本身够锐），
     *     但纯白 #F2F5F8 落在 #0B0E12 深底上对比度接近 20:1 ——
     *     每个像素台阶都被拉成硬边，看着像「贴上去的色块」而不是字。
     *     ⇒ 美化靠三件事，都不额外吃内存：
     *       ① 字号 24 → 16（= 署名那档）：像素台阶从 2px 缩到 1.33px，
     *          硬边变细，锐利度靠 16px 自己的笔画撑着，不像色块了。
     *       ② 色 白 → 品牌绿 C_GREEN：与左边 logo 声波柱同一个色，
     *          「图标 + 字」读起来是一个整体商标，而不是两个东西。
     *          对比度从 ~20:1 降到 ~9:1，硬边存在感大幅下降。
     *       ③ 加 3px 字间距：两个字分开 3px，像 logo 的字距，
     *          不再是「两个方块贴在一起」。
     *       ④ 下面加一条双色装饰条（亮绿 14 + 青 12，共 1 个 box 的高度 2px），
     *          这是最省内存的「商标感」来源 —— 纯文字没有视觉锚点，
     *          一条横线就把「这是品牌名」这件事说清楚了。
     *       ⚠️ 装饰条只用【2 个 box】（每个约 300 B），不放底衬大色块：
     *          LVGL 池只剩 15 KB，一块 96×26 的底衬要 1 KB+，不值。
     *
     *   ★ 第七次横账（480 宽，全部 hmtx 实测，unitsPerEm=1200）：
     *     12 ~ 42   logo 声波柱（30 宽）
     *     48 ~ 79   「拾声」16px + 3px 字距 = 26.7+3 = 30.7
     *     48 ~ 79   装饰条 y=31 高 2（亮绿 14 + 电波青 12）
     *     96        竖分隔线（y=10 高 18）
     *     106 ~ 187 「© 永远的兰兰」16px = 80.7
     *     214 ~ 348 时钟（右对齐 16px）
     *     352 ~ 366 电池图标 (14px)
     *     392 ~ 406 SD 卡图标 (14px)
     *     404 ~ 444 容量文字（12px）
     *     450 ~ 464 WiFi 图标 (14px)
     *   ⇒ 拾声(79) 与署名(106) 之间空 27px，两块牌子各自分开。
     */
    {
        /* ★ 2.8：y 10→5（顶栏 38→28 后，18px 字在 28 里居中 = 上下各 5）。
         *   装饰条已挪到地脚线（见 topbar_build 开头），此处不再有它压字的顾虑。*/
        lv_obj_t *brand = label(bar, 42, 5, F_MID, C_RED, "拾声");
        lv_obj_set_style_text_letter_space(brand, 3, 0);
    }
    box(bar, 86, 7, 1, 14, C_LINE, 0);

    /* ★ 署名（兰兰 10-03，两次反馈）：
     *   「字体和颜色更醒目，毕竟是推广作用的展示」
     *   「如果标题栏太挤，署名可以小一点点就好了，颜色醒目是关键」
     *   ⇒ 字号只从 11px 提到 14px（小一点点，不跟 18px 的「拾声」抢地方），
     *     醒目靠【颜色】：0x8A96A3 那种灰几乎看不见，换成 C_GREEN 亮绿
     *     在 0x0B0E12 深底上对比度极高，一眼就能扫到。
     *   不加底衬 box：多一个 lv_obj ≈ 300 B，池只剩 15 KB，没必要为装饰花内存。
     *
     * ★ 10-05 兰兰反馈「作者署名可稍微放大」⇒ F_BODY(12px) → F_MID(16px)。
     *   16px 下「© 永远的兰兰」约 100px 宽，106+100=206 < 时钟框 214 ⇒ 不叠。*/
    /* ★ 2.8：x 106→92、y 11→7、字号 F_MID(16px) → F_TINY(14px)。
     *   降字号不是「不醒目了」——醒目靠的是 C_GREEN 亮绿在 0x0B0E12
     *   深底上的高对比，14px 时仍有 9:1 以上；而 16px 的 104px 宽
     *   会把右边的时钟一直挤到 x=198 之后，五个指示图标就放不下了。*/
    lv_obj_t *sign = label(bar, 92, 7, F_TINY, C_CYAN, "© 永远的兰兰");

    /* ---- 右上角：时钟 + SD 卡（图标紧贴容量） + WiFi ----
     * ★ 10-03 第十次（兰兰原话）：
     *   「标题栏出现两个内存容量显示，在TF卡旁边正确的显示容量，
     *     在WiFi标志左边还有一个淡灰色的容量显示是多余的」
     *   「标题栏显示就绪的含义，如果不重要可以替换为当前时间」
     *   ⇒ 「● 就绪 · TF 29G」那一格改成【当前时间】，
     *     容量只在 SD 卡图标右边出现一次。
     * ★ 第十一次（兰兰：「保留的那个距离内存卡标志太远，而且不是绿色字体」）：
     *   容量文字原来框在 396~448 且右对齐 ⇒ 字实际落在 420 附近，
     *   离 SD 图标（378~392）空了 28 px，看着像两样不相干的东西。
     *   ⇒ 改成【左对齐紧贴图标】，并用 C_GREEN + 14px 字号（有卡时）。
     *
     * ★ 顶栏横向账（480 宽，写死坐标前先算一遍，别再叠字）：
     *     106 → 190  署名「© 永远的兰兰」(14px ≈ 84)
     *     214 → 370  时钟（右对齐，18px，「18:34」≈ 56）
     *     378 → 392  SD 卡图标 (14px)
     *     396 → 436  容量文字（左对齐紧贴图标，14px，「29 G」≈ 34）
     *     444 → 458  WiFi 图标 (14px)
     *   历史老账：SD 容量框曾是 402~468 而 WiFi 图标在 452 ⇒ 叠了 16 px。
     *
     * ★ 第十二次（兰兰：「当前时间字体可以大一点，醒目一点，如足够地方」
     *   +「标题时钟可以往中间移动点」）：
     *   ① 11px 琥珀色太不起眼 ⇒ 提到 F_H2(18px)。颜色保持 C_AMBER
     *     （琥珀在深底上比绿更醒目，且和「拾声」的白、署名的绿、容量的绿
     *     都能一眼区分开）。
     *   ② 「往中间移」：原来框 198~372（174 宽）而「18:34」只 56 px 宽，
     *     靠右对齐 ⇒ 字落在 316~372，整个屏幕正中间偏右，看着是「贴着 SD 卡」。
     *     现在框 214~330（116 宽）⇒ 字落在 274~330，中心在 302，
     *     顶栏中线是 240 ⇒ 中心偏右 62 px，但比原来（中心 344）左移了 42 px。
     *     不敢再往左：署名结束于 190，要留 20 px 呼吸。
     *
     *   ⚠️ 顺带说明一件【故意没做】的事：第 7 页要显示满 12 格电平条，
     *   100 ms 的 LVGL 定时器在长时间重绘时理论上可能被饿死。解法是
     *   `CONFIG_FREERTOS_HZ=100`（心跳 2→10 拍/秒），但改 sdkconfig 一次 =
     *   **LVGL 877 文件 + wpa_supplicant 全量重编 25~30 分钟**，
     *   为「可能饿死」付这个代价不划算。现状：频谱与电平实测都正常跳动。
     *   ⇒ **别为了预防性风险去动 sdkconfig。**
     *
     * ★ 顶栏横向账（480 宽，写死坐标前先算一遍，别再叠字）：
     *     106 → 190  署名「© 永远的兰兰」(14px ≈ 84)
     *     214 → 330  时钟（右对齐，18px，「18:34」≈ 56 ⇒ 实际落在 274~330）
     *     336 → 350  电池图标 (14px)              ← 10-04 新增
     *     352 → 380  电量文字（14px，「85」≈ 16）   ← 10-04 新增
     *     378 → 392  SD 卡图标 (14px)             ← 与电量重叠 14px！
     *     396 → 436  容量文字（左对齐紧贴图标，14px，「29 G」≈ 34）
     *     444 → 458  WiFi 图标 (14px)
     *   历史老账：SD 容量框曾是 402~468 而 WiFi 图标在 452 ⇒ 叠了 16 px。
     *
     * ⚠️⚠️ 10-04 教训：上面这份账**自己算错了**（电量 352~380 与 SD 378~392
     *   重叠 14 px）。插电池加电量图标时我把「往左挤」当成理所当然，
     *   实际右边已经被三个指示物占满 —— 顶栏是这台机唯一的全局指示区，
     *   加任何东西之前必须把四者（时钟/电量/SD/WiFi）一起重算。
     *
     * ★ 第四次横账（10-04，加电池图标后的实际排布）——
     *   把 SD 容量框与 WiFi 图标整体【右移 14 px】给电池腾位置：
     *     106 → 190  署名「© 永远的兰兰」(14px ≈ 84)
     *     214 → 330  时钟（右对齐 18px，「18:34」≈ 56 ⇒ 实际 274~330）
     *     374 → 388  ★电池图标 (14px)  ← 新增
     *     392 → 406  SD 卡图标 (14px)  ← 原 378，右移 14
     *     404 → 444  容量文字（14px，「29 G」≈ 34） ← 原 396，右移 8
     *     450 → 464  WiFi 图标 (14px)  ← 原 444，右移 6
     *   ★ 电池图标只有 16 px 宽，**放不下百分比数字** ⇒ 顶栏只用【颜色】表电量
     *     （绿/琥珀/红），百分比数字放到夜间页与状态页那两行整宽的地方。
     *   ⚠️ 顶栏再加任何东西之前，先把这四个一起重算，别只算自己那一个。
     *
     * ★★★★★★ 10-05 第六次横账（v1.38）—— 兰兰真机复验：
     *   「时钟没有向左移动」＋「电源标志向左移动了，挡住了时钟分钟位」
     *
     *   【我上一版算错了，注释还写着"更靠中间"，实际是右移了 36px —— 假账害死人】
     *   原因：把框宽从 116 加到 152，右边缘随之从 330 推到 366，
     *   而右对齐的字是【贴着右边缘】放的 ⇒ 字从 274~330 挪到 310~366。
     *   同时电池左移到 352，正好落在 310~366 里间 ⇒ 压住分钟位。
     *   ⇒ **教训：右对齐的框，加宽等于右移；改框宽前必须先算字落在哪。**
     *
     *   ★ 本版所有宽度改成【实测值】（用 fontTools hmtx 按 unitsPerEm 折算），
     *     不再拍脑袋估算 —— 前五次的账全部是估的，所以连错两次。
     *       12px 署名 78px ｜ 16px 署名 104px ｜ 24px 署名 156px
     *       24px「18:34」= 4×12 + 8 = 56px（数字 advance 600/1000，冒号 400/1000）
     *
     *   第六次横账（480 宽）：
     *     106 → 210  署名「© 永远的兰兰」(16px = 104px 实测)
     *     214 → 348  时钟（24px，右对齐，框宽 134；字落 292~348，中心 320）
     *     352 → 366  电池图标 (14px)              ← 保持 352，与时钟框留 4px
     *     392 → 406  SD 卡图标 (14px)              ← 不动（兰兰两次确认正确）
     *     404 → 444  容量文字（12px，「29 G」≈ 34）   ← 不动
     *     450 → 464  WiFi 图标 (14px)              ← 不动
     *
     *   时钟为什么只能到中心 320（而不是顶栏中线 240）：
     *   左边被 210px 宽的署名占着，署名不能压（那是兰兰要求放大的），
     *   右边被电池/SD/容量/WiFi 占着 112px。剩下 210~352 这 142px 空档，
     *   中心是 281；字宽 56 ⇒ 右对齐后落在 292~348（中心 320）。
     *   要真正居中到 240 得让时钟【居中对齐】而非右对齐，代价是
     *   「--:--」占位期间（未校时时）字会左右跳。⇒ 保持右对齐。
     */
    /* ★★★★ 10-10 顶栏重排（兰兰真机三条）：
     *   ①「当前时间只有前两位，后两位被右边挡住了」
     *     真因：时钟框宽只有 38 px，而 18px 字号下「18:34」实测约 42 px
     *     ⇒ 右对齐时右边溢出的部分被裁掉。占位期显示「--:--」只有约 25 px，
     *       所以之前的截图【完全看不出来】—— 这类 bug 只在"真有时间"时暴露。
     *   ②「内存卡容量大小删了」⇒ 顶栏不再有容量文字（腾出 30 px）
     *   ③「内存卡和电源标志朝右移动点就能完整显示时间了」⇒ 整体右移
     *
     *   本轮新账（320 宽 × 28 高，右半部分）：
     *     178 → 240  时钟（右对齐，框宽 62，★ 比原来宽 24 px）
     *     248 → 262  电池图标   （原 x=218，右移 30）
     *     270 → 284  内存卡图标 （原 x=236，右移 34）
     *     292 → 306  WiFi 图标 （原 x=292，不动）
     *     右侧余 14 px。
     *   ⚠️ 时钟必须显式设 LV_LABEL_LONG_CLIP：label 默认是 LONG_WRAP，
     *     框比文字窄时它会折成两行，第二行会掉出 28 高 的顶栏、压到内容区
     *     （上一版靠"刚好不折"侥幸没出这个问题，加宽后再也不会有）。
     *   ⚠️ 改这里任何一个坐标，请把上面 4 行一起重算（铁律：只算自己那一个必然叠字）。*/
    s_lbl_topstat = label(bar, 178, 4, F_MID, C_CYAN, "--:--");
    /* ★★ 10-05 第七次：F_H2(24px) → F_MID(16px)（兰兰：「时钟字体大小也和署名一样，
     *   这样整体协调」）。右对齐框宽也跟着重算 ——
     *   ★ 铁律：改字号必须同时改框宽的账，否则又是「按 24px 时代的框放 16px 的字」。
     *     实测 16px：「18:34」= 33.3px、「--:--」= 22.2px（数字 advance 600/1000、
     *     冒号与短横 400/1000，unitsPerEm=1200）。
     *   框 214~348（宽 134）右对齐 ⇒ 字落 315~348，离电池图标(352)还有 4px。
     *   为什么还留 4px 而不是顶到 352：--:-- 占位期只有 22.2px 宽，
     *   贴着 352 会在校时前后看到字「跳一下」，留白反而更稳。*/
    lv_obj_set_width(s_lbl_topstat, 62);
    lv_label_set_long_mode(s_lbl_topstat, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_lbl_topstat, LV_TEXT_ALIGN_RIGHT, 0);

    s_ic_sd = label(bar, 270, 7, F_ICON, C_MUTED, LV_SYMBOL_SD_CARD);
    lv_obj_set_style_text_color(s_ic_sd, C_GREEN, 0);
    /* ★ 10-10 兰兰：「内存卡容量大小删了」⇒ 顶栏不再有容量文字（腾出 30 px 给时钟）。
     *   容量信息没丢：首页「音乐库」卡上写「N 首 →」、状态页写「TF … MB · 音频 N 首」。
     *   ⚠️ 句柄变量保留、置 NULL：refresh_sd_info() 那边是 `if (s_lbl_sdinfo)` 守卫的，
     *     不建对象它自动跳过 —— 这样只改一处，不必去动刷新函数（少动一处少一个坑）。*/
    s_lbl_sdinfo = NULL;

    /* WiFi 图标（最右）*/
    s_ic_wifi = label(bar, 292, 7, F_ICON, C_MUTED, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_ic_wifi, C_MUTED, 0);

    /* ---- 电池图标（10-04 加，10-05 左移）----
     * ★ 位置 352~366：★ 10-05 从 374 左移 22px。
     *   原因：原来 374 与 SD 图标 392 只差 18px，图标 14px 宽 ⇒ 净间隙 4px，
     *   兰兰真机反馈「充电和电源标志向左移动一点点，太紧贴内存卡标志了」。
     *   左移后与 SD 拉开 40px，且离时钟右端（366）刚好接上，不留空隙浪费。
     *   宽度只有 14 px ⇒ **放不下百分比文字**，只用颜色表示电量档位
     *   （绿/琥珀/红）。百分比数字放夜间页和状态页（那里有整行空间）。*/
    s_ic_batt = label(bar, 248, 7, F_ICON, C_MUTED, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(s_ic_batt, lv_color_hex(0x3A424C), 0);
    s_batt_shown_pct = -2;      /* -2 = 还没读过，强制首次刷 */
}

/* ============================================================
 *  左侧竖导航
 * ============================================================ */
static void nav_clicked(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    goto_page(idx);
    tap_note("导航");
}

static void nav_build(lv_obj_t *scr)
{
    static const char *names[NAV_N] = {"首页", "发现", "播放", "夜间", "状态"};
    lv_obj_t *rail = box(scr, 0, TOPBAR_H, RAIL_W, CONT_H, C_PANEL, 0);
    box(scr, RAIL_W - 1, TOPBAR_H, 1, CONT_H, C_LINE, 0);

    /* ★★★ 2.8 重排（10-10 第三轮）—— 兰兰原话「左边导航页标题只看到一半」。
     *   两个毛病叠在一起，都出在这段旧账上（它是按 3.5 的 CONT_H=282 算的）：
     *   ① 纵向：5 格 × (8 + i*52)、格高 46 ⇒ 第 5 格顶边 y=216，底边 262。
     *      CONT_H 只有 212 ⇒ 「状态」整格落在屏外（不是文字被切，是格子没了）。
     *   ② 横向：标签写在 label(b, 22, 14) ⇒ rail 坐标 x=26，
     *      而 rail 只有 40 宽 ⇒ 可用 14px；两字 14px 字号约 28px
     *      ⇒ 右边一半被裁掉，「首页」看起来像「首厂」。
     *   新账：5 格高 34、步进 40、起点 9 ⇒ 9 + 4*40 + 34 = 203 ≤ 212 ✓
     *   标签改成【在整条 rail 上居中】：text_align=CENTER + 宽=RAIL_W。 */
    for (int i = 0; i < NAV_N; i++) {
        lv_obj_t *b = box(rail, 2, 9 + i * 40, RAIL_W - 4, 34, C_HILITE, 10);
        lv_obj_set_style_bg_opa(b, i == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        /* ★★ 第二十二次：导航按钮不挂 ev_press/ev_release。
         *   它们按下时改 opa ⇒ 触发重排 ⇒ LVGL 在同一帧发现指针下的对象变了
         *   就把它判成滚动并放弃 CLICKED —— 20-04「跳转键要碰几次」的同款坑，
         *   当时只修了跳转键，导航栏这几个一直没摘。*/
        lv_obj_add_event_cb(b, nav_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_nav_btns[i] = b;

        s_nav_marks[i] = box(b, 2, 10, 2, 14, i == 0 ? C_RED : C_NAV_DIM, 1);

        /* ★★ 兰兰：「首页左边导航栏字体都改成绿色」。
         *   原来未选中的用 C_MUTED(#7C8894 灰)，在 C_PANEL(#151A21) 底上
         *   对比度只有 3.4:1，看着像「禁用」。
         *   ⇒ 全部改绿；当前页用亮绿 C_GREEN，其余用暗绿 0x1F7A5C ——
         *     「都是绿的」满足要求，同时当前页仍能一眼分出来
         *     （靠亮度差，而不是靠颜色差）。*/
        /* ★★★ 2.8：标签的父对象从按钮 b 换成 rail，并在 rail 上整宽居中。
         *   为什么非换不可（10-10 第三轮的真因）：
         *     写成 label(b, 22, 14) 时 rail 坐标是 4+22 = 26，
         *     rail 总宽 40 ⇒ 只剩 14px，而「首页」两个字 14px 要 28px
         *     ⇒ 右边一半被父对象裁掉。这不是字号问题，是【起点太靠右】。
         *   换到 rail 上居中后：40px 里放 28px 文字，左右各余 6px。
         *   ⚠️ 居中的对象【必须显式给宽】（这里 = RAIL_W），
         *     LVGL 的 label 默认宽是「文本宽」，不给宽就无从居中。
         *   左侧那根小绿条因此也要缩到 2px 宽、贴左边，
         *   否则会和居中后的文字左端（rail x=6）叠上。*/
        s_nav_labels[i] = label(rail, 0, 9 + i * 40 + 9, F_TINY,
                                i == 0 ? C_RED : C_NAV_DIM, names[i]);
        lv_obj_set_width(s_nav_labels[i], RAIL_W);
        lv_obj_set_style_text_align(s_nav_labels[i], LV_TEXT_ALIGN_CENTER, 0);
    }
}

/* ============================================================
 *  首页 —— 正在播放 + 栏目条（横滑）+ 双列电台网格（上下滑）
 * ============================================================ */

static lv_obj_t *s_lbl_list_title;    /* 网格上方那行「全部 · 共 N 台」*/
/* ★ 10-04 第二十八次：原来只有一张「N 个常用台」的收藏卡，
 *   现在收藏搬去标签条了，这里改显示【最近听过】。
 *   s_lbl_favcnt 保留（收藏页标题、收藏数还靠它），但不再指向首页卡。*/
static lv_obj_t *s_lbl_favcnt;        /* 收藏数量文字（收藏页/全屏页用）*/
static lv_obj_t *s_lbl_histcnt;       /* 首页「最近听过」卡：最近一电台名 */
static lv_obj_t *s_lbl_sdinfo2;       /* 首页「音乐库」卡：内存卡音频数 */
static int       s_hr_sel = -1;       /* 当前选中的台（g_stations 下标）*/
/* ★ 10-04 第二十九次：记着上一次看到的 SD 挂载状态。
 *   app_main 里 ui_init() 早于 app_sd_mount()，建卡时卡还没挂上，
 *   所以首页「音乐库」那行字要等挂载状态【发生变化】时再刷一次。*/
static bool      s_last_sd_mounted = false;
static int       s_pool[GRID_MAX];    /* 当前筛选结果的下标池 */
static st_grid_t *s_grid_home;        /* 首页那层（固定「全部」）*/
static st_grid_t *s_grid_find;        /* 发现页那层 */
static st_grid_t *s_grid_list;        /* 第 7 页全屏列表（栏目/省份/收藏共用一个）*/
/* 第 7 页标题「新闻综合 · 共 128 台」。ovs_grid_refresh() 在本行之前就要用它，
 * 所以声明提前到这儿，定义仍留在文件后段（page_list_build 里）。*/
static lv_obj_t *s_lbl_list_title2;

/* ★ 10-11：第 7 页标题行右端那一格。两种形态共用同一个位置（见 page_list_build）：
 *     普通列表 → s_lbl_list_hint 一行灰字「长按收藏」
 *     收藏/历史 → s_btn_list_clear 一颗可点的「清空全部」
 *   同一时刻只显示一个。两个都在 page_list_build 里建。*/
static lv_obj_t *s_lbl_list_hint;
static lv_obj_t *s_btn_list_clear;
static lv_obj_t *s_lbl_list_clear;        /* 清空按钮里的字（两下确认要改文案）*/
/* 「清空全部」要按两下：第一下只是【上膛】，4 秒内再点才算数。
 * 用 lv_tick_get()（毫秒、单调）而不是系统时间 —— 同 app_sleep 的理由。*/
static bool      s_clear_armed;
static uint32_t  s_clear_arm_tick;

/* 前向声明：utf8_copy_n / ev_open_hist / home_cards_refresh 都定义在文件后段，
 * 但首页建页（page_home_build）要用。*/
static void utf8_copy_n(char *dst, size_t dstsz, const char *src, int max_chars);
static void ev_open_hist(lv_event_t *e);
static void home_cards_refresh(void);
/* ★ on_grid_play / on_grid_fav（第 1784/1785 行）在 home_grid_refresh（1858）
 *   之前，但它们要调它重排 ⇒ 必须在这里先声明。
 *   ★★★ 别为了「顺序好看」把它挪到 on_grid_fav 后面：
 *   网格排序的真相源应该在【建网格的地方】，不是在使用者后面。*/
static void home_grid_refresh(void);

/* 第 7 页当前在看什么 */
enum { LIST_BY_CAT = 0, LIST_BY_PROV, LIST_BY_FAV, LIST_BY_HIST,
LIST_BY_ALL_OVS,      /* 把全部海外台平铺一页（保留，给日志对照用）*/
LIST_BY_OVS_REGION,   /* ★ 海外【二级】：先列 11 个地区，点一个再看它的台 */
LIST_BY_RESUME };     /* ★ 10-11：首页「续听」格 —— 只列有进度的本地音频 */
static int         s_list_mode  = LIST_BY_CAT;
static int         s_list_cat   = 0;
static int         s_list_prov  = 0;
static const char *s_list_title = "";
static int         s_list_total = 0;
static int         s_list_back  = 0;   /* 返回键回哪一页 */

static void on_grid_play(int idx);
static void on_grid_fav(int idx);
static void on_grid_del(int idx);      /* ★ 10-11：收藏/历史页长按删一条 */
static void list_page_refresh(void);   /* 第 7 页全屏列表刷新（定义在文件后段）*/
static void goto_page(int idx);        /* 切页（定义在网格段之后，这里先声明）*/
static void ev_open_fav(lv_event_t *e); /* 收藏页入口（定义在文件后段）*/
/* 第 8 页 WiFi 设置入口（定义在文件后段）。★ 必须在这里前向声明：
 * 状态页 page_stat_build 在 3400 多行就要挂它，而定义在 3500 多行。*/
static void ev_open_wifi_page(lv_event_t *e);
/* 这一页的静态部分（标题/状态行/动作键）建页时建一次；
 * wifi_page_destroy 的声明在 goto_page 之前（那里更早）。*/
static void wifi_page_ensure(void);
/* ★ 第二十四次：WiFi 页的 60 ms 待办定时器已删（屏幕键盘整个去掉了，
 *   就没有「在事件回调里删/建对象」这个需求了）。*/
/* 海外二级：第 7 页在「地区块」与「该地区的台」之间来回切（定义在文件后段）*/
static void list_page_back_to_ovs(void);
static void ovs_grid_refresh(void);

static void home_update_card(int st_idx)
{
    int n = app_radio_station_count();
    if (st_idx < 0 || st_idx >= n) return;
    const net_station_t *st = app_st_get(st_idx);

    if (s_lbl_station) {
        char nm[64];
        utf8_copy_n(nm, sizeof(nm), st->name, 16);
        lv_label_set_text(s_lbl_station, nm);
    }
    if (s_lbl_source) {
        const char *pn = (st->prov == NET_PROV_NONE) ? "全国"
                                                      : g_prov_name[st->prov % NET_PROV_N];
        /* ★ 10-11：去掉尾部写死的「· 64 kbps」。两条理由，②是硬约束：
         *   ① 那是个【假值】—— 字符串常量，跟这条流的真实码率毫无关系
         *      （真码率只有 app_radio 解出流之后才知道）。展示假数字不如不展示。
         *   ② 这一行 10-11 已从「卡片第三行（可用 200 px）」搬到
         *      「第一行右半（可用 126 px）」（兰兰要的：腾行给频谱）。
         *      实测「中国香港 · 音乐 · 64 kbps」= **165 px** ⇒ 尾巴必被省略号吃掉，
         *      反而连「音乐」两个字都看不全。现在「中国香港 · 音乐」= **99 px** ✓。
         *   ⚠️ 将来若要显示真实码率，得先在 app_radio 里把它存成状态再传过来，
         *      不要再往这里塞字符串常量。*/
        lv_label_set_text_fmt(s_lbl_source, "%s · %s", pn,
                              g_cat_name[st->cat % NET_CAT_N]);
    }
    /* ★ 这里【不】写 s_lbl_playname：第 3 页才是真正的播放界面，
     *   曲名由 player_refresh() 管；首页再改它就会串台。*/
    if (s_lbl_playstate) {
        lv_label_set_text_fmt(s_lbl_playstate, "%s · %s",
                              app_radio_is_paused() ? "已暂停" : "播放中",
                              st->name);
    }
}

/* 把符合筛选条件的台下标收进 pool，返回个数。
 * cat < 0 = 不按栏目筛；prov < 0 = 不按省份筛。*/
static int collect_stations(int *pool, int cap, int cat, int prov)
{
    int n = app_radio_station_count();
    int pn = 0;
    for (int i = 0; i < n && pn < cap; i++) {
        if (cat  >= 0 && (int)app_st_get(i)->cat  != cat)  continue;
        if (prov >= 0 && (int)app_st_get(i)->prov != prov) continue;
        pool[pn++] = i;
    }
    return pn;
}

/* 首页那层网格：固定「全部」（栏目走全屏页） */
/* ★★★ 10-04 第二十八次：首页网格不再只是「按台单顺序的前 6 台」。
 *
 *   兰兰：「在启动时首页显示了全部页面的前六个在前排直观显示，
 *         如果有收藏页保存电台，可以在首页最优先显示收藏页，
 *         就是栏目增加收藏放第一，如果有历史播放最好」
 *   ⇒ 三段拼接：**收藏 → 历史 → 台单其余**，前 6 格优先给它们。
 *
 *   为什么是这个顺序（不是听播放次数）：
 *     收藏是【用户主动表达过的意图】——我明确想留这个台；
 *     历史是【刚才发生过的事实】——我上次的注意力在哪；
 *     台单顺序是【默认值】——没什么信息量。
 *     三者可信度递减，所以放前面的理由递减。
 *
 *   ★ 去重：三段之间必须互相去重，否则同一个台会占掉 6 格里的 3 格。
 *     app_fav_list() 和 app_hist_top() 各自只保证【自己内部】不重复，
 *     跨段的重叠要在这里用一个已用数组挡掉。*/
static void home_grid_refresh(void)
{
    int n = 0;
    static bool used[512];        /* ★ 定长 512：GRID_MAX 是这个量级，
                                    越界访问会读坏栈上的相邻变量。
                                    台单 686 台但首页只取 6 个，
                                    前 6 个下标不可能超过 512。*/

    memset(used, 0, sizeof(used));

    /* ---- 第 1 段：收藏 ---- */
    int fav[16];
    int nf = app_fav_list(fav, (int)(sizeof(fav) / sizeof(fav[0])));
    for (int i = 0; i < nf && n < GRID_MAX; i++) {
        int idx = fav[i];
        if (idx < 0 || idx >= 512 || used[idx]) continue;
        used[idx] = true;
        s_pool[n++] = idx;
    }

    /* ---- 第 2 段：历史（最近听的在前）---- */
    int hist[16];
    int nh = app_hist_top(hist, (int)(sizeof(hist) / sizeof(hist[0])));
    for (int i = 0; i < nh && n < GRID_MAX; i++) {
        int idx = hist[i];
        if (idx < 0 || idx >= 512 || used[idx]) continue;
        used[idx] = true;
        s_pool[n++] = idx;
    }

    /* ---- 第 3 段：台单其余，按原顺序补满 ---- */
    int rest[256];
    int nr = collect_stations(rest, (int)(sizeof(rest) / sizeof(rest[0])), -1, -1);
    for (int i = 0; i < nr && n < GRID_MAX; i++) {
        int idx = rest[i];
        if (idx < 0 || idx >= 512 || used[idx]) continue;
        used[idx] = true;
        s_pool[n++] = idx;
    }

    /* ★★★ 10-10：首页网格已按兰兰要求移除 ⇒ 本函数现在【永远】拿不到网格。
     *   原实现会调 grid_pages_ensure(0) 兜底重建，而 case 0 已删：
     *   再调就是「warn 一条 + 白建一个容器又删掉」，纯噪音。
     *   ⇒ 兜底一并摘掉，静默返回。
     *   函数本身保留：on_grid_play / on_grid_fav / 切台回调里都有
     *   `if (s_grid_home) ...` 判空，但「判空后还是调进来」的路径存在，
     *   留一个立即返回的空壳最省事，也好过把十几处调用点全删一遍。*/
    if (!s_grid_home) return;
    grid_set(s_grid_home, s_pool, n);
    if (s_lbl_list_title) {
        int total = app_radio_station_count();
        if (n < total) {
            /* ★ 说清楚「只显示了 6 个」而不是撒谎说「共 6 台」——
             * 兰兰能看到还剩多少台没显示，才知道要点栏目进全屏页。*/
            lv_label_set_text_fmt(s_lbl_list_title,
                                  "收藏/历史优先 · 显示 %d / 共 %d 台",
                                  n, total);
        } else {
            lv_label_set_text_fmt(s_lbl_list_title,
                                  "全部电台 · 共 %d 台", n);
        }
    }
    ESP_LOGI(TAG, "home grid: %d 格（收藏 %d + 历史 %d + 其余补满）", n, nf, nh);
}

/* 点某一格：★ 真的开始播这一台，并跳到播放页（「按了有反应」看得见）*/
static void on_grid_play(int idx)
{
    /* ============================================================
     *  ★ 10-11：历史页里的【本地音频】格子（编码见 HIST_LOCAL_BASE）
     * ============================================================
     *  兰兰：「起码在历史播放里面有本地音频记录是可以这样续播的」。
     *  点它 = 播这个文件；有没有上次的进度由 app_radio 自己去查、
     *  自己 seek（见 app_radio.c 的 s_resume_sec），这里不用管。*/
    if (idx >= HIST_LOCAL_BASE) {
        const char *fp = app_resume_path_at(idx - HIST_LOCAL_BASE);
        if (fp == NULL || fp[0] == '\0') return;
        tap_note(fp);

        /* 记住目录：播放页的「上一个 / 下一个」要在同一目录里换曲。
         * ★ 必须和 SD 页点文件那条路做同一件事 —— 漏了的话，
         *   从历史进来的这一首按"下一个"会跑去上一个目录。*/
        const char *b = strrchr(fp, '/');
        if (b != NULL) {
            size_t dl = (size_t)(b - fp);
            if (dl >= sizeof(s_local_dir)) dl = sizeof(s_local_dir) - 1;
            memcpy(s_local_dir, fp, dl);
            s_local_dir[dl] = '\0';
        }

        esp_err_t err = app_radio_play_file(fp);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "play_file(hist) -> %s", esp_err_to_name(err));
        }
        /* s_play_from 不用设：grid_slot_clicked 已经按 g->from_page（=6）写好了，
         * 所以播放页的返回键会回到这一页「最近听过」。*/
        player_refresh();
        goto_page(2);
        return;
    }

    if (idx < 0 || idx >= app_radio_station_count()) return;
    s_hr_sel = idx;
    home_update_card(idx);
    tap_note(app_st_get(idx)->name);
    /* 网格里正在播的那格要高亮，播完把几层都重填一次 */
    if (s_grid_home) grid_fill(s_grid_home);
    if (s_grid_find) grid_fill(s_grid_find);
    if (s_grid_list) grid_fill(s_grid_list);

    esp_err_t err = app_radio_play_station(idx);
    if (err != ESP_OK) {
        /* 具体原因由播放页那一行显示（app_radio_last_error 给中文原因）*/
        ESP_LOGW(TAG, "play_station(%d) -> %s", idx, esp_err_to_name(err));
    }
    /* ★★ 10-04 第二十八次：刚播的这一台已经进了历史（app_radio 里记的），
     *   而首页网格的排序依赖历史 ⇒ 必须重排。
     *   ★ 只在首页建过网格时才重排：网格是懒建的，
     *     切台发生在播放页时 s_grid_home 可能还是 NULL。
     *   ⚠️ 顺序：先刷网格（它会按新顺序重填），再刷卡片
     *     （卡片显示「最近听的台名」，依赖的也是历史）。*/
    if (s_grid_home) home_grid_refresh();
    home_cards_refresh();

    goto_page(2);
    player_refresh();
}

/* 长按一格 = 收藏 / 取消收藏。★ 收藏存 NVS，掉电不丢（见 app_fav.c）*/
static void on_grid_fav(int idx)
{
    if (idx < 0 || idx >= app_radio_station_count()) return;
    bool had = app_fav_has(idx);
    esp_err_t r = app_fav_toggle(idx);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "fav_toggle(%d) -> %s", idx, esp_err_to_name(r));
        tap_note("收藏满了（最多 64 个）");
        return;
    }
    tap_note(had ? "已取消收藏" : "已收藏");
    /* 所有层的收藏标记都要跟着变 */
    if (s_grid_home) grid_fill(s_grid_home);
    if (s_grid_find) grid_fill(s_grid_find);
    if (s_grid_list) grid_fill(s_grid_list);
    if (s_lbl_favcnt) {
        lv_label_set_text_fmt(s_lbl_favcnt, "%d 个常用台", app_fav_count());
    }
    /* ★★ 10-04 第二十八次：收藏是首页排序的【第 1 段】，
     *   收藏/取消都会改变首页前 6 格是谁 —— 只 grid_fill 不重排的话，
     *   刚收藏的台不会真的「顶到最前面」，只是高亮变了。*/
    if (s_grid_home) home_grid_refresh();
}

/* ★ 10-11：收藏页 / 历史页里「长按 删一条」（兰兰要的「收藏历史删单条」）。
 *
 *  ⚠️ 只在 del_mode 的网格上被调（由 list_page_refresh 按 s_list_mode 置位），
 *     所以这里用 s_list_mode 分派是安全的 —— 不存在「收藏页的格子
 *     长按却按历史逻辑删」的可能。
 *
 *  ★ 为什么删完要 list_page_refresh() 整页重画，而不是像收藏那样只
 *     grid_show_cell() 改一格：删掉一条会让后面所有条目上移一格，
 *     "这一格"的身份在删除后就不成立了（见 grid_slot_long 的注释）。*/
static void on_grid_del(int idx)
{
    if (s_list_mode == LIST_BY_FAV) {
        if (idx < 0 || idx >= app_radio_station_count()) return;
        esp_err_t r = app_fav_remove(idx);
        if (r != ESP_OK) {
            ESP_LOGW(TAG, "fav_remove(%d) -> %s", idx, esp_err_to_name(r));
            tap_note("删除失败");
            return;
        }
        tap_note("已取消收藏");
        if (s_lbl_favcnt) lv_label_set_text_fmt(s_lbl_favcnt, "%d 个常用台", app_fav_count());
        /* 首页排序第 1 段就是收藏，删了要重排；发现页/首页的收藏星标也要变 */
        if (s_grid_home) home_grid_refresh();
        if (s_grid_find) grid_fill(s_grid_find);
        list_page_refresh();
        return;
    }

    if (s_list_mode != LIST_BY_HIST) return;

    if (idx >= HIST_LOCAL_BASE) {
        /* 本地音频条目（pool 里是 HIST_LOCAL_BASE + 续播表下标）。
         * ★ 必须先把路径拷出来：app_resume_forget() 会把表往前挪，
         *   app_resume_path_at() 返回的指针指向的是表内部，删完就失效。*/
        static char fp[APP_RESUME_PATH];
        const char *p = app_resume_path_at(idx - HIST_LOCAL_BASE);
        if (p == NULL || p[0] == '\0') return;
        size_t l = strlen(p);
        if (l >= sizeof(fp)) l = sizeof(fp) - 1;
        memcpy(fp, p, l);
        fp[l] = '\0';
        app_resume_forget(fp);
        tap_note("已从续听记录移除");
        list_page_refresh();
        return;
    }

    if (idx < 0 || idx >= app_radio_station_count()) return;
    /* ★ 用的是台单里的完整台名 —— app_hist_note() 当初存的就是它
     *   （app_radio.c:1843 传的是 app_st_get(idx)->name），所以能精确命中。
     *   这个指针指向台单静态区，后面的重排动不到它。*/
    esp_err_t r = app_hist_remove(app_st_get(idx)->name);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "hist_remove(%d) -> %s", idx, esp_err_to_name(r));
        tap_note("删除失败");
        return;
    }
    tap_note("已删除这条记录");
    if (s_grid_home) home_grid_refresh();       /* 首页「最近听过」卡要跟着变 */
    if (s_grid_list) grid_fill(s_grid_list);
    list_page_refresh();
}

static void ev_home_play(lv_event_t *e)
{
    (void)e;

    if (app_radio_is_playing()) {
        app_radio_toggle_pause();
        tap_note(app_radio_is_paused() ? "暂停" : "继续");
    } else if (s_hr_sel >= 0) {
        app_radio_play_station(s_hr_sel);
        tap_note(app_st_get(s_hr_sel)->name);
    } else {
        tap_note("先在下面选一个台");
        return;
    }

    s_playing = !app_radio_is_paused();
    if (s_playbtn_lbl)   lv_label_set_text(s_playbtn_lbl, s_playing ? "暂停" : "播放");
    if (s_lbl_playbtn)   lv_label_set_text(s_lbl_playbtn, s_playing ? "暂停" : "播放");
    if (s_hr_sel >= 0)   home_update_card(s_hr_sel);
}

static void ev_open_play(lv_event_t *e)
{
    (void)e;
    tap_note("正在播放卡 -> 播放页");
    s_play_from = 0;              /* 从首页大卡进来的 ⇒ 返回键回首页 */
    goto_page(2);
}

/* 「城市声音集」卡片 -> SD 本地音频页 */
static void ev_open_sd(lv_event_t *e)
{
    (void)e;
    tap_note("城市声音集 -> 本地音频");
    s_sd_count_dirty = true;      /* 每次从首页进来重新数一次全卡音频 */
    goto_page(5);
    ui_sd_page_refresh();
}

/* hstrip 的回调原型：void (*)(int) */
static void ev_pick_cat(int cat);
static void ev_pick_prov(int prov);

/* 点栏目胶囊 → 进第 7 页全屏列表看这一栏的台。
 * ★ 兰兰 10-03 原话：「首页按新闻就直接进入全屏的新闻电台页，
 *   只要有退回键就可以了，毕竟屏幕小，需要更大的分类页」。
 *
 * ★★ 10-04 第二十八次：前 3 颗是【自定义入口】，先分流再谈栏目。
 *   下标 0/1/2 见 k_cat_short 上面的定义。
 *   ⚠️ 分流必须做在【最前面】：下标 3..14 才是真栏目，
 *     若把 0 也当 cat 传下去，s_list_cat=0 恰好是「新闻」，
 *     点「收藏」会列出一整页新闻台 —— 静默错，编译零报错。*/
static void ev_pick_cat(int cat)
{
    if (cat < 0) return;

    if (cat == HCAT_FAV) {
        tap_note("收藏电台");
        s_list_mode  = LIST_BY_FAV;
        s_list_title = "我的收藏";
        s_list_back  = 0;
        list_page_refresh();
        goto_page(6);
        return;
    }
    if (cat == HCAT_HIST) {
        tap_note("播放历史");
        s_list_mode  = LIST_BY_HIST;
        s_list_title = "最近听过";
        s_list_back  = 0;
        list_page_refresh();
        goto_page(6);
        return;
    }
    if (cat == HCAT_SD) {
        /* 卡内 = 内存卡本地音频，直接进第 5 页。*/
        tap_note("音乐库");
        s_sd_count_dirty = true;      /* 每次进来重新数一次全卡音频 */
        goto_page(5);
        ui_sd_page_refresh();
        return;
    }

    /* ★ 10-11：「续听」格 —— 原「宗教」那一格。
     *   必须挡在下面那条通用路径【之前】，否则 13 会被当成 g_cat_name[10]。*/
    if (cat == HCAT_RESUME) {
        tap_note("继续收听");
        s_list_mode  = LIST_BY_RESUME;
        s_list_title = "继续收听";
        s_list_back  = 0;
        list_page_refresh();
        goto_page(6);
        return;
    }

    /* 「全部」不进全屏页 —— 首页那层网格本来就是全部。*/
    if (cat >= HCAT_ALL) {
        tap_note("全部电台就在下面");
        return;
    }
    s_list_mode  = LIST_BY_CAT;
    s_list_cat   = cat - HCAT_REAL;      /* ★ 减去 3，把自定义入口让出去 */
    s_list_title = g_cat_name[s_list_cat];
    s_list_back  = 0;
    tap_note(k_cat_short[cat]);
    list_page_refresh();
    goto_page(6);
}

/* ============================================================
 *  首页【栏目多列网格】（★ 2.8 新增，替代 3.5 那条横滑 hstrip）
 * ============================================================
 *  为什么必须换掉 hstrip：
 *    3.5 横屏 480 宽，那条胶囊条能把 16 颗排成「看得见 5 颗 + 左右箭头翻页」；
 *    2.8 内容区只剩 280 宽，同屏 5 颗时每颗 44px，
 *    而 280 宽要装 16 项 ⇒ 兰兰必须一直按 ← → 才能找到「境外」，
 *    小屏上这个交互太重。
 *  兰兰 10-10 原话：「下面 12 个栏目多列排就可以了」⇒ 一次全摊开、不用翻。
 *
 *  版面账（内容区 280 × 212）：
 *    5 列 49 + 4 缝 5 = 265，起点 x=6 ⇒ 6..271（右边留 9）
 *    3 行 30 + 2 缝 5 = 100，起点 y=108 ⇒ 108..208（下边留 4）
 *  15 格 = k_cat_short 的前 15 项 = 收藏 / 历史 / 卡内 + 12 个真栏目。
 *  ★ 下标与 k_cat_short【一一对应】⇒ 点击直接交给 ev_pick_cat(i)，
 *    它内部已有「0/1/2 是自定义入口、3 起才是栏目」的分流逻辑，
 *    这里不要再抄一份（抄两份迟早不一致）。 */
#define CATG_COLS  5
#define CATG_ROWS  3
#define CATG_N     (CATG_COLS * CATG_ROWS)   /* 15 */
#define CATG_X     6
#define CATG_Y     108
#define CATG_CW    49
#define CATG_CH    30
#define CATG_GX    5
#define CATG_GY    5

static lv_obj_t *s_catg_cell[CATG_N];
static lv_obj_t *s_catg_lbl[CATG_N];

static void ev_catg_click(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= CATG_N) return;
    tap_note(k_cat_short[i]);
    ev_pick_cat(i);
}

static void catgrid_build(lv_obj_t *p)
{
    for (int i = 0; i < CATG_N; i++) {
        int cx = CATG_X + (i % CATG_COLS) * (CATG_CW + CATG_GX);
        int cy = CATG_Y + (i / CATG_COLS) * (CATG_CH + CATG_GY);
        lv_obj_t *c = tick(p, cx, cy, CATG_CW, CATG_CH, C_PANEL, 8, NULL);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_border_color(c, C_LINE, 0);
        /* ★ 不用 tick() 的 tag 回调：这里要传【下标】，而 tag 是字符串。
         *   用 lv_event_get_user_data 带 intptr_t —— 与导航栏同一套做法。*/
        lv_obj_add_event_cb(c, ev_catg_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_catg_cell[i] = c;

        /* 3 个自定义入口用琥珀字（与胶囊条时代的口径一致：
         * 一眼分出「这三颗不是栏目，是快捷入口」）。*/
        lv_color_t col = (i < HCAT_REAL) ? C_CYAN : C_MUTED;
        s_catg_lbl[i] = label(c, 0, 7, F_TINY, col, k_cat_short[i]);
        /* ★ 居中三件套：显式给宽 + text_align=CENTER + 父对象内 x=0。
         *   少任何一条都会变成「贴着左边」（和导航栏那次同一个坑）。*/
        lv_obj_set_width(s_catg_lbl[i], CATG_CW);
        lv_obj_set_style_text_align(s_catg_lbl[i], LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void page_home_build(lv_obj_t *p)
{
    /* ---------- 正在播放 大卡（★ 2.8 重排：248×104 → 274×84，独占整行）----------
     *  兰兰 10-10：「屏幕小，首页重新排一下，正在播放页做小一点」，
     *  紧接着又定：「收藏和卡内在下面栏目里有了，右边专用收藏和本地按键不用了」
     *  ⇒ 右边那两张 90px 小卡【整组删掉】，横向空间全部还给这张卡。
     *  新账（内容区 280 × 212）：
     *    播放卡      x 6..280（274 宽） y 6..90
     *    提示行      y 92
     *    栏目网格    y 108 起，5 列 × 3 行（见 catgrid_build 的版面账）
     *  ⚠️ 高度 74 → 84 是为了把「删掉右卡」空出来的那截填上，
     *    让卡片区下沿停在 y=90，和提示行（92）只差 2px，不留大缝。*/
    lv_obj_t *card = tick(p, 6, 6, 274, 84, C_PANEL, 14, NULL);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, C_LINE, 0);
    lv_obj_add_event_cb(card, ev_open_play, LV_EVENT_CLICKED, NULL);

    label(card, 12, 5, F_TINY, C_CYAN, "● 正在播放");

    /* ★★ 10-11 重排（兰兰建议，采纳）：
     *   「首页正在播放栏，一共三行+频谱，但是频谱被压缩的很小。
     *     第三行电台所在地区栏目比特率可以放在正在播放标签的右边，
     *     这样频谱幅度就有了」
     *
     *  旧账（三行竖着栈）：只有 84 px 高的卡片里排
     *     行1 y=5  ● 正在播放
     *     行2 y=24 台名
     *     行3 y=50 来源
     *     频谱 y=68 高 12  ← 柱高被压成一条线，几乎看不出起伏
     *  新账（把来源行并入第一行，腾出整整一行给频谱）：
     *     行1 y=5   ● 正在播放(12..87，实测 75) + 来源(91..217，宽 126)
     *     行2 y=23  台名（宽 208，高 20）→ 23..43
     *     频谱 y=48 高 32 → 48..80（底距 4，与旧版一致）
     *  ⇒ 柱高 12 → 32，**频谱幅度 2.67 倍**。右侧圆钮(222,22,44×44)不动，
     *    与来源行右端 217 留 5 px 缝。
     *
     *  ⚠️ 来源行文案同时改了（必须改，不是顺手）：它原来是
     *     "%s · %s · 64 kbps" —— 「中国香港 · 音乐 · 64 kbps」实测 **165 px**，
     *     塞进这 126 px 必然被省略号吃掉大半。
     *     而且那个「64 kbps」是【写死的展示值】，跟流的真实码率毫无关系
     *     （真码率只有 app_radio 从流里解出来才知道）—— 展示假数字不如不展示。
     *     现在只写「地区 · 栏目」：最长「中国香港 · 音乐」= **99 px** ✓ 余 27。
     *     ⇒ 要真实码率的话应该由 app_radio 报出来，那是另一件事，不在本次范围。*/
    s_lbl_source = label(card, 91, 5, F_TINY, C_MUTED, "网络流 · 64 kbps");
    lv_obj_set_width(s_lbl_source, 126);
    lv_label_set_long_mode(s_lbl_source, LV_LABEL_LONG_DOT);

    /* ★ 台名必须用台名专用字库（st）：主字库只有 625 个界面文案字，
     *   台单 753 字，「圳涿 碚 禺 廣 華 藝 國」这些它没有 ⇒ 会出方块。
     * ★ 卡宽 274，台名可用宽 = 274-12-圆钮区(44+12) = 刚好 210
     *   ⇒ 16px 下放得下 13 个汉字，长台名基本不用省略号了。
     *   超长的仍靠 LONG_DOT 出省略号，绝不换行压到下面的频谱上。*/
    s_lbl_station = label(card, 12, 23, F_ST18, C_TEXT, "空闲");
    lv_obj_set_width(s_lbl_station, 208);
    lv_obj_set_height(s_lbl_station, 20);
    lv_label_set_long_mode(s_lbl_station, LV_LABEL_LONG_DOT);

    /* 迷你频谱：★ 10-11 —— 柱高 12 → 32。
     *   旧注释记的是「卡变矮了（104→84），柱高 18 → 12」——
     *   这次靠「来源行并入第一行」把那一行高度还给了频谱。
     *   宽度仍是 180（13 根柱子 × 12 px + 12 缝），柱宽不变。*/
    spectrum(card, 12, 48, 180, 32, C_GREEN);

    lv_obj_t *pb = tick(card, 222, 22, 44, 44, C_GREEN, 22, NULL);
    lv_obj_add_event_cb(pb, ev_home_play, LV_EVENT_CLICKED, NULL);
    s_playbtn_lbl = label(pb, 0, 0, F_TINY, C_ONDARK, "暂停");
    lv_obj_center(s_playbtn_lbl);

    /* ---------- 右侧两张小卡：★ 10-10 按兰兰要求【整组删除】----------
     *  兰兰原话：「收藏和卡内在下面栏目里有了，右边专用收藏和本地按键不用了」
     *
     *  删掉的是这两张（它们原本是 3.5 上放在 x=264 的）：
     *    · 「音乐库」   → 现在走栏目网格第 3 格「卡内」
     *    · 「最近听过」 → 现在走栏目网格第 2 格「历史」
     *  两张卡的功能在下面 5×3 网格里都有对应格子，
     *  首页顶部因此从「三分栏」变成「一栏通宽」——小屏上更清爽。
     *
     *  ⚠️ 句柄 s_lbl_sdinfo2 / s_lbl_histcnt 保留未删：
     *    home_cards_refresh() 里对它们有判空，不建就恒为 NULL，
     *    函数自然变成空转；将来要把卡加回来也是改这一处。
     *    ev_open_sd / ev_open_hist 同样保留（网格的格子还在用 ev_pick_cat
     *    走同名分支）。*/

    /* ---------- 栏目多列网格（★ 2.8 重排：横滑胶囊条 → 5×3 一次摊开）----------
     *  3.5 时代这里是一条 16 颗的横滑胶囊条 + ← → 翻页键
     *  （宽度 408 = 480 − 56 − 16）。2.8 内容区只有 280，
     *  同屏 5 颗时每颗 44px，兰兰要按好几下才能摸到「境外」。
     *  ⇒ 换成 catgrid_build（见本函数上方那段大注释）：5 列 × 3 行，
     *    15 格 = 收藏/历史/卡内 + 12 个栏目，一次全在眼前。
     *  ⚠️ s_strip_cat 保留定义（hstrip_show 里那句 `h == &s_strip_cat`
     *    分支还在用），但首页不再创建它 —— 发现页的省份条不受影响。*/
    /* ★ 2.8：文案要说准网格是 15 格（3 个专属 + 12 栏目），
     *   否则兰兰一数就发现「写 12 却有 15 格」。
     *   ⚠️ 用字限制：「专」「模」这两个字【主字库里没有】
     *     （字库是按 3.5 的界面文案烘的，2.8 新加的标题词不在其中）
     *     ⇒ 别写「3 专属」，会出方块。改成直接点名那三格。*/
    label(p, 6, 92, F_TINY, C_MUTED, "点格进全屏页 · 收藏/历史/卡内");
    catgrid_build(p);

    /* ---------- 电台网格：★ 10-10 按兰兰要求【从首页整体移除】----------
     *  兰兰原话：「首页不显示电台了，只有栏目」。
     *  做的事：
     *    ① 不建 s_lbl_list_title（原来那行「全部电台 · 共 N 台」）；
     *    ② grid_pages_ensure() 里 case 0 已删 ⇒ 首页不再建网格容器/格子；
     *    ③ goto_page() 里首页不再触发 ensure/destroy。
     *  ⚠️ 台单本身没动，只是入口改走上面的栏目网格 → 第 7 页全屏列表
     *    （那页 24 格 + 返回键，小屏上本来就该这么看）。
     *  ⚠️ 句柄 s_lbl_list_title / s_grid_home 保留：home_grid_refresh、
     *    on_grid_play 等处都有判空，删变量反而要改一堆地方。*/

    /* 两张小卡的下行文字（卡里几首歌 / 最近听的台）*/
    home_cards_refresh();
}

/* ============================================================
 *  v1.40：三套电台网格的「进页才建 / 离开就删」
 * ============================================================
 *  ★ 为什么单独拆出来而不是继续写在 page_*_build 里：
 *    那三个建页函数是【一次性】的（启动时全跑一遍，8 个页面对象全常驻）。
 *    要让网格离开就删，就得把建网格的动作从建页里摘出来，
 *    由 goto_page 在切页时按需调用。
 *
 *  ★ 三套网格与页号的对应（page_idx 从 0 数）：
 *      0 = 首页     3 行 × 3 列 =  9 格
 *      1 = 发现页   4 行 × 3 列 = 12 格
 *      6 = 分类页   8 行 × 3 列 = 24 格
 *
 *  ★ 销毁时必须把句柄置 NULL：网格的格子绑了 4 个事件回调
 *    （PRESSED/RELEASED/CLICKED/LONG_PRESSED），LVGL 随父容器
 *    一起删对象时会把回调摘掉，安全；但我们自己的 st_grid_t 里
 *    还留着 cell[]/nm[] 指针，置 NULL 才能保证「已删」这件事
 *    只由 s_grid_built[] 一个事实源说了算。
 *    —— 这与夜间页那条「三个定时器下一拍会往已释放句柄上写」
 *      是同一类错误（use-after-free），必须同标准要求。
 *
 *  ★★★ 为什么每套网格要套一个自己的容器（s_grid_box[]），不能直接
 *     lv_obj_clean(页面)：
 *     那个 API 会把页面下【所有】子对象删光 —— 包括首页的「正在播放」
 *     大卡、音乐库卡、最近听过卡、标题行，以及分类页的返回键。
 *     而这些对象是建页时建的、 supposed 常驻，删了就没了
 *     （首页网格重建时并不会重画那三张卡）。
 *     ⇒ 所以给网格一个专属父容器：容器铺满内容区 (0,0,CONT_W,CONT_H)
 *        且透明/不可滚/不收事件，网格在容器内沿用原来的 x/y，
 *        视觉与 v1.39 完全一致；销毁时只删这个容器。
 *        —— 这条是「删除的范围要精确到【谁建的】」，
 *          和网格句柄置 NULL 是同一条原则的两面。*/
static bool     s_grid_built[8];
static lv_obj_t *s_grid_box[8];

static void grid_pages_destroy(int page_idx)
{
    if (page_idx < 0 || page_idx >= 8 || !s_grid_built[page_idx]) return;
    s_grid_built[page_idx] = false;
    if (page_idx == 0) s_grid_home = NULL;
    else if (page_idx == 1) s_grid_find = NULL;
    else if (page_idx == 6) s_grid_list = NULL;
    /* ★ 只删自己那个容器。绝不能写 lv_obj_clean(s_pages[i]) —— 那样
     *   会连页面上的卡片、标题、返回键一起删掉（上一段注释详述）。*/
    if (s_grid_box[page_idx]) {
        lv_obj_del(s_grid_box[page_idx]);
        s_grid_box[page_idx] = NULL;
    }
    ESP_LOGI(TAG, "grid: 第 %d 页网格已删（池已还回）", page_idx);
}

/* ---- 建：三套网格。几何参数与 v1.39 完全一致，一个值都没改 ----
 *   ⚠️ 改这些 x/y/w/h 之前先想清楚：它们是 v1.39 刚按
 *   「横向账以父容器 CONT_W=424 为基准」重算过的（铁律 30），
 *   分类页尤其别改回 468 —— 那会让第三列整列画出页外。*/
static void grid_pages_ensure(int page_idx)
{
    if (page_idx < 0 || page_idx >= 8 || s_grid_built[page_idx]) return;
    /* 专属容器：铺满内容区、透明、不可滚、不吃事件。
     * 网格在它里面用【原来的 x/y】⇒ 视觉零变化。
     * ⚠️ 容器不能省 —— 删网格时只能删它，否则会把整页清空。*/
    lv_obj_t *bx = box(s_pages[page_idx], 0, 0, CONT_W, CONT_H, C_BG, 0);
    lv_obj_set_style_bg_opa(bx, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(bx, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bx, LV_OBJ_FLAG_CLICKABLE);
    s_grid_box[page_idx] = bx;

    switch (page_idx) {
    /* ★★★ 10-10：case 0（首页电台网格）【已删】。
     *   兰兰原话：「首页不显示电台了，只有栏目」——
     *   首页改成 5×3 的栏目网格（catgrid_build）后，这层网格没有任何入口。
     *   ⇒ 让它落到 default：刚建的容器 bx 当场删掉、s_grid_box[0] 置 NULL。
     *   ⚠️ goto_page 已改成一进首页不再调本函数，所以正常流程不会走到这里；
     *     留着 default 的兜底是为了「万一别处误调，别留一个半死的空容器」。*/
    case 1:
        /* ★★ 2.8 几何（内容区 280×212）：
         *   标题行 0..20 ｜ 地区条 22..56 ｜ 提示 58..72 ｜ 计数行 74..88
         *   网格 y=92..210（118 高）⇒ 4 行 × 间距 29（格高 25），两列 ×129 px */
        s_grid_find = grid_create_ex(bx, 8, 92, 264, 118, 4, 2);
        s_grid_find->on_play   = on_grid_play;
        s_grid_find->on_fav    = on_grid_fav;
        s_grid_find->from_page = 1;
        find_grid_refresh();
        break;
    case 6:
        /* ★★ 2.8 几何：返回+标题行 0..30 ｜ 网格 y=34..208（174 高）
         *   ⇒ 6 行 × 间距 29（格高 25），两列 ×129 px = 一屏 12 台 */
        s_grid_list = grid_create_ex(bx, 8, 34, 264, 174, 6, 2);
        s_grid_list->on_play   = on_grid_play;
        s_grid_list->on_fav    = on_grid_fav;
        /* ★ 返回键回第 6 页自己（它自己还有一个返回键，回首页/发现页）。
         *   兰兰要的是「要返回刚刚在发现页选择的地区栏」——
         *   而这个地区栏列表正是第 6 页，所以两层返回链是：
         *     播放页 →（本键）→ 第 6 页（还停在这一省/这一栏）→（‹ 返回）→ 发现/首页 */
        s_grid_list->from_page = 6;
        /* ★ 建完必须刷内容，否则进这一页看到的是【空网格】——
         *   原来它常驻，page_list_build 建完就是满的；
         *   改成按需建后，建完这一步没人做了。
         *   ⚠️ list_page_refresh 内部会判 s_grid_list，
         *   而这里已赋值 ⇒ 不会回调 ensure，递归安全。*/
        list_page_refresh();
        break;
    default:   /* 那几页没有网格：刚建的容器没用上，删掉别留空对象 */
        lv_obj_del(bx);
        s_grid_box[page_idx] = NULL;
        return;
    }
    s_grid_built[page_idx] = true;
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    /* ★★ 10-10：CLIB 模式下 lv_mem_monitor() 是空实现（total_size=0），
     *   free_size 恒 0 ⇒ 照旧判 free<8192 会【每一页】都刷一条假的
     *   "池只剩 0 B" E 日志，把真错误淹掉（10-10 就为这条假日志查了半天）。
     *   total_size==0 ⇒ 走系统堆，余量只看 IDF 堆（app_stlist 内存体检）。*/
    if (m.total_size == 0) {
        ESP_LOGI(TAG, "grid: 第 %d 页网格已建（内存来源=CLIB，余量看 IDF 堆）", page_idx);
    } else {
        ESP_LOGI(TAG, "grid: 第 %d 页网格已建，池 free=%u B（used %u%%）",
                 page_idx, (unsigned)m.free_size, (unsigned)m.used_pct);
        /* ★ 建完立刻体检：若建完就只剩不到 8 KB，说明这一页太重，
         *   下一次重绘极可能分配失败 —— 那种情况下宁可现在报出来，
         *   也不要等到渲染时崩在 lv_draw_add_task 里（那里看不出是谁）。*/
        if (m.free_size < 8192) {
            ESP_LOGE(TAG, "★★ 建完第 %d 页网格后池只剩 %u B ——"
                          "重绘时极可能 lv_malloc 返 NULL 而崩溃",
                     page_idx, (unsigned)m.free_size);
        }
    }
}

/* ============================================================
 *  第 6 页：城市声音集（SD 卡本地音频浏览）
 * ============================================================
 *  ★★ 10-11：这一页的目录列表从「死板 6 行」改成【可上下滑】。
 *    兰兰真机原话：「显示本目录 6 项，实际有更多的文件夹……6 个城市而已，
 *    其它无法显示，如何让文件夹能下滑显示，还是显示需要重新做一页」。
 *    —— 根因不是「滑不动」，是【压根没读进来】：原来那行是
 *       app_sd_list(path, ents, SD_ROWS) ⇒ 只读前 6 条，
 *       第 7 条起连目录项都没进过内存，屏上自然永远只有 6 个。
 *    修法（不改页面骨架，与电台网格同一套虚拟化手法，见 grid_create_ex）：
 *      ① 一次读进 SD_LIST_MAX 条（超出部分截断，日志会报）；
 *      ② 这 6 行对象【循环复用】—— 滚动只改 y 和文字，一个对象都不新增；
 *      ③ 用一个 spacer 顶住内容高度，否则 LVGL 按子对象包围盒算不出可滚范围
 *         （行会被搬回可视区，包围盒永远只有一屏 ⇒ 滚一下就弹回顶部）。
 * ============================================================ */
#define SD_ROWS     6     /* 一屏 6 行（可视区 136 px ÷ 步进 22 = 6.18）*/
#define SD_ROW_H   22     /* 行步进：行高 20 + 缝 2（改这里要同步 page_sd_build）*/
#define SD_LIST_MAX 96    /* 一次读进多少条（96 × 268 B ≈ 25.7 KB，走 PSRAM）*/

static lv_obj_t *s_sd_icon[SD_ROWS];
static lv_obj_t *s_sd_name[SD_ROWS];
static lv_obj_t *s_sd_size[SD_ROWS];
static lv_obj_t *s_sd_row[SD_ROWS];
static lv_obj_t *s_lbl_sd_path;
static lv_obj_t *s_lbl_sd_info;
static lv_obj_t *s_lbl_sd_play;      /* 「● 正在播放：xxx」一行 */
static lv_obj_t *s_lbl_sd_scroll;    /* ★ 10-11：右上「滑动看更多」（能滚才显示）*/

/* 按【UTF-8 字符】截断复制（不是按字节）。
 * ★ 为什么不能用 snprintf("%.*s")：那是按字节截。中文 3 字节/字，
 *   正好切在字符中间就会留下半个 UTF-8 序列 ⇒ 屏上多一个乱码字。
 *   卡里的中文长文件名很常见，所以这里老老实实按字符数截。*/
static void utf8_copy_n(char *dst, size_t dstsz, const char *src, int max_chars)
{
    int    n = 0;
    size_t o = 0;
    while (src && *src && n < max_chars) {
        unsigned char c = (unsigned char)*src;
        int len = 1;
        if      ((c & 0x80) == 0x00) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (o + (size_t)len + 1 > dstsz) break;
        for (int i = 0; i < len; i++) {
            if (!src[i]) { dst[o] = '\0'; return; }
            dst[o++] = src[i];
        }
        src += len;
        n++;
    }
    dst[o] = '\0';
}

/* ★★ 10-11：目录项缓冲改成【PSRAM + 懒分配】。
 *   为什么不是 static 数组：app_sd_entry_t 带 name[260] ⇒ 268 B/条，
 *   96 条静态就是 25.7 KB 常驻内部 RAM（.bss），而内部 RAM 开机后只剩
 *   17 万字节左右 —— 这类"只在 SD 页用"的缓冲丢 PSRAM 更合适。
 *   ⚠️ 失败时退回内部堆（宁可少显示几页，也不要整页空白）。*/
static app_sd_entry_t *s_sd_ents  = NULL;   /* 本目录的条目（PSRAM）*/
static int             s_sd_cnt   = 0;      /* 本目录读到几条 */
static int             s_sd_first = 0;      /* 第几条排在屏上第一行 */
static lv_obj_t       *s_sd_cont  = NULL;   /* 滚动容器 */
static lv_obj_t       *s_sd_spacer = NULL;  /* 撑内容高度（见 grid_create_ex）*/
static char            s_sd_path[256] = "/sdcard";
static char            s_sd_prev[256] = "";

/* 按 s_sd_first 把 6 行重填一遍（滚动时只调这个：不建/不删对象）
 * ★ 行对象的 y 必须写成 idx * SD_ROW_H（而不是 (i)*SD_ROW_H）：
 *   行是"贴着内容坐标"摆的，LVGL 再减去 scroll_y 得到屏上位置；
 *   写 idx 才能让它始终落在可视带里，同时保留不足一行的那点位移（滚动才顺）。*/
static void sd_rows_fill(void)
{
    for (int i = 0; i < SD_ROWS; i++) {
        int idx = s_sd_first + i;
        lv_obj_t *r = s_sd_row[i];
        if (!r) continue;
        lv_obj_set_y(r, idx * SD_ROW_H);
        if (s_sd_ents && idx < s_sd_cnt) {
            const app_sd_entry_t *it = &s_sd_ents[idx];
            /* ★ 图标只能用字库里真有的符号（◎ ● ◆ ★ …）*/
            if (s_sd_icon[i]) lv_label_set_text(s_sd_icon[i], it->is_dir ? "◎" : "●");
            if (s_sd_name[i]) {
                /* 按字符（不是按字节）截断：中文长文件名很常见，
                 * 按字节截会把一个字切成半个 UTF-8 序列，屏上就多个乱码字 */
                char buf[96];
                utf8_copy_n(buf, sizeof(buf), it->name, 16);
                lv_label_set_text(s_sd_name[i], buf);
            }
            if (s_sd_size[i]) {
                if (it->is_dir) lv_label_set_text(s_sd_size[i], "目录 →");
                else            lv_label_set_text_fmt(s_sd_size[i], "%u KB",
                                                      (unsigned)(it->size / 1024));
            }
            lv_obj_clear_flag(r, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (s_sd_name[i]) lv_label_set_text(s_sd_name[i], "");
            if (s_sd_size[i]) lv_label_set_text(s_sd_size[i], "");
            if (s_sd_icon[i]) lv_label_set_text(s_sd_icon[i], "");
        }
        lv_obj_set_style_bg_color(r, C_PANEL, 0);
    }
}

/* 滚动回调：算出「第几条第几行在最上面」，变了才重填
 * （否则每滚一个像素就重排 6 行 × 4 个 label，白费 CPU）*/
static void sd_scroll_cb(lv_event_t *e)
{
    (void)e;
    if (!s_sd_cont || s_sd_cnt <= 0) return;
    int max_first = s_sd_cnt - SD_ROWS;
    if (max_first < 0) max_first = 0;
    int f = lv_obj_get_scroll_y(s_sd_cont) / SD_ROW_H;
    if (f < 0) f = 0;
    if (f > max_first) f = max_first;
    if (f == s_sd_first) return;
    s_sd_first = f;
    sd_rows_fill();
}

/* 把滚动条/提示同步到「这一目录能不能滚」：
 * 能滚 → 右上角写「滑动看更多」；不能滚 → 清空。
 * ★ 用字必须先在 xs_font_cjk12 里查过（_ui_width.py）：
 *   这个字库是【按卡里的语料烘的】，缺 一/不/上/下/个 ——
 *   写「上下滑」会直接画成方块。「滑动看更多」六个字都在（实测）。*/
static void sd_scroll_hint_refresh(void)
{
    if (!s_lbl_sd_scroll) return;
    bool can = (s_sd_cnt > SD_ROWS);
    lv_label_set_text(s_lbl_sd_scroll, can ? "滑动看更多" : "");
}

static void sd_row_clicked(lv_event_t *e)
{
    /* ★ 10-11：行对象是【循环复用】的 ⇒ 屏上「第 slot 行」= 第 (s_sd_first+slot) 条，
     *   不能再把行号当成条目下标（那会变成"点第 3 行却播了别的文件"）。*/
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    if (slot < 0 || slot >= SD_ROWS) return;
    int r = s_sd_first + slot;
    if (r < 0 || r >= s_sd_cnt || s_sd_ents == NULL) return;
    const app_sd_entry_t *it = &s_sd_ents[r];
    tap_note(it->name);

    if (it->is_dir) {
        /* 进子目录。
         * ★ 必须给 %s 加精度上限（%.250s）：gcc 的 -Wformat-truncation 会按
         *   「s_sd_path 最大 255 + '/' + it->name 最大 259」算最坏情况，
         *   目标缓冲区开多大都可能被判「可能截断」，而 IDF 默认 -Werror → 编译不过。
         *   加了精度它就能算出上界 250+1+250=501，缓冲区 600 足够，警告消失。*/
        char next[600];
        snprintf(next, sizeof(next), "%.250s/%.250s", s_sd_path, it->name);
        strncpy(s_sd_prev, s_sd_path, sizeof(s_sd_prev) - 1);
        s_sd_prev[sizeof(s_sd_prev) - 1] = '\0';
        strncpy(s_sd_path, next, sizeof(s_sd_path) - 1);
        s_sd_path[sizeof(s_sd_path) - 1] = '\0';
        ui_sd_page_refresh();
    } else {
        /* 音频文件：交给播放器（解码 -> I2S -> 喇叭）。
         * ★ 路径拼接同样要加 %s 精度上限，理由见上面目录分支的注释。*/
        char full[600];
        snprintf(full, sizeof(full), "%.250s/%.250s", s_sd_path, it->name);

        for (int i = 0; i < SD_ROWS; i++) {
            if (s_sd_row[i]) lv_obj_set_style_bg_color(s_sd_row[i], C_PANEL, 0);
        }
        if (s_sd_row[slot]) lv_obj_set_style_bg_color(s_sd_row[slot], C_HILITE, 0);

        /* 记住目录：播放页的「上一个 / 下一个」要在同一目录里换曲 */
        snprintf(s_local_dir, sizeof(s_local_dir), "%.250s", s_sd_path);

        ESP_LOGI(TAG, "SD file picked: %s (%u B)", full, (unsigned)it->size);

        esp_err_t pr = app_radio_play_file(full);
        ESP_LOGI(TAG, "play_file -> %s", esp_err_to_name(pr));

        if (s_lbl_sd_play) {
            if (pr == ESP_OK) {
                char nm[96];
                utf8_copy_n(nm, sizeof(nm), it->name, 14);
                lv_label_set_text_fmt(s_lbl_sd_play, "● 正在播放：%s", nm);
            } else {
                lv_label_set_text_fmt(s_lbl_sd_play, "播放失败 (%s)",
                                      esp_err_to_name(pr));
            }
        }

        /* ★ 关键：点了就【跳到播放页】。
         *   兰兰 10-03 反馈「按一下本地音频文件没有对应的播放」——
         *   上一版点了只在 SD 页那一行写个字，屏幕上什么都没有变化，
         *   所以体感就像「没反应」。现在直接把用户带到正在播放的界面。
         *   返回键回 SD 页（from=5），不是回首页。*/
        s_play_from = 5;
        player_refresh();
        goto_page(2);
    }
}

/* ============================================================
 *  台单热重载后重建 UI（10-06）
 * ============================================================
 *  串口敲 st_reload 之后调这个：把三套网格全拆掉再重建，
 *  否则界面上还是旧的台单名字和旧的台数。
 *
 *  ★ 为什么用 grid_pages_destroy + grid_pages_ensure（v1.40 建立的机制）
 *    而不是重新 ui_init()：ui_init 会重建整个界面，代价大且会闪屏；
 *    网格机制本来就是为「按需建/按需拆」设计的，正好复用。
 *
 *  ★ LVGL 线程安全：串口命令任务不是 LVGL 任务，直接调 lv_obj_del 会炸。
 *    所以这里只【置脏标记】，真正的重建交给 LVGL 自己的定时器
 *    （见 ui_tick 里的处理），这和本文件其它地方的约定一致。
 */
static bool s_stations_dirty = false;

static void st_reload_apply(void)
{
    for (int idx = 0; idx < 8; idx++) {
        if (!s_grid_built[idx]) continue;
        grid_pages_destroy(idx);
        grid_pages_ensure(idx);
    }
    /* 收藏/历史不是独立页面，是网格里的格子 —— grid_pages_ensure() 里
     * 会顺带调 home_grid_refresh() / find_grid_refresh() 重建它们，
     * 所以这里不用另外刷（v1.45 实测：app_fav/app_hist 都走 app_st_get，
     * 名字变了它们下次打开自然就是新的）。*/
    ESP_LOGI(TAG, "台单已重载，网格重建完成");
}

void ui_reload_stations(void)
{
    s_stations_dirty = true;
    /* 兜底：如果当前不在任何网格页，立刻重建，别等下一次 tick */
    if (s_grid_built[0] || s_grid_built[1] || s_grid_built[6])
        st_reload_apply();
    s_stations_dirty = false;
}

void ui_sd_page_refresh(void)
{
    if (!s_lbl_sd_path) return;

    if (!app_sd_is_mounted()) {
        lv_label_set_text(s_lbl_sd_path, "未检测到内存卡");
        lv_label_set_text(s_lbl_sd_info, "请插入 FAT32 格式的 TF 卡后重试");
        s_sd_cnt   = 0;
        s_sd_first = 0;
        for (int i = 0; i < SD_ROWS; i++) {
            if (s_sd_name[i]) lv_label_set_text(s_sd_name[i], "");
            if (s_sd_size[i]) lv_label_set_text(s_sd_size[i], "");
            if (s_sd_icon[i]) lv_label_set_text(s_sd_icon[i], "");
            if (s_sd_row[i])  lv_obj_set_style_bg_color(s_sd_row[i], C_PANEL, 0);
        }
        if (s_sd_spacer) lv_obj_set_size(s_sd_spacer, 1, SD_ROWS * SD_ROW_H);
        sd_scroll_hint_refresh();
        return;
    }

    /* ★ 10-11：一次读进 SD_LIST_MAX 条。
     *   原来这里是 app_sd_list(..., SD_ROWS) ⇒ 永远只有前 6 条，
     *   第 7 条起连内存都没进过 —— 兰兰看到的"只有 6 个城市"就是这么来的。
     *   缓冲懒分配：只在第一次进这一页时申请（25.7 KB PSRAM）。*/
    if (s_sd_ents == NULL) {
        size_t need = (size_t)SD_LIST_MAX * sizeof(app_sd_entry_t);
        s_sd_ents = heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
        if (!s_sd_ents) s_sd_ents = heap_caps_malloc(need, MALLOC_CAP_8BIT);
        ESP_LOGI(TAG, "SD 目录缓冲：%d 条 × %u B = %u KB（%s）",
                 SD_LIST_MAX, (unsigned)sizeof(app_sd_entry_t),
                 (unsigned)(need / 1024), s_sd_ents ? "已分配" : "★失败");
        if (!s_sd_ents) {
            lv_label_set_text(s_lbl_sd_info, "内存不足：目录列表开不出来");
            s_sd_cnt = 0;
            sd_scroll_hint_refresh();
            return;
        }
    }

    int n = app_sd_list(s_sd_path, s_sd_ents, SD_LIST_MAX);
    if (n < 0) n = 0;
    s_sd_cnt = n;
    if (n >= SD_LIST_MAX) {
        ESP_LOGW(TAG, "本目录条目 ≥ %d 条，只读进前 %d 条", SD_LIST_MAX, SD_LIST_MAX);
    }

    uint32_t mb = 0;
    const char *fs = "?";
    app_sd_get_info(&mb, &fs);

    /* 全卡音频数：只在置脏时重扫，否则用缓存（见文件上方 s_sd_count_dirty 注释）*/
    s_sd_audio_total = app_sd_audio_total(s_sd_count_dirty);
    s_sd_count_dirty = false;

    lv_label_set_text_fmt(s_lbl_sd_path, "%s", s_sd_path);
    /* ★ 这一行原来写「%s · %u MB · 本目录 %d 项 · 全卡音频 %d 首」，
     *   实测 339 px 而限宽 268 px（_ui_width.py），尾巴整段被 LONG_DOT 吃掉，
     *   等于白写；而「全卡音频 N 首」在首页「音乐库」卡上本来就有。
     *   ⇒ 只留「卡格式 + 容量 + 本目录条数」= 220 px（含 3 位方案仍稳进），
     *     能不能滚由右上角「滑动看更多」提示。*/
    lv_label_set_text_fmt(s_lbl_sd_info, "%s · %u MB · 本目录 %d 项",
                          fs, (unsigned)mb, n);

    /* 内容高度先顶好，再回顶部，最后重排行（顺序反了会被 LVGL 钳一次滚动位置）*/
    if (s_sd_spacer) {
        int content_h = n * SD_ROW_H;
        int min_h     = SD_ROWS * SD_ROW_H;
        if (content_h < min_h) content_h = min_h;
        lv_obj_set_size(s_sd_spacer, 1, content_h);
    }
    s_sd_first = 0;
    if (s_sd_cont) lv_obj_scroll_to_y(s_sd_cont, 0, LV_ANIM_OFF);
    sd_rows_fill();
    sd_scroll_hint_refresh();

    /* 播放状态回填：从其它页切回 SD 页时也要显示对 */
    if (s_lbl_sd_play) {
        const char *np = app_radio_now_playing();
        if (np && np[0]) {
            char nm[96];
            utf8_copy_n(nm, sizeof(nm), np, 14);
            lv_label_set_text_fmt(s_lbl_sd_play, "● 正在播放：%s", nm);
        } else {
            lv_label_set_text(s_lbl_sd_play, "");
        }
    }

    ESP_LOGI(TAG, "SD page: %s -> %d 条（一屏 %d 行，%s）",
             s_sd_path, n, SD_ROWS, n > SD_ROWS ? "可上下滑" : "不用滑");
}

static void ev_sd_up(lv_event_t *e)
{
    (void)e;
    tap_note(s_sd_prev[0] ? "返回上级" : "返回首页");
    if (s_sd_prev[0]) {
        strncpy(s_sd_path, s_sd_prev, sizeof(s_sd_path) - 1);
        s_sd_path[sizeof(s_sd_path) - 1] = '\0';
        s_sd_prev[0] = '\0';
        ui_sd_page_refresh();
    } else {
        goto_page(0);
    }
}

static void page_sd_build(lv_obj_t *p)
{
    /* ★★ 2.8 重排（10-10）：3.5 那版是「标题 8,8（22px）+ 返回键 x=328」，
     *   返回键整颗在 280 宽的页外（328 > 280）—— 兰兰报的「看不见的按键」。
     *   新账（页高 212）：
     *     2..20   标题（18px）+ 返回键（右）
     *     26..40  路径（左）｜ 同一行的右上：滚动提示
     *     42..56  卡信息 ｜ 58..72 播放状态
     *     76..212 6 行列表（★ 10-11 起是【可滚动容器】，
     *                       步进 22、行高 20 ⇒ 一屏正好 6 行，
     *                       条目多于一屏时上下滑，行对象循环复用）*/
    label(p, 6, 2, F_MID, C_TEXT, "城市声音集");
    lv_obj_t *st = label(p, 118, 5, F_SD, C_CYAN, "本地音频");
    lv_obj_set_width(st, 84);

    lv_obj_t *back = tick(p, 204, 2, 72, 20, C_HILITE, 10, NULL);
    lv_obj_add_event_cb(back, ev_sd_up, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bt = label(back, 0, 0, F_SD, C_TEXT, "← 返回");
    lv_obj_center(bt);

    /* 路径 / 卡信息 / 播放状态
     * ★ 这三行都必须用 F_SD（大字符集）：它们会显示【卡里的名字】，
     *   小字库遇到源码里没出现过的汉字只能画方块。
     * ★ 三行都必须限宽 + DOT：卡里的名字长度不可控，不限宽会横向溢出到屏外。*/
    s_lbl_sd_path = label(p, 6, 26, F_SD, C_GREEN, "/sdcard");
    lv_obj_set_width(s_lbl_sd_path, 184);   /* ★ 10-11：268 → 184，右边留给滚动提示 */
    lv_label_set_long_mode(s_lbl_sd_path, LV_LABEL_LONG_DOT);

    /* ★ 10-11：右上角滚动提示（只在内容超过一屏时出现）。
     *   为什么另开一个 label 而不并进下面那行：信息行已经 220 px，
     *   再挂 42 px 的提示会顶到 268 px 上限，换台卡多两位数字就溢出。*/
    s_lbl_sd_scroll = label(p, 190, 26, F_SD, C_AMBER, "");
    lv_obj_set_width(s_lbl_sd_scroll, 84);
    lv_obj_set_style_text_align(s_lbl_sd_scroll, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(s_lbl_sd_scroll, LV_LABEL_LONG_DOT);

    s_lbl_sd_info = label(p, 6, 42, F_SD, C_MUTED, "读取中…");
    lv_obj_set_width(s_lbl_sd_info, 268);
    lv_label_set_long_mode(s_lbl_sd_info, LV_LABEL_LONG_DOT);

    s_lbl_sd_play = label(p, 6, 58, F_SD, C_GREEN, "");
    lv_obj_set_width(s_lbl_sd_play, 268);
    lv_label_set_long_mode(s_lbl_sd_play, LV_LABEL_LONG_DOT);

    /* 6 行列表 —— ★★ 10-11：放进【可滚动容器】，上下滑看更多。
     *   手法与电台网格完全一致（见 grid_create_ex / grid_scroll_cb）：
     *     容器可滚(VER) + 6 行循环复用 + spacer 顶内容高度。
     *   ⚠️ 行的父对象从「页面 p」变成「容器 s_sd_cont」⇒ x 照旧 6，
     *      y 由 sd_rows_fill() 按「条目序号 × 步进」写（滚动位置由 LVGL 叠加）。*/
    s_sd_cont = lv_obj_create(p);
    lv_obj_remove_style_all(s_sd_cont);
    lv_obj_set_pos(s_sd_cont, 0, 76);
    lv_obj_set_size(s_sd_cont, CONT_W, CONT_H - 76);
    lv_obj_set_style_bg_opa(s_sd_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_sd_cont, 0, 0);
    lv_obj_add_flag(s_sd_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_sd_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_sd_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(s_sd_cont, C_LINE, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_sd_cont, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(s_sd_cont, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(s_sd_cont, 2, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(s_sd_cont, sd_scroll_cb, LV_EVENT_SCROLL, NULL);

    for (int i = 0; i < SD_ROWS; i++) {
        int y = i * SD_ROW_H;
        lv_obj_t *r = tick(s_sd_cont, 6, y, 268, 20, C_PANEL, 8, NULL);
        lv_obj_add_event_cb(r, sd_row_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_sd_row[i] = r;

        s_sd_icon[i] = label(r, 6, 2, F_SD, C_CYAN, "");
        s_sd_name[i] = label(r, 24, 2, F_SD, C_TEXT, "");
        lv_obj_set_width(s_sd_name[i], 150);
        lv_label_set_long_mode(s_sd_name[i], LV_LABEL_LONG_DOT);
        s_sd_size[i] = label(r, 0, 2, F_SD, C_MUTED, "");
        lv_obj_set_width(s_sd_size[i], 262);
        lv_obj_set_style_text_align(s_sd_size[i], LV_TEXT_ALIGN_RIGHT, 0);
    }

    /* spacer：LVGL 按【子对象包围盒】算可滚范围，而行会随滚动被搬回可视区
     * （包围盒永远只有一屏）⇒ 必须有个"永远在最底下"的高对象把内容顶起来。
     * 高度在 ui_sd_page_refresh() 里按条目数设。*/
    s_sd_spacer = box(s_sd_cont, 0, 0, 1, SD_ROWS * SD_ROW_H, C_BG, 0);
    lv_obj_set_style_bg_opa(s_sd_spacer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_sd_spacer, LV_OBJ_FLAG_CLICKABLE);
}

/* ============================================================
 *  页面二：发现（省份筛选 + 双列网格）
 * ============================================================
 *  10-03 第八次：这页原来在台名里做子串匹配找城市名（一个省配一组关键词），
 *    只能覆盖 7 个省，而且台单一换就得重配关键词。
 *  第九次：台单里每条台本来就带【省份字段】（_gen_stations.py 照抄网页版的
 *    c 字段），所以直接按字段筛 —— 31 个省全都能选，一个不漏、一个不错。
 *
 *  ★ 兰兰 10-03 原话：「发现页，地区无法向右边滑动选择，造成只有几个地区
 *    可以选，还是和首页一样最好双列显示，直观更容易」
 *    → 省份条改成横滑（32 个胶囊：全部 + 31 省），结果区是双列网格。
 *    点省份也是进第 7 页全屏看（跟首页栏目一致的操作逻辑）。
 */

static lv_obj_t *s_lbl_find_title;
static int       s_fr_national = 0;   /* 全国台几个 */

/* 点省份胶囊 → 进第 7 页全屏看这一省 */
static void ev_pick_prov(int prov)
{
    if (prov < 0 || prov >= NET_PROV_N) return;
    s_list_mode  = LIST_BY_PROV;
    s_list_prov  = prov;
    s_list_title = g_prov_name[prov];
    s_list_back  = 1;
    tap_note(g_prov_name[prov]);
    list_page_refresh();
    goto_page(6);
}

/* 发现页那层网格：默认显示【央广 / 国广 / 无省份的全国台】。
 * ★ 为什么不给「全部」：那跟首页那一层完全重复，两个页面看一样的东西
 *   就等于白占一屏。发现页的价值是「按地方找」，
 *   而这层先给覆盖面最广的那批当入口，想按省就点上面的省份条。
 *   两遍扫描：第一遍收全国台，第二遍才收地方台（保证全国台一定在最前）。*/
static void find_grid_refresh(void)
{
    static int pool[GRID_MAX];
    int n = 0;
    int total = app_radio_station_count();
    for (int i = 0; i < total && n < GRID_MAX; i++) {
        if (app_st_get(i)->prov == NET_PROV_NONE) pool[n++] = i;
    }
    s_fr_national = n;
    for (int i = 0; i < total && n < GRID_MAX; i++) {
        if (app_st_get(i)->prov != NET_PROV_NONE) pool[n++] = i;
    }
    /* ★ v1.40：网格按需建 ⇒ 可能为 NULL（页面刚建时先于 ensure 调用）。
     *   同 home_grid_refresh，判空 + 兜底建。*/
    if (!s_grid_find) {
        ESP_LOGW(TAG, "find: 网格还没建 —— 先建再填台");
        grid_pages_ensure(1);
        if (!s_grid_find) return;
    }
    grid_set(s_grid_find, pool, n);
    if (s_lbl_find_title) {
        /* ★★ 10-10 修一处「数字在说谎」：原来一律写「全部 %d 个」，而 %d
         *   是【装进网格的条数 n】，n 被 GRID_MAX(512) 截过。
         *   真机 1253 台 ⇒ 这一行会写「全部 512 个」，看起来像台单只有 512 台。
         *   ⇒ 分清两个量：shown（网格里真有几条）/ all（台单里共几条），
         *     截断时如实写成「显示 512/1253」。
         *   ⚠️ 为什么不直接把 GRID_MAX 提到 1400：pool 是函数内 static int[]，
         *     两处（发现页 + 全屏列表页）就多占 7 KB 内部 RAM，
         *     而内部 SRAM 正是上一轮 boot loop 的瓶颈。宁可如实标注。*/
        int all = app_radio_station_count();
        /* ★ 10-11：去掉「· 上下滑」尾巴（占 62 px）。14px 下这两句实测
         *   166 / 189 px，限宽 268 ⇒ 余 79~102 px，单行稳稳放得下。*/
        if (all > n) {
            lv_label_set_text_fmt(s_lbl_find_title, "全国 %d · 显示 %d/%d",
                                  s_fr_national, n, all);
        } else {
            lv_label_set_text_fmt(s_lbl_find_title, "全国台 %d 个 · 全部 %d 个",
                                  s_fr_national, n);
        }
    }
    ESP_LOGI(TAG, "find grid: national=%d total=%d", s_fr_national, n);
}

/* ============================================================
 *  发现页右上角「海外」入口 —— 二级
 * ============================================================
 *  ★ 兰兰 10-03 第十二次：
 *   「发现页海外标签点中以后所有电台都集中在一页太多了，
 *     我的自用版HTML或者exe的海外版有详细分类的，
 *     北美欧洲澳洲香港台湾都分开的」
 *
 *   ⇒ 网页版（XianHai.html）的地区就是这 11 个：
 *     中国台湾 / 中国香港 / 中国澳门 / 其他华语 / 北美 / 欧洲 /
 *     日韩 / 新马 / 东南亚 / 大洋洲
 *     固件台单里【已经是分开的】（g_prov_name 的 31~41 项），
 *     所以只要把这个子分类列表摆出来就是网页版那个结构。
 *
 *   做法：第 7 页（全屏列表）的网格换成「先选地区、点了再看台」——
 *     用现成的 st_grid_t 画 11 个地区块，每块显示「地区名 + 台数」。
 *     不新建页面、不多占 lv_obj：复用第 7 页那套虚拟网格。
 */

/* 11 个海外地区在 g_prov_name 里的下标（= 31..41，与台单生成一致）*/
#define OVS_PROV_FIRST NET_PROV_CN_N     /* 31 */

static int s_ovs_pick = -1;              /* 已选的海外地区下标；-1 = 正在选地区 */

static void ovs_slot_clicked(int slot, int idx);   /* 下面定义，这里先引用 */

/* 在第 7 页画「海外地区块」：每块 = 地区名 + 台数 */
static void ovs_grid_refresh(void)
{
    static int pool[GRID_MAX];
    int n = 0;
    int total = app_radio_station_count();

    for (int pv = OVS_PROV_FIRST; pv < NET_PROV_N && n < GRID_MAX; pv++) {
        int cnt = 0;
        for (int i = 0; i < total; i++) {
            if (app_st_get(i)->prov == pv) cnt++;
        }
        if (cnt > 0) pool[n++] = pv;      /* pool 存的是【地区下标】，不是台下标 */
    }
    s_list_total = n;
    if (s_lbl_list_title2) {
        /* ★ 10-11：原来写「海外与港澳台 · %d 个地区 · 点一个看它的台」＝实测 190 px，
         *   塞进 148 px 的标题里必然折行压住下面的地区块（兰兰报的就是这个）。
         *   改成「海外地区 %d 个」＝124 px，余 24 px。
         *   「点一个看它的台」这类说明不用写在标题里 —— 格子本身就是可点的。*/
        lv_label_set_text_fmt(s_lbl_list_title2, "海外地区 %d 个", n);
    }
    /* ★ v1.40：网格改成按需建，所以这里必须判空。
     *   ovs_grid_refresh() 由「点海外按钮」触发，而那个按钮在发现页上 ——
     *   若此时【还没进过第 6 页】，s_grid_list 就是 NULL（以前它常驻，
     *   永远不会为空，所以旧代码没判）。不加判空就是往 NULL 写字段。*/
    if (!s_grid_list) {
        ESP_LOGW(TAG, "ovs: 第 6 页网格还没建（未进过该页）—— 先建再填地区");
        grid_pages_ensure(6);
        if (!s_grid_list) return;
    }
    s_grid_list->mode = 1;               /* 这一层画的是地区块，不是台 */
    /* ★ 10-11：这一层不是「可删列表」⇒ del_mode 必须归零，
     *   否则从「历史页」退回来点海外（同一个网格对象）会带着删标记。*/
    s_grid_list->del_mode  = 0;
    s_grid_list->on_region = ovs_slot_clicked;
    if (s_btn_list_clear) lv_obj_add_flag(s_btn_list_clear, LV_OBJ_FLAG_HIDDEN);
    if (s_lbl_list_hint)  lv_obj_clear_flag(s_lbl_list_hint, LV_OBJ_FLAG_HIDDEN);
    grid_set(s_grid_list, pool, n);
    ESP_LOGI(TAG, "ovs regions: %d", n);
}

/* 网格里点一块：按当前模式分派（海外地区 vs 台名）*/
/* ★★★★ 10-05 真机 bug（兰兰：「点最后一个海外地区，跳到国内电台了」）：
     *   原来这里读的是【全局 s_pool[idx】——那是首页 s_grid_home 的池子。
     *   而 ovs_grid_refresh 把海外地区填进的是它的【局部 pool[]】，
     *   grid_set 已经把数据复制进 s_grid_list->pool[]（见 st_grid_t.pool）。
     *   ⇒ 点第N 格时读的是首页缓存的第 N 条 ⇒ 「北美」点到「欧洲」、
     *     甚至点到国内的台，完全串位。
     *   ⇒ 改成读【自己那个网格】的池子：s_grid_list->pool[idx]。
     *   ★ 这类 bug 的教训：同一个「池」概念在代码里有两个实例时，
     *     回调必须明确用哪一个，不能凭函数名里的 pool 想当然。*/
static void ovs_slot_clicked(int slot, int idx)
{
    (void)slot;
    /* ★★ 10-05 修正两次才改对，这里必须钉死：
     *   `idx` 【已经是 prov 索引】（grid_show_cell 里 `g->idx[slot] = station_idx`，
     *   而 mode==1 时 pool[] 装的就是 prov，见 ovs_grid_refresh）。
     *   ⇒ 不要再 `pool[idx]` 反查一次，那会取到别的台的下标。
     *   ⇒ 也【不能拿 s_grid_list->total 做上界】：total 是「地区个数」（≤11），
     *   而海外 prov 从 OVS_PROV_FIRST(=31) 起算，31 >= 11 恒成立 ⇒ 点了全被拦掉，
     *   表现为「点海外地区没反应」。上界只能是 NET_PROV_N。*/
    if (idx < OVS_PROV_FIRST || idx >= NET_PROV_N) return;
    int pv = idx;
    s_ovs_pick = pv;
    s_list_mode  = LIST_BY_PROV;
    s_list_prov  = pv;
    s_list_title = g_prov_name[pv];
    s_list_back  = 6;                 /* 返回键回「海外地区选择」这一层 */
    tap_note(g_prov_name[pv]);
    list_page_refresh();
}

static void ev_pick_overseas(lv_event_t *e)
{
    (void)e;
    s_ovs_pick   = -1;
    s_list_back  = 1;                 /* 返回键回发现页 */
    list_page_back_to_ovs();
}

/* ★ 点「海外」→ 进第 7 页但显示地区块（不是台）*/
static void list_page_back_to_ovs(void)
{
    s_list_mode  = LIST_BY_OVS_REGION;
    s_list_title = "海外与港澳台";
    goto_page(6);
    ovs_grid_refresh();
}

static void page_find_build(lv_obj_t *p)
{
    /* ---------- 省份条（横向可滑 · 虚拟）----------
     * ★ 42 个地区每个一颗实体胶囊 = 84 个 lv_obj ≈ 34 KB，
     *   而 LVGL 池只有 103 KB（同屏还有两套网格）⇒ 必然抽干。
     *   所以是 hstrip：只建 7 颗，滑到哪填到哪。*/
    /* ---------- 标题行（2.8：整行高 20，标签左、海外键右）---------- */
    label(p, 6, 2, F_TINY, C_MUTED, "地区");

    /* 海外直达（右上角）。琥珀色与地区条里那些海外胶囊同色，一眼认得出。
     *   ⚠️ tick() 的第 8 参只是「日志 tag」，不是回调 —— 靠它做点击是接不上的，
     *   必须自己 lv_obj_add_event_cb。这个坑我差点又踩。
     * ★ 2.8：x 330 → 200（330 在 280 宽的页里完全在屏外）、宽 86 → 74、高 22 → 20。*/
    lv_obj_t *ovs = tick(p, 200, 2, 74, 20, lv_color_hex(0x3A2E12), 10, "海外");
    lv_obj_add_event_cb(ovs, ev_pick_overseas, LV_EVENT_CLICKED, NULL);
    /* 台数实时数一遍，不写死 —— 台单一改，硬编码的 194 就变成谎话 */
    int ovs_n = 0, tot = app_radio_station_count();
    for (int i = 0; i < tot; i++) {
        unsigned char pv = app_st_get(i)->prov;
        if (pv != NET_PROV_NONE && pv >= NET_PROV_CN_N) ovs_n++;
    }
    lv_obj_t *ovs_t = label(ovs, 0, 0, F_TINY, C_AMBER, "");
    lv_label_set_text_fmt(ovs_t, "海外 %d", ovs_n);
    lv_obj_center(ovs_t);
    ESP_LOGI(TAG, "find page: overseas stations = %d", ovs_n);

    /* ★★ 2.8 适配（10-10）：宽度按 CONT_W 自适应、cw_min 降到 30。
     *   地区条 y=24..56（高 32）—— 箭头 26 px + 胶囊 28 px 高。*/
    hstrip_create(&s_strip_prov, p, 4, 24, CONT_W - 8, 32, 30,
                  NET_PROV_N, ev_pick_prov);

    /* ---------- 提示行（2.8：文案砍短，否则 14px 下 280 宽放不下）---------- */
    lv_obj_t *fhint = label(p, 6, 58, F_TINY, C_MUTED,
                            "点地区看全部台 · 长按台名=收藏");
    lv_obj_set_width(fhint, 268);
    lv_label_set_long_mode(fhint, LV_LABEL_LONG_DOT);

    /* ---------- 电台网格（两列 + 上下滑）---------- */
    s_lbl_find_title = label(p, 6, 74, F_TINY, C_MUTED, "载入中…");
    lv_obj_set_width(s_lbl_find_title, 268);
    lv_obj_set_height(s_lbl_find_title, 16);     /* ★ 10-11：同 s_lbl_list_title2，钉一行 */
    lv_label_set_long_mode(s_lbl_find_title, LV_LABEL_LONG_DOT);
    /* ★ v1.40：网格本体搬走，改由 grid_pages_ensure(1) 建。详见那段注释。*/
}

/* ============================================================
 *  第 7 页：全屏电台列表（首页栏目 / 发现页地区 / 收藏 共用）
 * ============================================================
 *  ★ 兰兰 10-03：「首页按新闻就直接进入全屏的新闻电台页，只要有退回键
 *    就可以了，毕竟屏幕小，需要更大的分类页」「最好都是电台页都是双列的，
 *    显示只要电台名称就足够了」
 *  → 一屏 7 行 × 2 列 = 14 个台名，只有标题 + 返回键，没有别的干扰。
 */

static lv_obj_t *s_btn_back;

static void ev_list_back(lv_event_t *e)
{
    (void)e;
    tap_note("返回");
    /* ★ 海外二级：s_list_back==6 表示「上一层也是第 7 页」（地区块列表），
     *   所以不是简单 goto_page —— 得先把这一层重画回地区块，
     *   否则会看到【同一个页面里内容没变】。*/
    if (s_list_back == 6) {
        list_page_back_to_ovs();
        return;
    }
    goto_page(s_list_back);
}

/* 收藏：把 NVS 里的台名解析成下标 */
static void list_page_refresh(void)
{
    static int pool[GRID_MAX];
    int n = 0;
    const char *title = s_list_title;

    if (s_list_mode == LIST_BY_CAT) {
        if (s_list_cat < 0 || s_list_cat >= NET_CAT_N) s_list_cat = 0;
        title = g_cat_name[s_list_cat];
        n = collect_stations(pool, GRID_MAX, s_list_cat, -1);
    } else if (s_list_mode == LIST_BY_PROV) {
        if (s_list_prov < 0 || s_list_prov >= NET_PROV_N) s_list_prov = 0;
        title = g_prov_name[s_list_prov];
        n = collect_stations(pool, GRID_MAX, -1, s_list_prov);
    } else if (s_list_mode == LIST_BY_ALL_OVS) {
        /* ★ 把地区 >= 31（中国台湾起）的台一次收齐。
        *   判据用 prov 下标而不是台名里找「台/港/澳」字 ——
        *   台名是数据不是结构，改一次数据就要重配关键词（第八次踩过这个坑）。*/
        int total = app_radio_station_count();
        for (int i = 0; i < total && n < GRID_MAX; i++) {
            unsigned char pv = app_st_get(i)->prov;
            if (pv != NET_PROV_NONE && pv >= NET_PROV_CN_N) pool[n++] = i;
        }
    } else if (s_list_mode == LIST_BY_OVS_REGION) {
        /* 这一层画的是地区块，由 ovs_grid_refresh() 处理 */
        n = 0;
    } else if (s_list_mode == LIST_BY_HIST) {
        /* 播放历史：app_hist_top() 已经顺手做了「解析不出来的跳过」
         *   和「同名去重」，这里直接用。
         * ★ 10-11：再把「最近听过的本地音频」接在电台后面。
         *   app_hist 只存【台名】（设计如此，见 app_hist.h ①），
         *   本地音频的进度单独存在 app_resume 的 LRU 表里 ——
         *   两份数据天然分开，这里只是把它们拼进同一个网格显示。
         *   编码见 HIST_LOCAL_BASE 的注释。
         *   ⚠️ 顺序是"电台在前、本地在后"而不是按时间混排：
         *     混排需要两边共用一个时间轴（app_hist 里没有时间戳，
         *      只有 LRU 次序），硬做就得改 app_hist 的存档格式。
         *     而现在这样已经满足兰兰要的"历史里有本地音频、点了能续播"，
         *     代价是零 —— 不值得为排序去动存档。*/
        title = "最近听过";
        n = app_hist_top(pool, GRID_MAX);
        int lc = app_resume_count();
        for (int i = 0; i < lc && n < GRID_MAX; i++) {
            pool[n++] = HIST_LOCAL_BASE + i;
        }
        if (lc > 0) ESP_LOGI(TAG, "list: 历史后面接上本地音频 %d 条", lc);
    } else if (s_list_mode == LIST_BY_RESUME) {
        /* ★ 10-11：首页「续听」格 —— 只列【听了一半、还没听完】的本地音频。
         *   ★ 为什么能一行代码就复用整条链路：
         *     编码沿用历史页里那套 HIST_LOCAL_BASE + i，
         *     所以 grid_show_cell 的渲染分支、on_grid_play 的播放分支、
         *     grid_slot_long 的「不认长按」分支，一个字都不用改就都能用。
         *   ⚠️ 这一页天然只有本地文件：app_resume 只在 !is_url 时记账（见 app_radio.c），
         *     网络电台的进度从来不进这张表 —— 不是遗漏，是设计。*/
        title = "继续收听";
        int rc = app_resume_count();
        for (int i = 0; i < rc && n < GRID_MAX; i++) {
            pool[n++] = HIST_LOCAL_BASE + i;
        }
        ESP_LOGI(TAG, "list: 继续收听 %d 条（最近优先）", n);
    } else {
        title = "我的收藏";
        n = app_fav_list(pool, GRID_MAX);
    }
    s_list_total = n;
    ESP_LOGI(TAG, "list title: %s (%d)", title ? title : "?", n);

    /* ★ v1.40：网格按需建 ⇒ 这里可能还没建。
     *   典型触发：从播放页按返回直接跳第 6 页，或点标签条进「新闻」
     *   而机器还没进过第 6 页。grid_set 里会写 g->idx[] / g->total，
     *   s_grid_list 为 NULL 就是往地址 0x0.. 写。*/
    if (!s_grid_list) {
        ESP_LOGW(TAG, "list: 第 6 页网格还没建 —— 先建再填台");
        grid_pages_ensure(6);
        if (!s_grid_list) return;
    }

    /* ★ 回到台名模式（上一行可能还停在「地区块模式」）——
     *   grid 的 mode 是常驻字段，不清的话这一页会把【地区下标】
     *   当【台下标】显示，越界读 g_stations[]。*/
    s_grid_list->mode = 0;
    s_grid_list->on_region = NULL;

    /* ★ 10-11：收藏页 / 历史页 = 长按删一条 + 右上角「清空全部」。
     *   其它模式（栏目/地区/海外）恢复成「长按 = 收藏/取消收藏」的旧行为。
     *   ⚠️ 两个字段每次都要显式写全：这个网格是 4 层共用的（首页/发现/
     *     第 7 页各一），上一次进收藏页留下的 del_mode=1 不清掉，
     *     下次进「新闻」栏目长按就会去删收藏 —— 静默删错东西。*/
    bool delm = (s_list_mode == LIST_BY_FAV || s_list_mode == LIST_BY_HIST);
    s_grid_list->del_mode = delm ? 1 : 0;
    s_grid_list->on_del   = on_grid_del;

    /* 右上角那一格：灰字提示 ⇄ 清空按钮，二选一 */
    if (s_lbl_list_hint) {
        if (delm) lv_obj_add_flag(s_lbl_list_hint, LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_clear_flag(s_lbl_list_hint, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_btn_list_clear) {
        if (delm) lv_obj_clear_flag(s_btn_list_clear, LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(s_btn_list_clear, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_lbl_list_clear) lv_label_set_text(s_lbl_list_clear, "清空全部");
    s_clear_armed = false;              /* 换页/换模式都泄压，别让上一次的上膛延续 */

    /* ★★★ 10-11 兰兰真机反馈：标题换行，第二行被下面的电台网格挡住。
     *   原话：「标题比如交通台共132然后换行『台 上下滑』换行被下面的电台挡住了，
     *          上下滑这个注释不用，共132台可以适当简化成132台，这样一行就能放下了」。
     *
     *   实测（_ui_width.py，F_MID=18px，标题可用宽 148）：
     *     "%s · 共 %d 台 · 上下滑" = 243 px  ⇒ 折两行，第二行压在 y=40 起的网格上
     *     "%s %d台"               = 129 px  ⇒ 余 19（最坏 4 字栏目名 + 3 位台数）
     *   ★ 台数上界也核实过：栏目最大「新闻综合」619、地区最大「中国台湾」105，
     *     都是 3 位 ⇒ 129 px 就是真实最坏值，不是拍脑袋。
     *   ⚠️ 只把宽度设成 148 是【挡不住】的：LONG_DOT 只裁横向，
     *     纵向仍会自己长高（铁律 119：居中的 label 超宽双向溢出，
     *     定宽的 label 超长则【向下】溢出）。所以建那一头把高度也钉成一行
     *     （见 page_list_build 里 lv_obj_set_height 那几行）。*/
    if (s_lbl_list_title2) {
        /* ★ 10-11：「续听」页列的是【音频文件】，量词不能叫「台」——
         *   否则显示成「继续收听 3台」，听着像三个广播电台。
         *   单独走一条分支，措辞也说人话（空表时别写「没有条」这种机器话）。*/
        if (s_list_mode == LIST_BY_RESUME) {
            if (n == 0) lv_label_set_text(s_lbl_list_title2, "还没有可续听的");
            else        lv_label_set_text_fmt(s_lbl_list_title2, "继续收听 %d 条", n);
        } else if (n == 0) {
            lv_label_set_text_fmt(s_lbl_list_title2, "%s 没有台", title);
        } else {
            lv_label_set_text_fmt(s_lbl_list_title2, "%s %d台", title, n);
        }
    }
    grid_set(s_grid_list, pool, n);
}

/* 首页收藏卡 → 全屏收藏页
 * ★ 10-04 第二十八次：首页那张卡已改成「最近听过」，
 *   收藏的首页入口搬到了标签条第 1 颗（走 ev_pick_cat 的 HCAT_FAV 分支）。
 *   这个函数保留：全屏收藏页内部、以及将来别处要跳收藏页时还能用。*/
static void ev_open_fav(lv_event_t *e)
{
    (void)e;
    tap_note("收藏电台");
    s_list_mode  = LIST_BY_FAV;
    s_list_title = "我的收藏";
    s_list_back  = 0;
    list_page_refresh();
    goto_page(6);
}

/* 首页「最近听过」卡 → 全屏历史页（10-04 第二十八次新增）*/
static void ev_open_hist(lv_event_t *e)
{
    (void)e;
    tap_note("最近听过");
    s_list_mode  = LIST_BY_HIST;
    s_list_title = "最近听过";
    s_list_back  = 0;
    list_page_refresh();
    goto_page(6);
}

/* ★★ 首页那两张小卡的下行文字。单独抽出来，因为有三处要刷：
 *   ① 建首页时  ② 切台后（历史变了）  ③ 插卡/拔卡后（音频数变了）
 *   以前收藏数那行是散在 on_grid_fav 和首页建页里各写一遍的 ——
 *   两处写法迟早会不一致（和「顶栏时钟死 label」同一个毛病）。*/
static void home_cards_refresh(void)
{
    if (s_lbl_histcnt) {
        int top[1];
        int n = app_hist_top(top, 1);
        if (n > 0) {
            /* 显示【台名】而不是「N 个」：开机第一眼想知道的是「在听什么」，
             * 不是「有几条记录」。台名最长 14 字，11px 下约 150 px，
             * 卡片可用宽 152-24 = 128 px ⇒ 按字符截到 9 个字。*/
            char nm[64];
            utf8_copy_n(nm, sizeof(nm), app_st_get(top[0])->name, 9);
            lv_label_set_text_fmt(s_lbl_histcnt, "%s", nm);
        } else {
            lv_label_set_text(s_lbl_histcnt, "还没有记录");
        }
    }
    if (s_lbl_sdinfo2) {
        if (app_sd_is_mounted()) {
            /* ★ 必须传 rescan 参数（app_sd_audio_total(bool)）——
             *   写成 app_sd_audio_total() 会被 gcc 拦（too few arguments）。
             * ★ 这里传 false：首页这张卡每秒可能被刷到（切台时会），
             *   传 true 就是每刷一次递归 stat 几千条目录项。
             *   真正需要重扫的地方是 SD 页切目录（那边已有 s_sd_count_dirty）。*/
            int cnt = app_sd_audio_total(false);
            if (cnt > 0) {
                lv_label_set_text_fmt(s_lbl_sdinfo2, "%d 首 →", cnt);
            } else {
                lv_label_set_text(s_lbl_sdinfo2, "暂无音频 →");
            }
        } else {
            lv_label_set_text(s_lbl_sdinfo2, "未插内存卡");
        }
    }
}

/* ★ 10-11：「清空全部」按钮。★ 必须按两下才生效 ——
 *   收藏/历史是用户攒了很久的东西，一颗键点错就全没了，代价不对等。
 *   （与「恢复出厂」同一条原则：不可逆动作必须有二次确认。）
 *   第一下：上膛 + 按钮文案改成「再点一次」，4 秒内没跟上就自动泄压。
 *   ⚠️ 别用「弹窗」做法：这页 280×212 已经排满，弹出层会盖住网格，
 *     而且 LVGL 消息框另起一个模态对象，池余量本来就只剩几十 KB。*/
static void ev_list_clear(lv_event_t *e)
{
    (void)e;
    uint32_t now = lv_tick_get();
    if (!s_clear_armed || (now - s_clear_arm_tick) > 4000u) {
        s_clear_armed     = true;
        s_clear_arm_tick  = now;
        if (s_lbl_list_clear) lv_label_set_text(s_lbl_list_clear, "再点一次");
        tap_note("再点一下确认清空");
        return;
    }
    s_clear_armed = false;

    if (s_list_mode == LIST_BY_FAV) {
        app_fav_clear();
        if (s_lbl_favcnt) lv_label_set_text_fmt(s_lbl_favcnt, "%d 个常用台", app_fav_count());
    } else if (s_list_mode == LIST_BY_HIST) {
        app_hist_clear();
    } else {
        return;                     /* 不该发生：按钮只在这两个模式现身 */
    }
    ESP_LOGW(TAG, "已全部清除（mode=%d）", s_list_mode);
    tap_note("已全部清除");
    if (s_grid_home) home_grid_refresh();   /* 首页收藏/历史卡要跟着变 */
    if (s_grid_find) grid_fill(s_grid_find);
    list_page_refresh();                    /* 整页重画：列表变空 + 按钮复位 */
}

static void page_list_build(lv_obj_t *p)
{
    /* 标题行 + 返回键
     * ★ 2.8：返回键 4,2 58×26；标题 x=68 宽 150（3.5 是 x=76 宽 340 ⇒ 右缘 416
     *   在 280 宽的页里超出 136 px，长地区名会直接画到屏外）。*/
    s_btn_back = tick(p, 4, 2, 58, 26, C_HILITE, 13, NULL);
    lv_obj_add_event_cb(s_btn_back, ev_list_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bt = label(s_btn_back, 0, 0, F_BODY, C_TEXT, "‹ 返回");
    lv_obj_center(bt);

    /* ★ 10-05 兰兰反馈「地区列表的标题字体太大，长地区名，上下滑的滑会换行
     *   挡住电台名」。
     *   根因同上：F_H2(24px)。但这里【已经】设了 LONG_DOT，理论上不该换行——
     *   实测换行说明两点之一：① 24px 下 LVGL 判定长文本的时机与 12px 不同；
     *   ② 更要紧的是 y=10 处 24px 字高 24，已占掉到 y=34，
     *   而网格从 y=40 开始，只剩 6px；字一换行第二行就压到网格第一行。
     *   修法：F_H2(24px) → F_MID(16px)。16px 下字高 16，10~26，
     *   网格 40 起 ⇒ 中间留 14px 安全带。长地区名仍走 LONG_DOT 出省略号。*/
    s_lbl_list_title2 = label(p, 68, 6, F_MID, C_TEXT, "电台");
    lv_obj_set_width(s_lbl_list_title2, 148);
    /* ★ 10-11：高度钉成一行（18px 字 line_height=21）。这句话不是装饰 ——
     *   只设宽度的话，文本一超长 LVGL 就自己往下长第二行，而网格从 y=40 开始，
     *   第二行正好压在第一个台名上（兰兰看到的「被下面的电台挡住」）。
     *   钉住高度后，超长只能横向出省略号，永远压不到邻居。
     *   ⚠️ 高度必须 ≥ line_height，否则会把字的下缘（如「品/京」的竖）切掉。*/
    lv_obj_set_height(s_lbl_list_title2, 22);
    lv_label_set_long_mode(s_lbl_list_title2, LV_LABEL_LONG_DOT);

    /* ---- 网格：y=40 起，高 236 → ★ 10-05 三列后 8 行 × 29px = 24 台/屏
     *   （原来双列 6 行 = 12 台。行高 236/8 = 29 px，扣掉格间距 4 是 25 px，
     *   放 12px 字 + 上下各 6 px 余量，够；台名字体走 F_ST12。
     *   ⚠️ 若以后把 vis_rows 提到 9，行高掉到 26 px 以下就会显得挤，
     *      届时应先把容器高度 h 从 236 加大，而不是压字号。）
     *
     * ★★★★★★ 10-05 v1.39 修【最右一列被挡字】
     *   兰兰原话：「三列最右一列基本都会挡住几个字」。
     *
     *   根因不是台名太长，是**容器比它所在的页面还宽**：
     *     本页父对象 p 的宽 = CONT_W = LCD_H_RES(480) − RAIL_W(56) = **424**
     *     （左边那条 56px 竖导航栏就占在这儿），
     *     而这里给网格 x=6、w=468 ⇒ 右缘 6+468 = **474**，比页面宽出 50px。
     *     cell_w = (468−6)/3 = 154，三列落在 0~154 / 160~314 / 320~474；
     *     第三列有 50px（424~474）落在页面之外，看不见。
     *     8 字台名 12px 实测最宽 80px，居中在 154 里 ⇒ 占 357~437，
     *     其中 424~437 那 13px 被切掉 ⇒ **每行的后 1~2 个字消失**。
     *     兰兰说「基本都会挡住几个字」，就是这个 —— 位置固定 ⇒ 是宽度算错，
     *     不是随机截断。
     *
     *   修法：横向一律以【父容器宽】为基准，不用屏幕宽。
     *     可用 = CONT_W(424) − x(6) − 右边距(6) = **412**
     *     cell_w = (412 − 6×2) / 3 = **133**
     *     三列占 133×3 + 6×2 = 411 ≤ 412 ✓，列右缘 133 / 272 / 411 全在页内
     *     8 字台名 80px 居中在 133 里 ⇒ 占 302~382，右侧余 29px ✓
     *
     *   ★★★ 教训（与 v1.38「上界用错量纲」同源，必须一起记）：
     *     **横向坐标以父容器宽为基准，纵向以父容器高为基准；
     *       拿屏幕宽（480）去算放在 424 宽父级里的坐标，必然溢出。**
     *     屏幕 480 里有 56px 是导航栏，页面里没有。
     *   ★ 顺带把上面 s_lbl_list_title2 的宽度也一起核过：
     *     x=76 w=340 ⇒ 右缘 416 ≤ 424 ✓（这行原来是对的，没动）。*/
    /* ★ v1.40：网格本体搬走，改由 grid_pages_ensure(6) 建。详见那段注释。*/

    /* 操作提示
     * ★★ 2.8：3.5 那条整宽提示放在 y=266（页高 282 的最后一行），
     *   而 2.8 的页只有 212 高，网格 34..208 之后再放一行就出屏了。
     *   ⇒ 提示搬到标题行右端，只留最关键的一条（长按=收藏）：
     *     「点电台名=播放」不用说也懂，「返回=回上一页」返回键上写着。
     *   ⚠️ 10-10 真机截图抓到：原来写「长按=收藏」+ 宽 56，而
     *     「长按=收藏」在 14px 下实测约 **63 px**（4 个汉字 56 + 等号 7）
     *     ⇒ 56 px 装不下，LVGL 折成两行，第二个「藏」字掉到 y≈36，
     *       压在返回键下面露出来 —— 截图里一眼可见。
     *   修法：文案砍成「长按收藏」（纯 4 个汉字 = 56 px），
     *         宽度给到 70 px（x=206 ⇒ 右缘 276 ≤ 280），等号那点余量也留出来了。
     *   ★ 教训归到铁律 66 那一类：**定宽 + 文案长度必须一起算，
     *     14px 汉字按 14 px/字 算，ASCII 按 7 px 算。**/
    s_lbl_list_hint = label(p, 206, 8, F_TINY, C_MUTED, "长按收藏");
    lv_obj_set_width(s_lbl_list_hint, 70);
    lv_obj_set_style_text_align(s_lbl_list_hint, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(s_lbl_list_hint, LV_LABEL_LONG_DOT);

    /* ★ 10-11 兰兰：「收藏历史删单条和全部清除可以做」。
     *   这一颗 = 「清空全部」，与上面那行灰字【共用同一个槽位】，只在
     *   收藏页 / 历史页现身（由 list_page_refresh 切换显隐）。
     *
     *   ⚠️ 为什么挤在标题行右端，而不是另起一行放页底：
     *     本页 280×212，网格占 y=34..208，页底只剩 4 px。
     *     标题行右端 x=206..276 是唯一的空位：标题最坏
     *     「新闻综合 619台」= 129 px（实测），68+129 = 197 < 206 ✓。
     *   ⚠️ 文案量过尺：「清空全部」14px = 56 px，「再点一次」也 56 px，
     *     都塞得进 70 px。别改成更长的（如「清空全部记录」= 84 px 会折行）。*/
    s_btn_list_clear = tick(p, 206, 4, 70, 22, C_HILITE, 8, NULL);
    lv_obj_add_event_cb(s_btn_list_clear, ev_list_clear, LV_EVENT_CLICKED, NULL);
    s_lbl_list_clear = label(s_btn_list_clear, 0, 0, F_TINY, C_TEXT, "清空全部");
    lv_obj_center(s_lbl_list_clear);
    lv_obj_add_flag(s_btn_list_clear, LV_OBJ_FLAG_HIDDEN);   /* 默认是普通列表 */
}

/* ============================================================
 *  页面三：本地播放（进度环 + 曲名 + 传输控制）
 * ============================================================
 *  10-03 第七次改造。原来这页是「假 FM 调谐盘」：环上写死 105.8 MHz，
 *  两个键只弹一行日志，跟真正在放的东西没有任何关系 ——
 *  所以兰兰点完卡里的 mp3 会觉得「没有播放界面」。
 *  现在改成真界面：环 = 进度、环心 = 曲名与时间、右键 = 换曲与暂停。
 *  以后接上网络电台，同一页换个数据源即可。*/
/* ★★ 10-11 兰兰：「音量拖不到最大，音量不用拖拉条，还是用 +- 按键，
 *   显示百分比就行了，步进 10 就可以了」⇒ 第八行的滑块【整条删掉】，
 *   改成「[-] 60% [+]」三格。
 *
 *   ★ 为什么滑块会"拖不到最大"（记下来，别再犯）：
 *     那格只有 124 px 宽要线性映射 0~100 档 ⇒ 每 1% 仅 1.24 px。
 *     再加 LVGL 滑块的 knob 本身有宽度（约 14 px）——
 *     按住 knob 往右拖到贴边时，"手指位置"与"值"之间还差半个 knob，
 *     越想拖满越容易停在 96~99。而档位是整数 0~100，差一点就是差一档。
 *   ⇒ 按钮式步进 10 天然没这个问题：点 10 下必定到 100，读数永远是整数。
 *   ⚠️ 手势（上下滑 ±10）【保留】—— 它本来就是 10 的步进，与按钮等价。
 *   ⚠️ 这是第三次动「音量控件」了（10-04 命中区、10-10 静音位、10-11 换按钮）：
 *     前两次都在"怎么让滑块更好拖"上打转，而正确的做法是【换掉滑块】。*/
static lv_obj_t *s_lbl_vol;      /* 第八行：音量百分比读数（"60%"）*/

/* 音量步进。改这一处 = 同时改 按钮 与 手势 的步长（两边都走 vol_apply）。*/
#define VOL_STEP 10

/* 播放页手势用：local_step / player_refresh 定义在文件后段 */
static void local_step(int step);
static void player_refresh(void);

/* ★ 10-11：改音量的【唯一出口】—— 第八行的 +/- 键与播放页上下滑手势都走它。
 *   ⇒ 步长、clamp、解静音、落盘、刷新读数，只有一份实现，
 *     不会出现"按钮这条路改了、手势那条忘了"（本项目反复踩过这类不对称）。*/
static void vol_apply(int v)
{
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    /* ★ 调音量 = 「我要听」⇒ 顺手解除静音。
     *   否则语义自相矛盾：用户要"调大"，却被静音位挡着。
     *   10-10 血泪：直写 codec 看着当下有声，但静音标志仍是 true，
     *   下一次 apply_audio（声音键 / xs_cfg_init）又把增益打回 0
     *   ⇒ 表现为"刚调好又没声了"，找不到规律。*/
    if (v > 0 && xs_cfg_muted()) xs_cfg_set_mute(false);
    xs_cfg_set_volume(v);                               /* 落盘，重启后还在 */
    if (s_lbl_vol) lv_label_set_text_fmt(s_lbl_vol, "%d%%", v);
}

/* ★ 10-11 播放页手势：左右滑切换台/曲，上下滑调音量（兰兰：「播放页手势可以做」）。
 *
 *  ★★★ 两个必须知道的前提（都是查 LVGL 源码得来的，不是猜的）：
 *
 *  ① 手势默认【一路冒泡到最顶层】。lv_indev.c:1841 那句
 *       while(gesture_obj && lv_obj_is_gesture_bubble(gesture_obj))
 *           gesture_obj = lv_obj_get_parent(gesture_obj);
 *     会把 gesture_obj 抬到【第一个关掉 GESTURE_BUBBLE 的祖先】。
 *     所以光在播放页容器上挂回调是【收不到】的 —— 事件直接传给了屏幕。
 *     ⇒ 必须对播放页容器 clear GESTURE_BUBBLE（见 page_play_build 末尾）。
 *
 *  ② 进度条那条热区必须【拦下】手势（在它上面 clear GESTURE_BUBBLE），
 *     否则在进度条上左右拖超过 50 px 就会触发「切台」——
 *     而用户明明是在拖进度。这是手势与拖动唯一的冲突点，已在源头堵掉；
 *     拖动本身不受影响（它是 seek_drag_poll 每 100 ms 主动读坐标，
 *     不经过 LVGL 的事件系统，10-04 就是这么设计的）。
 *
 *  ★ 阈值：CONFIG_LV_INDEV_DEF_GESTURE_LIMIT=50（像素）+ MIN_VELOCITY=3。
 *    也就是说「慢慢推 40 px」不算手势，「快速滑 50 px 以上」才算 ——
 *    这正是我们想要的（不会把慢速微调误判成翻页）。*/
static void ev_play_gesture(lv_event_t *e)
{
    (void)e;
    lv_indev_t *ind = lv_indev_active();
    if (ind == NULL) return;
    lv_dir_t d = lv_indev_get_gesture_dir(ind);

    if (d == LV_DIR_LEFT) {
        tap_note("手势·下一个");
        local_step(1);
    } else if (d == LV_DIR_RIGHT) {
        tap_note("手势·上一个");
        local_step(-1);
    } else if (d == LV_DIR_TOP) {
        /* 向上滑 = 音量调大（与绝大多数播放器一致）*/
        int v = app_audio_get_volume() + 10;
        vol_apply(v);
        tap_note("手势·音量 +10");
        player_refresh();
    } else if (d == LV_DIR_BOTTOM) {
        int v = app_audio_get_volume() - 10;
        vol_apply(v);
        tap_note("手势·音量 -10");
        player_refresh();
    }
}

/* ★ 10-11：音量 +/- 键。方向由 user_data 传（+1 / -1）。
 *   与 ev_speed_cycle / ev_mode_cycle 的"循环切档"不同，这两个键是【单调】的：
 *   到 0 / 100 就停住（clamp 在 vol_apply 里），不绕回去 ——
 *   "音量从 100 再点一下变 0"是反直觉的，用户会以为按坏了。*/
static void ev_vol_step(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    vol_apply(app_audio_get_volume() + dir * VOL_STEP);
    tap_note(dir > 0 ? "音量 +10" : "音量 -10");
}

/* mm:ss */
static void fmt_mmss(int sec, char *out, size_t n)
{
    if (sec < 0) sec = 0;
    snprintf(out, n, "%02d:%02d", sec / 60, sec % 60);
}

/* 把播放器的真实状态刷到界面上。
 * 由 1 秒定时器在「第 3 页可见时」调用；点文件 / 换台 / 暂停时也手动调一次。
 *
 * ★ 两种音源在同一页里长得不一样（10-03 第八次）：
 *   本地文件 —— 有总时长 ⇒ 底部细进度条显示进度，时长行 "12:03 / 45:10"
 *   网络直播 —— MP3 直播流没有总时长 ⇒ 进度条退化成底线，
 *               时长行显示 "LIVE 03:21"（已听多久）。
 * ★ 10-03 第十一次：原来那个 200x200 的进度环删了（兰兰「圆圈在，频谱就展不开」），
 *   换成底部一条 6 px 的细条，省下的空间全给频谱。*/
static void player_refresh(void)
{
    /* ★ 10-11：音量读数【无条件】同步一次（放在最前面 ——
     *   下面「未播放」那条分支会提前 return，放后面就漏了）。
     *
     *   为什么需要在这儿刷：音量可能被【别的路径】改掉 ——
     *   设置页的静音开关、播放页上下滑手势、将来的语音（"声音大一点"）、
     *   以及外挂蓝牙模块上的按键。而 +/- 两个键只在"按下那一刻"更新读数
     *   ⇒ 从其它路径改了音量再回这一页，读数就是旧的（用户会以为没生效）。
     *   本页每秒钟被调用一次（s_cur_page==2 时），正好当同步点。
     *   ★ 代价为零：lv_label_set_text 内部先 strcmp，文本没变直接返回、不重绘。*/
    /* ★ 10-11：文案带「音量」二字（兰兰：「音量按键没有标签」）。
     *   最宽「音量 100%」= 68 px，读数格 104 px ⇒ 余 36 px。*/
    if (s_lbl_vol) lv_label_set_text_fmt(s_lbl_vol, "音量 %d%%", app_audio_get_volume());

    const char *name = app_radio_now_playing();
    const char *err  = app_radio_last_error();
    bool playing     = app_radio_is_playing();

    if (!playing || name == NULL || name[0] == '\0') {
        if (s_prog_play)     lv_obj_set_width(s_prog_play, 0);
        if (s_np_name)      lv_label_set_text(s_np_name, "空闲");
        if (s_np_time)      lv_label_set_text(s_np_time, "--:--");
        /* ★ 10-11：s_np_hint / s_lbl_playname 已随第九行改版删除
         *   （见 page_play_build 里那段说明），这里不再有它们的分支。*/
        if (s_np_meta)      lv_label_set_text(s_np_meta,
                                              "网络电台 / 本地音频\n首页或「城市声音集」");
        if (s_np_state)     lv_label_set_text(s_np_state,
                                              (err && err[0]) ? err : "未在播放");
        if (s_lbl_playbtn)  lv_label_set_text(s_lbl_playbtn, "播放");
        if (s_lbl_favbtn) {
            /* ★★ 这里原来写 "\2606 收藏"，屏幕上显示的是「6 收藏」——
             *   C 的八进制转义 \ddd 最多吃 3 位数字，\2606 被解析成
             *   \260(=U+00B0，非法 UTF-8) + 字面字符 '6'，那个 6 就漏出来了。
             *   U+2606 的八进制是 2606（4 位），八进制压根表达不了
             *   ⇒ 只能用 \uXXXX / \xXX / 或（本工程 .c 是 UTF-8）直接写字符。
             *   见 firmware/_oct_escape_check.py（门禁，能自检）。*/
            lv_label_set_text(s_lbl_favbtn, "☆ 收藏");
            lv_obj_set_style_text_color(s_lbl_favbtn, lv_color_hex(0x3A424C), 0);
        }
        spec_refresh();          /* 停播时频谱也要塌回底线 */
        return;
    }

    bool live = app_radio_is_stream();
    int  el   = app_radio_elapsed_s();
    int  tot  = app_radio_total_s();
    bool pz   = app_radio_is_paused();
    int  kHz  = app_radio_stream_rate();
    int  ch   = app_radio_stream_channel();

    char es[12], ts[12];
    fmt_mmss(el, es, sizeof(es));

    if (live) {
        /* 直播没有总时长 ⇒ 进度条就是一条底线（宽度 0），
           声音的起伏由上面那排真频谱表达。
           ★ 直播不能 seek：ev_seek_press 里 app_radio_can_seek() 为 false 会直接
             提示「直播不能拖进度」并返回，不会进入拖动态。*/
        if (s_prog_play && !s_seek_drag) lv_obj_set_width(s_prog_play, 0);
        /* 手柄也藏起来：不能拖的东西不该给一个「抓得住」的暗示 */
        if (s_seek_knob) lv_obj_set_style_bg_opa(s_seek_knob, LV_OPA_TRANSP, 0);
        /*★★ 10-04：直播时间行后面挂上「延迟 Ns」。
         *  兰兰两次报「听着像延迟」，但两次原因完全不同：
         *    修复前是重播老片（文件名是几分钟前的），
         *    现在是源本身 + 十几秒的正常延迟。
         *  光听分不出来，屏上打出数字就不用猜、也不用去对比电视。
         *  只有 HLS 直播能算（分片文件名里有 unix 时间戳）；算不出就照旧。*/
        long lag = app_radio_hls_lag_s();
        if (s_np_time) {
            if (lag > 0) lv_label_set_text_fmt(s_np_time, "LIVE  %s  延迟%lds", es, lag);
            else         lv_label_set_text_fmt(s_np_time, "LIVE  %s", es);
        }
    } else {
        if (tot > 0) fmt_mmss(tot, ts, sizeof(ts));
        else         snprintf(ts, sizeof(ts), "--:--");
        /* ★ 正在拖动时不要抢：让前景宽度与时间行跟着手指走，
         *   否则 player_refresh 每秒刷一次会把拖动预览「拽回去」。*/
        if (s_prog_play && !s_seek_drag) {
            int w = 0;
            if (tot > 0) {
                int64_t d = (int64_t)el * s_seek_bar_w / tot;
                if (d > s_seek_bar_w) d = s_seek_bar_w;
                if (d < 0)   d = 0;
                w = (int)d;
            }
            lv_obj_set_width(s_prog_play, w);
        }
        /* ★★ 第二十一次：手柄【常显】并跟着播放进度走。
         *   这是兰兰说的「拖动条可以设置个拖动手柄，这样或许有效」——
         *   之前把手柄设成 LV_OPA_TRANSP，等于「能拖的东西看不见」，
         *   手感上根本不知道有条能拖。*/
        if (s_seek_knob && !s_seek_drag) {
            int d = 0;
            if (tot > 0) {
                int64_t k = (int64_t)el * s_seek_bar_w / tot;
                if (k < 0) k = 0;
                if (k > s_seek_bar_w) k = s_seek_bar_w;
                d = (int)k;
            }
            seek_knob_show(SEEK_BAR_X + d, app_radio_can_seek());
        }
        if (s_np_time && !s_seek_drag) {
            lv_label_set_text_fmt(s_np_time, "%s / %s", es, ts);
        }
    }

    /* 进度条下面那行提示：让兰兰一眼知道现在能不能拖
     *   （电平表撤掉后，这一行是唯一告诉他「条能拖」的界面信息）*/
    if (s_lbl_level && !s_seek_drag) {
        if (s_seek_drag)              lv_label_set_text(s_lbl_level, "松手就跳过去");
        else if (live)                 lv_label_set_text(s_lbl_level, "直播流 · 进度条只读");
        else if (tot > 0)              lv_label_set_text(s_lbl_level, "拖粗条跳位置 · 上面四键跳分钟");
        else                          lv_label_set_text(s_lbl_level, "读不出总长 · 只能用四个跳转键");
    }

    /* 曲名：按【字符】截断，按字节截会把汉字切半个（见 utf8_copy_n 注释）*/
    char nm[80];
    utf8_copy_n(nm, sizeof(nm), name, 22);
    if (s_np_name) lv_label_set_text(s_np_name, nm);

    /* ★★ 10-11：原来这里有一整块「状态提示」（网络直播 · 64 kbps / 从 12:34 接着听 /
     *   44.1 kHz · 立体声），现在【撤掉】—— 兰兰：「播放页底部有当前电台状态用不到」。
     *   · 采样率/声道：与第五行左半 s_np_meta 的来源信息重复 ⇒ 直接不要；
     *   · 「接着上次听」这一条【保留】，但搬到了 s_np_meta（见下面那段）——
     *     本地音频一打开进度条就从中段开始，不解释一句会让人以为"跳台了"。
     *   ⇒ 只留一个标志位，真正的文案在 s_np_meta 里拼。*/
    int rs = app_radio_resumed_s();
    if (s_np_meta) {
        /* ★ 10-11：续播提示搬家到这里（原来在最底部那行，随第九行改版让位给音量）。
         *   只在【确实续播了】时替换第一行 —— 平时第一行还是来源信息。
         *   实测「接着上次听 · 12:34」≈ 120 px，限宽 132 ⇒ 放得下。
         *   ⚠️ 只放第一行：这一格是【单行高度】的 CLIP，第二行会被进度条盖住。*/
        char rs_line[40] = "";
        if (rs > 0) {
            char rss[12];
            fmt_mmss(rs, rss, sizeof(rss));
            snprintf(rs_line, sizeof(rs_line), "接着上次听 · %s", rss);
        }
        if (live) {
            int cur = app_radio_station_current();
            const char *cat = (cur >= 0)
                              ? g_cat_name[app_st_get(cur)->cat % NET_CAT_N]
                              : "网络流";
            /* ★ 10-05 第二行文案缩短：原来「交通台 · 第 12/1246 台」14 字符 ≈ 168px
     *   > 框宽 140px ⇒ 第二行自己又换行 ⇒ 三行压到状态行（见上方注释）。
     *   改成「交通台 12/1246」9 字符 ≈ 108px，放得下。
     *   台单现在是 1246 台，五位数，改成只显示末位也不好看 ⇒ 保留全量，
     *   靠缩短栏目名那一段腾空间。*/
            lv_label_set_text_fmt(s_np_meta, "%s\n%s %d/%d",
                                  rs_line[0] ? rs_line : "网络电台 · 直播",
                                  cat, cur + 1, app_radio_station_count());
        } else if (kHz > 0) {
            /* 同理：「128.0 kHz · 16 bit · 立体声」17 字符会换行
             * ⇒ 「128.0kHz 立体声」11 字符 ≈ 132px，放得下。*/
            lv_label_set_text_fmt(s_np_meta, "%s\n%d.%01dkHz %s",
                                  rs_line[0] ? rs_line : "本地音频 · 内存卡",
                                  kHz / 1000, (kHz % 1000) / 100,
                                  ch == 1 ? "单声道" : "立体声");
        } else {
            lv_label_set_text_fmt(s_np_meta, "%s\n正在解析格式…",
                                  rs_line[0] ? rs_line : "本地音频 · 内存卡");
        }
    }
    if (s_np_state) {
        if (err && err[0]) lv_label_set_text_fmt(s_np_state, "× %s", err);
        else if (pz)       lv_label_set_text(s_np_state, "‖ 已暂停");
        else if (live)     lv_label_set_text(s_np_state, "● 正在播放（直播）");
        else               lv_label_set_text(s_np_state, "● 正在播放");
    }
    if (s_lbl_playbtn) lv_label_set_text(s_lbl_playbtn, pz ? "继续" : "暂停");

    /* 收藏键：★ = 已收藏，☆ = 未收藏；不是电台就灰掉（本地文件没法收藏）
     * ★★ 符号必须【直接写 UTF-8 字符】，不许写 "\2606"——
     *   八进制 \ddd 上限 3 位，\2606 = \260 + '6'，那个 6 会漏到屏幕上，
     *   兰兰 10-05 实机看到的就是「6 收藏」/「5 已收藏」。*/
    int fcur = app_radio_station_current();
    if (s_lbl_favbtn) {
        if (fcur < 0) {
            lv_label_set_text(s_lbl_favbtn, "☆ 收藏");
            lv_obj_set_style_text_color(s_lbl_favbtn, lv_color_hex(0x3A424C), 0);
        } else if (app_fav_has(fcur)) {
            /* ★ 10-11：文案从「★ 已收藏」缩到「★ 收藏」（实测 61 → 47 px）。
             *   原因：按钮只有 50 px，61 px 会往右溢出压住「上一个」的「上」字
             *   （同一次真机反馈里「表」字被压是同一类问题，见 page_play_build
             *   第一行那段账）。「★ 已收藏」比「★ 收藏」只多一个字，
             *   而实心星 + 绿色本身已经足够表达「已收藏」。*/
            lv_label_set_text(s_lbl_favbtn, "★ 收藏");
            lv_obj_set_style_text_color(s_lbl_favbtn, C_GREEN, 0);
        } else {
            lv_label_set_text(s_lbl_favbtn, "☆ 收藏");
            lv_obj_set_style_text_color(s_lbl_favbtn, C_TEXT, 0);
        }
    }

    /* ★ 返回键上的字跟着来源页变：用户按之前就知道会去哪一页。
     *   （s_lbl_playback 在第 3 页建好之前是 NULL，所以要判空）*/
    if (s_lbl_playback) {
        char want[24];
        snprintf(want, sizeof(want), "‹ %s", play_from_name());
        if (strcmp(lv_label_get_text(s_lbl_playback), want) != 0) {
            lv_label_set_text(s_lbl_playback, want);
        }
    }

    spec_refresh();
}

/* 上一个 / 下一个 —— 同一个键，两种音源（10-03 第八次）：
 *   在放电台 → 在整份内置台单里换台（119 台循环，换台后首页大卡同步）
 *   在放本地 → 在同一目录里换曲，到末尾回卷 */
static void local_step(int step)
{
    if (!app_radio_is_playing()) {
        if (s_np_state) lv_label_set_text(s_np_state, "还没在放");
        tap_note("还没在放");
        return;
    }

    if (app_radio_station_current() >= 0) {
        int nx = app_radio_station_step(step);
        if (nx < 0) {
            if (s_np_state) lv_label_set_text(s_np_state, "× 换台失败");
            ESP_LOGW(TAG, "station_step(%d) 失败", step);
            return;
        }
        s_hr_sel = nx;
        home_update_card(nx);
        if (s_grid_home) grid_fill(s_grid_home);
        if (s_grid_list) grid_fill(s_grid_list);
        player_refresh();
        return;
    }

    char nm[96];
    esp_err_t r = app_radio_play_next(s_local_dir, step, nm, sizeof(nm));
    if (r != ESP_OK) {
        /* 换不了就把原因留在状态行上（大多是「这一目录只有一首」）*/
        if (s_np_state) lv_label_set_text(s_np_state, "× 这一目录没有别的音频");
        ESP_LOGW(TAG, "play_next(%d) -> %s", step, esp_err_to_name(r));
        return;
    }
    player_refresh();
}

static void ev_prev_station(lv_event_t *e)
{
    (void)e;
    tap_note("上一个");
    local_step(-1);
}

static void ev_next_station(lv_event_t *e)
{
    (void)e;
    tap_note("下一个");
    local_step(1);
}

static void ev_play_toggle(lv_event_t *e)
{
    (void)e;

    /* 正在放（或暂停中）→ 真的暂停/继续 */
    if (app_radio_is_playing()) {
        app_radio_toggle_pause();
        tap_note(app_radio_is_paused() ? "暂停" : "继续");
        player_refresh();
        return;
    }

    /* 没在放：首页选过台就先试着重起那一台（比如刚开机、或上一台流断了）*/
    if (s_hr_sel >= 0 && app_radio_play_station(s_hr_sel) == ESP_OK) {
        tap_note(app_st_get(s_hr_sel)->name);
        home_update_card(s_hr_sel);
        player_refresh();
        return;
    }

    /* 连台都没选：这一键当「试听提示音」，至少能验证喇叭是通的 */
    s_playing = !s_playing;
    tap_note("提示音");
    if (s_playbtn_lbl)   lv_label_set_text(s_playbtn_lbl, s_playing ? "暂停" : "播放");
    if (s_lbl_playstate) lv_label_set_text(s_lbl_playstate, s_playing ? "播放中" : "已暂停");
    app_audio_beep_async();
}

/* 收藏/取消收藏【正在播的这一台】。
 * ★ 兰兰 10-03：「如果能在硬件版也能做收藏设置最好了」——
 *   收藏不能只靠长按网格（有些界面触不到），播放页这个键是最直接的入口。*/
static void ev_fav_current(lv_event_t *e)
{
    (void)e;
    int cur = app_radio_station_current();
    if (cur < 0) {
        tap_note("现在不是电台（本地文件不收藏）");
        return;
    }
    on_grid_fav(cur);
    player_refresh();
}

/* 回 SD 卡列表挑下一首（导航栏里没有「本地音频」入口，所以这页要留个门）*/
static void ev_open_sd_list(lv_event_t *e)
{
    (void)e;
    tap_note("本地列表");
    s_play_from = 5;              /* 手动跳过去的，回也回 SD 页 */
    ui_sd_page_refresh();
    goto_page(5);
}

/* ============================================================
 *  ★★★ 播放页的「本列表」键（2026-10-04 第二十一次新增）
 * ============================================================
 * 兰兰原话：「播放页还是需要一个正在播放电台或者本地音乐的
 *   所在栏目或者文件夹的列表按键比较好，提示音在状态页已经有了，无需重复」。
 *
 * 做什么：把「我现在这一首是从哪儿来的」那一栏直接摊开。
 *   · 在播网络台 → 打开【它所属的栏目】的全屏台单（第 7 页）
 *     （若它属于某个省，就打开那个省 —— 从发现页选来的台这样更合理）
 *   · 在播本地歌 → 回到 SD 当前目录（第 6 页），那就是它所在的文件夹
 *   · 空闲        → 提示「还没在播」
 *
 * ★ 为什么这个键比提示音有用：提示音只是自测扬声器的手段，
 *   状态页已经有一个；播放页的三个小键里，收藏和「本地」都是
 *   「跳到某个固定页」，缺一个「跳到【当前】上下文」的入口。
 */
static void ev_open_own_list(lv_event_t *e)
{
    (void)e;
    if (!app_radio_is_playing()) { tap_note("还没在播"); return; }

    if (app_radio_is_stream()) {
        int cur = app_radio_station_current();
        if (cur < 0 || cur >= app_radio_station_count()) { tap_note("台单已变"); return; }
        unsigned char pv = app_st_get(cur)->prov;
        if (pv != NET_PROV_NONE && pv < NET_PROV_N) {
            /* 有地区归属的台：打开该地区（发现页选来的台走这条）*/
            s_list_mode  = LIST_BY_PROV;
            s_list_prov  = pv;
            s_list_title = g_prov_name[pv];
        } else {
            /* 全国台：按栏目打开 */
            s_list_mode  = LIST_BY_CAT;
            s_list_cat   = app_st_get(cur)->cat % NET_CAT_N;
            s_list_title = g_cat_name[s_list_cat];
        }
        s_list_back  = 2;              /* 返回键回播放页 */
        s_play_from  = 6;              /* 播放页返回键也回这一页 */
        tap_note(s_list_title);
        list_page_refresh();
        goto_page(6);
    } else {
        /* 本地音频 ⇒ 它的「所在文件夹」就是 SD 页当前目录 */
        s_sd_count_dirty = true;
        s_play_from = 5;
        tap_note("所在文件夹");
        ui_sd_page_refresh();
        goto_page(5);
    }
}

/* ============================================================
 *  播放页的返回键（兰兰 10-03）
 * ============================================================
 *  「在发现页选中要听的电台自动进入播放页但是缺少一个返回发现页的按键，
 *    要返回刚刚在发现页选择的地区栏；如果是首页选中的电台自动进入播放页，
 *    那也可以返回键返回首页，做到从哪里来能够回到哪里」
 *
 *  ★ 为什么不写死「回首页」：那从发现页进来就回错地方了。
 *    每层网格建的时候把 from_page 写进 st_grid_t，点格子时记进 s_play_from，
 *    本键就 goto_page(s_play_from)。从全屏分类页进来时回到第 6 页，
 *    第 6 页自己的「‹ 返回」再回发现/首页 —— 两层构成完整的一条回退链。
 *  ★ 按键上的字也跟着变（首页/发现/本地/电台列表），不然用户不知道会去哪。*/
static lv_obj_t *s_btn_playback;

static const char *play_from_name(void)
{
    switch (s_play_from) {
    case 0:  return "首页";
    case 1:  return "发现";
    case 5:  return "本地";
    case 6:  return "电台列表";
    default: return "首页";
    }
}

static void ev_play_back(lv_event_t *e)
{
    (void)e;
    int to = s_play_from;
    if (to < 0 || to >= PAGES_N) to = 0;
    /* ★ 从全屏分类页回来时，那个页面要重新按当前筛选刷一次 ——
     *   收藏在播放页被改过时（比如按了 ☆ 收藏），列表要立刻反映出来。*/
    if (to == 6) list_page_refresh();
    if (to == 5) ui_sd_page_refresh();
    tap_note("返回");
    goto_page(to);
}

/* ---- 动态频谱（12 根柱子）----
 * ★ 这不是装饰动画：柱高来自 app_radio_spectrum()，那是 app_radio 里
 *   对解码后 PCM 做 Hann 窗 + Goertzel 算出来的真实频段能量。
 *   （首页那张大卡上原来的 spectrum() 是写死的静态示意，这次一并换成真的。）
 *
 * ★★★★★★ 10-05 v1.39：加【三种显示模式可切换】（兰兰：「播放页出来频谱还有什么
 *   动态音频展示，可以切换吗，不占用过多资源的情况下更加灵动」）
 *
 *   三种模式（数据源完全相同，只是摆法不同）：
 *     0 柱  从底部长起来的柱状图（原来那版）
 *     1 镜  以中线为轴上下对称，像示波器波形 —— 比柱状更「有生命」
 *     2 环  12 根柱摆成一个圆环，随能量向外/向内伸缩
 *
 *   ★★★ 资源铁律：三种模式**共用同一批 12 个 box**，切换只改 pos/size/bg_color，
 *     一个新 lv_obj 都不建。
 *     池只剩 15 KB；为三种模式各建一套 = 36 个 box ≈ 12 KB ⇒ 直接抽干。
 *     现在 0 额外开销。切换动作只是改几个整数，CPU 开销可以忽略。
 *
 *   ★ 为什么不做「更多模式」：每种新摆法都要一份坐标表，
 *     而 480×320 上 12 个元素能摆出的、既好看又看得清形状的排列就这几种。
 *     再加就是凑数了。*/
#define SPEC_SHOW 12                  /* 与 app_radio 的 SPEC_BINS 对齐 */
#define SPEC_BAR_W 18                 /* ★ 2.8：16→18（272 宽放 12 根，12×22-4=260）*/
#define SPEC_BAR_GAP 4
/* ★★★ 10-10 兰兰：「频谱只保留一种柱状就可以了」
 *   ⇒ 模式切换整套拿掉 —— 原来有 柱 / 镜 / 环 三档，靠点频谱区循环切：
 *     · 删掉 272×48 的透明热区（s_btn_spec）与「点频谱换图 · 柱」标题条（s_lbl_spec）；
 *     · 删掉 spec_mode_next() / ev_spec_mode() / s_spec_mode / SPEC_MODE_N / SPEC_TITLE_H；
 *     · spec_refresh() 只留模式 0（柱状从底部长），镜/环两套坐标计算一并去掉。
 *   ★ 为什么「只留一种」是对的（不只是省事）：
 *     2.8 的频谱区只有 32 px 高，环形半径被压到 R≤13、柱宽压到 5 px——
 *     那已经不是"另一种可视化"，只是"同一团东西换了个摆法"。
 *     与其留两个凑数的摆法，不如把标题带还给频谱（32→48 px），
 *     把一种做清楚。 */
static lv_obj_t *s_spec_bar[SPEC_SHOW];
static int       s_spec_x0 = 4;       /* 频谱区左边 */
static int       s_spec_w   = SEEK_BAR_W;    /* 频谱区宽（与进度条同宽，同一个宏）*/

static void spec_build(lv_obj_t *p)
{
    /* 12 根 × (18+4) - 4 = 260，居中放在 272 里 ⇒ 左边留 6 px */
    s_spec_x0 = (s_spec_w - (SPEC_SHOW * (SPEC_BAR_W + SPEC_BAR_GAP) - SPEC_BAR_GAP)) / 2;
    for (int i = 0; i < SPEC_SHOW; i++) {
        /* 初始给 3px 底线：有东西在屏幕上，不是一片空白 */
        s_spec_bar[i] = box(p, s_spec_x0 + i * (SPEC_BAR_W + SPEC_BAR_GAP),
                            SPEC_Y + SPEC_H - 3, SPEC_BAR_W, 3, C_GREEN, 2);
    }

    /* ★★★ 10-10 兰兰：「频谱只保留一种柱状就可以了」
     *   ⇒ 原来这里挂着一块 272×54 的透明热区（点哪儿都算「切模式」）
     *     和一条「点频谱换图 · 柱」的标题条，现在两个都删了。
     *     取舍理由写在上面 SPEC_SHOW 那段注释里。*/
}

/* 按真实频谱重画。淡出到底（没在放就全塌成 3px 底线）。
 * ★ 三种模式共用这一份数据，只有「算 pos/size」那段分叉。*/
static void spec_refresh(void)
{
    int n = app_radio_spectrum_bins();
    for (int i = 0; i < SPEC_SHOW; i++) {
        int v = (i < n) ? app_radio_spectrum(i) : 0;
        if (!app_radio_is_playing() || app_radio_is_paused()) v = 0;

        int bx, by, bw, bh;
        /* 低段偏青、高段偏绿，一眼能看出高低频分布 */
        lv_color_t c = (i < 4) ? C_CYAN : ((i < 8) ? C_GREEN : C_RED);

        /* 柱状，从底部长 —— 10-10 起只有这一种摆法（见 SPEC_SHOW 上方注释）。
         * v=0 时留 3 px 底线，屏上永远有东西，不会看着像"这块坏了"。*/
        bh = SPEC_H * v / 100;
        if (bh < 3) bh = 3;
        if (bh > SPEC_H) bh = SPEC_H;
        bw = SPEC_BAR_W;
        bx = s_spec_x0 + i * (SPEC_BAR_W + SPEC_BAR_GAP);
        by = SPEC_Y + SPEC_H - bh;

        lv_obj_set_size(s_spec_bar[i], bw, bh);
        lv_obj_set_pos(s_spec_bar[i], bx, by);
        lv_obj_set_style_bg_color(s_spec_bar[i], c, 0);
        lv_obj_set_style_radius(s_spec_bar[i], 2, 0);
    }
}

/* ============================================================
 *  电平表（12 格）
 *  ============================================================
 *  频谱告诉你「声音的音色如何」，电平告诉你「到底有没有在响」。
 *  后者更原始但更可靠 —— 兰兰 10-03 血泪里两次「日志全绿但喇叭没声」，
 *  如果当时屏幕上有一条电平条，1 秒就能定位。
 *
 * ★ 10-03 第十五次撤掉了屏幕上的 12 格电平表（兰兰：「电平幅度可以不要，
 *   换成加粗点进度条，这样更容易操作」）。理由与数据都写在 page_play_build
 *   的版面账里。电平信息没丢：状态页仍打 `level=NN`，autoplay 探针也还在。
 *⇒ 下面是「顶部频谱 + 粗进度条 + 一行提示」的实现，lmeter 相关函数已删。*/

static void page_play_build(lv_obj_t *p)
{
    /* ★★★ 2.8 版面总账（内容区 280 × 212）—— 见文件上方 SPEC_ / SEEK_ 宏那段。
     *   3.5 那一版是「左半显示（x=4..248）+ 右半控制（x=256..420）」：
     *   搬到 280 宽的页里，**右半边整栏一个像素都不在屏内** ——
     *   返回键、收藏、上一个、播放、下一个、倍速、频谱键、三个小键、
     *   音量滑块全都在那儿。这就是兰兰说的「看不见的按键」最集中的一页。
     *   ⇒ 改成上下九行、每行整宽（明细见 SPEC_/SEEK_ 宏旁的注释）。
     *
     *   ★ 曲名字体：3.5 时代最终定案是【必须用 F_SD】（理由见下面那段历史），
     *     但 2.8 的 F_SD 只是 14px 主字库的别名（2.8 暂时没有 cjk 大字符集），
     *     所以这里改用 F_ST18（16px 台名专用）—— 台名全覆盖，代价是
     *     【SD 卡里的歌名可能出方块】（已知遗留，记在待办里）。
     *
     * ★ 第十五次（兰兰：「电平幅度可以不要，换成加粗点进度条，这样更容易操作」）：
     *   12 格电平表撤掉了 —— 12 个 box ≈ 5 KB LVGL 池，换来的却是一根手指
     *   按不准的 6 px 细条。现在用 20 px 粗条 + 透明热区。 */
    /* ---- 第一行：传输键 ★★ 10-11 按【实测宽度】重排 ----
     *  兰兰真机反馈：「播放页，电台列表标签的表被右边收藏按键的五角星挡住」。
     *  复现（_ui_width.py 量出来的真值，不是估算）：
     *     ‹ 电台列表 = 68 px  ← 旧按钮只有 50 px，超 18
     *     ★ 已收藏   = 61 px  ← 旧按钮只有 50 px，超 11
     *     两个 label 都是 lv_obj_center 居中 ⇒ 各自往两边溢出，
     *     在 x≈55..73 叠在一起 ⇒ ★ 正好压住「表」字。逐字对上兰兰的描述。
     *  旧账（5×50，缝 4，x=7 ⇒ 7..273）没有留任何余量，文案一变长就崩。
     *
     *  新账：宽度全部按实测值给余量，缝仍 4，起点 x=5 ⇒ 5..275（右留 5）
     *     ① ‹返回   72  («‹ 电台列表» 68，余 4)
     *     ② ☆收藏   50  («★ 收藏» 47，余 3 —— 文案同步把「已」去掉，见 refresh 处)
     *     ③ 上一个  46  (42，余 4)
     *     ④ 暂停    40  (28，余 12)  ← 这一行唯一的「主角」，绿底
     *     ⑤ 下一个  46  (42，余 4)
     *     预算 72+50+46+40+46 = 254，+4×4 缝 = 270，5+270 = 275 ✓
     *
     *  ★ 每个 label 都加了【限宽 + LONG_DOT】：以后再有文案变长，
     *    最坏是被省略号截断，绝不可能再溢出到隔壁按钮上压字。
     *    （这就是「把一次性修好」升级成「结构性不再犯」的那一步。）*/
    lv_obj_t *b_back = tick(p, 5, 2, 72, 22, C_HILITE, 11, NULL);
    lv_obj_add_event_cb(b_back, ev_play_back, LV_EVENT_CLICKED, NULL);
    s_lbl_playback = label(b_back, 0, 0, F_TINY, C_TEXT, "‹ 首页");
    lv_obj_set_width(s_lbl_playback, 68);
    lv_label_set_long_mode(s_lbl_playback, LV_LABEL_LONG_DOT);
    lv_obj_center(s_lbl_playback);

    lv_obj_t *b_fav = tick(p, 81, 2, 50, 22, C_HILITE, 11, NULL);
    lv_obj_add_event_cb(b_fav, ev_fav_current, LV_EVENT_CLICKED, NULL);
    s_lbl_favbtn = label(b_fav, 0, 0, F_TINY, C_RED, "☆ 收藏");
    lv_obj_set_width(s_lbl_favbtn, 48);
    lv_label_set_long_mode(s_lbl_favbtn, LV_LABEL_LONG_DOT);
    lv_obj_center(s_lbl_favbtn);

    lv_obj_t *b_prev = tick(p, 135, 2, 46, 22, C_HILITE, 11, NULL);
    lv_obj_add_event_cb(b_prev, ev_prev_station, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pt = label(b_prev, 0, 0, F_TINY, C_CYAN, "上一个");
    lv_obj_set_width(pt, 44);
    lv_label_set_long_mode(pt, LV_LABEL_LONG_DOT);
    lv_obj_center(pt);

    /* 播放/暂停：绿底深字，是这一行唯一的「主角」*/
    lv_obj_t *b_play = tick(p, 185, 2, 40, 22, C_GREEN, 11, NULL);
    lv_obj_add_event_cb(b_play, ev_play_toggle, LV_EVENT_CLICKED, NULL);
    s_lbl_playbtn = label(b_play, 0, 0, F_TINY, C_ONDARK, "暂停");
    lv_obj_center(s_lbl_playbtn);

    lv_obj_t *b_next = tick(p, 229, 2, 46, 22, C_HILITE, 11, NULL);
    lv_obj_add_event_cb(b_next, ev_next_station, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pt3 = label(b_next, 0, 0, F_TINY, C_CYAN, "下一个");
    lv_obj_set_width(pt3, 44);
    lv_label_set_long_mode(pt3, LV_LABEL_LONG_DOT);
    lv_obj_center(pt3);

    /* ---- 第二行：曲名（16px）★ 点它 = 看「本列表」----
     *   ★ 3.5 上「本列表」是个独立小键（50×34，在右栏）。2.8 右栏没了、
     *     第一行也满了 ⇒ 改成【点曲名进去】：台名本身就代表「这是哪一栏的台」，
     *     点它看它所在的那个列表，是最自然的映射。*/
    /* ★★★ 2.8 曲名字体：10-11 改成 F_USER（14px 大字符集）—— 与 3.5 最终定案一致。
     *   ⚠️ 这段历史值得留着，别再改回去：
     *   3.5 时代就定过「曲名必须用 F_SD（大字符集）」，理由是曲名可能是
     *   **卡里用户自己拷进来的文件名**，用哪个字完全不可预知。
     *   2.8 当时【没有】cjk 大字符集，F_SD 只是 14px 主字库的别名，
     *   于是这里权宜用了 F_ST18（16px 台名专用）——台名全覆盖，
     *   代价写进了待办：「SD 卡里的歌名可能出方块」。
     *   ⇒ 10-11 兰兰真机实测坐实了：**本地文件夹页正常（那页用 F_SD/cjk12），
     *     一进 MP3 播放页就方块**。现在 xs_font_cjk12 已按 2.8 源码重烘
     *     （14px / 4661 码点 / _font_coverage.py 对 es28 全绿），故换回大字符集。
     *   ★ 顺带纠正一个更大的坑：st16 **只有 871 码点**，连「滑」「显示」
     *     「还没有」这些界面词都没有 ⇒ 凡是拿它画界面词/用户文本的地方都会方块。
     *     它只配画【台名】（台名里有 GB2312 没有的生僻字，那是它的存在理由）。
     *   ★ 判据：任何"可能显示用户文本"的 label ⇒ F_USER；纯台名 ⇒ F_ST16。*/
    s_np_name = label(p, 4, 26, F_USER, C_TEXT, "空闲");
    lv_obj_set_width(s_np_name, 272);
    lv_obj_set_height(s_np_name, 18);
    lv_label_set_long_mode(s_np_name, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(s_np_name, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_np_name, ev_open_own_list, LV_EVENT_CLICKED, NULL);

    /* ★ 3.5 的右卡里还有一个 s_lbl_playname 也写曲名（同一个台名显示两处），
     *   2.8 保留它，但放到最后一行左半 —— 见 r9。*/

    /* ★ 醒目靠这一行：台名是 F_SD(14px)，下面这行 18px 琥珀色写「网络电台 · LIVE」，
     *   尺寸与颜色形成主次，比单纯放大字号更耐看。*/
    /* ★★★ 10-05 兰兰真机反馈：「点电台进入播放页 LIVE 标签和播放时间挡住
     *   复标签：网络直播的网络 2 个字」。
     *   解开来看：不是「LIVE」和「网络直播」互相压，而是【时间那一行
     *   没有宽度约束】—— s_np_time 用 F_H2(24px) 且从不set_width，
     *   而 player_refresh 里直播时会写 "LIVE  00:32  延迟5s"（最长约 17 字符），
     *   24px 下 17×12 ≈ 204px，从 x=4 伸到 208 ⇒ 越过了 x=96 的 s_np_hint，
     *   把「网络直播 · 64 kbps」的前两个字盖掉了。
     *   v1.35 时这里是 18px（≈153px），刚好还在 96 右边不越界，
     *   换成 24px 就暴露了。⇒ 又是一次「砍字号档」引出的连带问题。
     *   修法：给它固定宽 88 + LONG_DOT，并把 hint 右移到 96 之后不变，
     *   超长文案（如延迟秒数变两位数）被省略号截断，绝不越界。*/
    /* ★★ 10-05 兰兰复验：「延迟时间标签挡住了下方直播流三个字」——
     *   上一版把 s_np_time 限宽到 88px 就以为万事大吉，
     *   可 16px 下「LIVE  00:32  延迟5s」实测【151px】（hmtx 折算，非估算），
     *   88px 装不下 ⇒ LONG_DOT 之前先 WRAP 换行，第二行 42~59 正好压住
     *   y=44 的 s_lbl_level（「直播流 · 进度条只读」的前三个字）。
     *
     *   修法（宽度全部改实测值）：左半区 x=4~256 共 252px，单行排得下：
     *     「LIVE  00:32  延迟5s」  16px = 151px  ⇒ 限宽 152
     *     「网络直播 · 64 kbps」   12px = 114px  ⇒ 限宽 148（不变，够）
     *     「直播流 · 进度条只读」 12px = 120px  ⇒ 限宽 244（不变，够）
     *   ★ 纵向账（行高是从字库 .c 的 .line_height 读回来的真实值，不是猜的）：
     *     2 ~ 15   s_np_name  12px 行高 13
     *     24 ~ 42  s_np_time  16px 行高 18 ← y 由 25 改 24，见下
     *     44 ~ 57  s_lbl_level 12px 行高 13 ⇒ 顶边 44
     *     58 ~ 频谱 SPEC_Y=58
     *   ⇒ 间隙 2px。
     *   ⚠️ 顺带纠正一个我自己抄错的数：16px 的 .line_height 是【18】不是 17。
     *   按 17 算会以为「25+17=42，间隙 2px 够宽」，
     *   按真实 18 算则是 25+18=43，只剩 1px —— 卡在临界上。
     *   ⇒ **行高必须查文件，不要凭印象写进注释**（本工具 --lineheight 就是干这个的）。*/
    /* ★★ 2.8：第三行 = 时间行（左） + 状态（右）。
     *   3.5 这里是「16px 时间 + 12px hint」挤在左半 244 宽里，且 s_np_time
     *   限宽 152 是为了给 hint 让位；2.8 整行 272 宽、两个 label 各占一半，
     *   谁也不会压谁（原来那两个「标签挡住下面几个字」的 bug 由此根除）。*/
    s_np_time = label(p, 4, 45, F_TINY, C_AMBER, "--:--");
    lv_obj_set_width(s_np_time, 132);
    lv_label_set_long_mode(s_np_time, LV_LABEL_LONG_DOT);

    /* 状态 / 错误原因（原第 3.5 版右卡里的那一行）*/
    s_np_state = label(p, 140, 45, F_TINY, C_AMBER, "空闲");
    lv_obj_set_width(s_np_state, 136);
    lv_label_set_long_mode(s_np_state, LV_LABEL_LONG_DOT);

    /* ★ 10-05：hint 从 (96,26) 挪到 (128,44)，与 s_lbl_level 同一行并排。
     *   起因：s_np_time 放宽到 152 后占 4~156，与原来的 hint(96~244) 重叠 60px。
     *   而 y=44 那行左半区还有 124px 空位（lbl_level 只要 120px）⇒ 换到那行最省。
     *   实测宽度：12px「直播流 · 进度条只读」= 120px，12px「网络直播 · 64 kbps」= 114px。
     *   128+114 = 242 < 244（与 s_lbl_level 同右边界），两行都不换行。*/
    /* ★★ 2.8：第五行 = 来源（左） + 进度提示（右）。这一行在频谱【下面】、
     *   进度条【上面】（y=113..127），两个 label 各 132/136 宽。
     *   ⚠️ 3.5 时代 s_np_meta 是【两行】文案（"网络电台 · 直播\n%s %d/%d"），
     *     而这里只给得出一行 ⇒ 用 CLIP（不是 DOT）：两行文案在单行高度里
     *     只显示第一行，绝不会把第二行折下去压到进度条上。
     *     （3.5 上就吃过这个亏：WRAP 折行压住了状态行。）*/
    s_np_meta = label(p, 4, 113, F_TINY, C_MUTED, "本地音频 · 内存卡");
    lv_obj_set_width(s_np_meta, 132);
    lv_obj_set_style_text_line_space(s_np_meta, 0, 0);
    lv_label_set_long_mode(s_np_meta, LV_LABEL_LONG_MODE_CLIP);

    s_lbl_level = label(p, 140, 113, F_TINY, C_MUTED, "细条可左右拖动");
    lv_obj_set_width(s_lbl_level, 136);
    lv_label_set_long_mode(s_lbl_level, LV_LABEL_LONG_DOT);

    /* 播放页的「曲名」在 3.5 上出现两次（左边大字 + 右边卡里），
     * 2.8 版面紧，右边那个放到最后一行左半 —— 见本函数尾部。*/

    /* ---- 频谱（12 根柱，数据来自 app_radio 的 Goertzel 分析）---- */
    spec_build(p);

    /* ---- 进度条：本地可拖动，直播只显示 ----
     *   ★ 兰兰 10-03 第十四次：「细条看到了，但不起作用，无法拖拉」，
     *     第十五次：「本地播放进度条还是没有」。
     *   ⇒ 真因：6 px 高的 box 当触摸目标，按不准。
     *     现在加粗到 20 px，另罩一个 44 px 高的透明热区，事件全挂热区。*/
    s_seek_bar_w = SEEK_BAR_W;
    s_arc_play = box(p, SEEK_BAR_X, SEEK_BAR_Y, SEEK_BAR_W, SEEK_BAR_H, C_HILITE, 4);
    s_prog_play = box(p, SEEK_BAR_X, SEEK_BAR_Y, 0, SEEK_BAR_H, C_GREEN, 4); /* 宽度由 player_refresh 改 */
    /* ---- 拖动手柄（第二十一次：18×34 的圆角把手，常显）----
     * ★ 之前是 4 px 宽、且只在拖动时显形，看着就是「绿线上的一道白线」，
     *   兰兰不知道能拖。现在做成一个明显的小把手，一直停在当前进度上。*/
    s_seek_knob = box(p, SEEK_BAR_X, SEEK_BAR_Y - 4, SEEK_KNOB_W, SEEK_KNOB_H,
                      lv_color_hex(0xF2F5F7), 6);
    /* 把手只显示，不收事件：命中全交给下面的透明热区，
     * 否则它会把热区挡掉一块（透明对象照样吃触摸，10-04 踩过）。*/
    lv_obj_clear_flag(s_seek_knob, LV_OBJ_FLAG_CLICKABLE);
    /* 透明热区：视觉上什么都没有，但 34 px 好按 */
    s_seek_hit = box(p, SEEK_BAR_X, SEEK_HIT_Y, SEEK_BAR_W, SEEK_HIT_H,
                     lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_seek_hit, LV_OPA_TRANSP, 0);
    lv_obj_set_flag(s_seek_hit, LV_OBJ_FLAG_CLICKABLE, true);
    /* ★★ 热区必须【不可滚】：LV_DIR_NONE 让 LVGL 不把它当滚动对象，
     *   这一条对 PRESSED/PRESSING 能不能收到是决定性的
     *   （兰兰连报三次「本地拖动还是不行」= 只有 PRESSING 收不到）。
     *   ⚠️ 但它同时意味着 LVGL 【不会】把拖动派发成 PRESSING ——
     *   所以拖动完全不依赖事件，改由 spec_timer_cb → seek_drag_poll()
     *   每 100 ms 主动读一次 indev 当前点。这是唯一稳的做法：
     *   要自己发事件就得靠 scroll 抢手势，一抢就把 PRESSING 也弄丢。*/
    lv_obj_set_scroll_dir(s_seek_hit, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(s_seek_hit, LV_SCROLLBAR_MODE_OFF);
    /* ★ 10-11：手势必须【在这里止步】。见 ev_play_gesture 上方注释 ② ——
     *   不关这个 flag，在进度条上左右拖过 50 px 会被判成「切台手势」，
     *   而用户明明是在拖进度。关掉后手势事件停在这一层，
     *   这里没有挂 GESTURE 回调 ⇒ 自然丢弃。拖动本身不受影响。*/
    lv_obj_clear_flag(s_seek_hit, LV_OBJ_FLAG_GESTURE_BUBBLE);
    /* 只挂「按下」与「松手」两个边界事件，中间的移动完全自管。
     * ⚠️ 上一版还挂了 LV_EVENT_PRESSING，它必然收不到，留着是误导。*/
    lv_obj_add_event_cb(s_seek_hit, ev_seek_press,    LV_EVENT_PRESSED,     NULL);
    lv_obj_add_event_cb(s_seek_hit, ev_seek_release,   LV_EVENT_RELEASED,    NULL);
    lv_obj_add_event_cb(s_seek_hit, ev_seek_release,   LV_EVENT_PRESS_LOST,  NULL);
    ESP_LOGI(TAG, "seek bar: x=%d w=%d y=%d h=%d, hit area y=%d h=%d, total=%d can_seek=%d",
             SEEK_BAR_X, SEEK_BAR_W, SEEK_BAR_Y, SEEK_BAR_H, SEEK_HIT_Y, SEEK_HIT_H,
             app_radio_total_s(), (int)app_radio_can_seek());

    /* ---- 快退 2 / 退 10 / 进 10 / 进 2（见版面宏的注释）----
     * 兰兰原话：「快进10有了，但是10秒太短，需要分钟级别的，2分钟，10分钟」。
     * ★ 文案用汉字「退2分/进2分」：« » 这两个符号 SimHei/Deng 都没有 ⇒ 方块。
     *
     * ★★ 2026-10-04 第二十次：兰兰报「快进10分钟有效，但经常要触碰几次才有效，
     *   2 分钟无反应可能不灵敏」。
     *   真因不是「不灵敏」，是【点击被 LVGL 判成滚动吃掉了】：
     *     tick() 给按钮挂了 ev_press(LV_EVENT_PRESSED) 去改透明度 opa，
     *     而「改样式 → 重排 → 对象坐标变了」，LVGL 在同一帧里
     *     发现指针下的对象变了，就把它转成 scroll 并放弃 CLICKED。
     *     手指按得越重越容易触发 ⇒ 表现为「要碰几次」。
     *   ⇒ 修法：跳转键【不挂 ev_press/ev_release】，改挂 LV_EVENT_PRESSED 自己做
     *     一次性动作（按下即跳，不等松手）。按下即响应 = 不可能被判成滚动。
     *     视觉反馈用 tap_note 的文字提示，不靠改按钮样式。*/
    {
        static const int      secs[4] = { -120, -600, 600, 120 };
        static const char *const txts[4] = { "退2分", "退10分", "进10分", "进2分" };
        for (int i = 0; i < 4; i++) {
            int x = SEEK_BAR_X + i * (SEEK_STEP_W + SEEK_STEP_GAP);
            /* ★ 高度从 20 加到 30（SEEK_STEP_H），手指目标大一圈 */
            lv_obj_t *b = box(p, x, SEEK_ROW1_Y, SEEK_STEP_W, SEEK_STEP_H,
                              C_HILITE, 8);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_scroll_dir(b, LV_DIR_NONE);
            /* 按下即跳：不等 RELEASED ⇒ 不会有「按住不动算滚动」的窗口 */
            lv_obj_add_event_cb(b, ev_seek_step, LV_EVENT_PRESSED,
                                (void *)(intptr_t)secs[i]);
            /* ★ 第二十二次：退用青、进用绿（兰兰：「关键按键的字体最好是彩色的」）。
             *   方向靠颜色分，比靠文字顺序记更省心：左手边一排青=往回，
             *   右手边一排绿=往前，中间那条分界就是「现在」。*/
            lv_obj_t *t = label(b, 0, 0, F_BODY, (i < 2) ? C_CYAN : C_GREEN, txts[i]);
            lv_obj_center(t);
        }
    }

    /* ---- 电平表（12 格条）：频谱看「音色」，这条看「有没有声音」 ----
     * ★ 为什么自己拿小方块拼而不用 lv_linemeter：那个控件自带一整套
     *   style（渐变/边框/指示器每个都是 lv_obj），一个就吃掉几 KB。
     *   池只剩 21 KB，得省。12 个 box ≈ 5 KB。*/
    /* 电平表已撤（10-03 第十五次）。这一行改成拖动提示，
     * 每次 player_refresh 会按能不能拖更新文案。
     * ★ 第十八次挪到 y=228 之下没空间了（进度条现在 210~238），
     *   改放在【频谱区上方那行】—— 也就是 y=44，紧贴时间行下面。
     *   反正 58 才是频谱顶，44~56 这一行本来就空着。*/
    /* ★ 10-05：宽度 244 → 120（实测 12px「直播流 · 进度条只读」= 120px）。
     *   右半 124px 让给 s_np_hint，两段边界明确，避免再次叠字。
     *   另两处文案实测：12px「拖粗条跳位置 · 上面四键跳分钟」= 156px、
     *   12px「读不出总长 · 只能用四个跳转键」= 168px —— 都超过 120px，
     *   会出省略号。这是【故意接受的取舍】：兰兰要求优先保住左半区不叠字，
     *   且这行是操作提示，截断成「拖粗条跳位置 · 上面四键跳…」仍能看懂。*/
    /* ---- 第八行：倍速 + 本地 + 音量 ----
     * ★★ 2.8 改动（原来这三样都在右栏，整栏在屏外）：
     *   倍速键从 78×22 缩到 44×16、本地键从「三个小键」里单独拿出来，
     *   音量文字 + 滑块挤在同一行的右边。
     *   横向账：4 + 44 + 4 + 44 + 4 + 52 + 4 + 116 = 272 ✓（4..276）*/
    s_btn_speed = tick(p, 4, 174, 44, 16, lv_color_hex(0x1B3A31), 8, NULL);
    lv_obj_add_event_cb(s_btn_speed, ev_speed_cycle, LV_EVENT_PRESSED, NULL);
    /* ★ 键上的字只放档位本身（「1.0X」）—— 44 px 放不下「倍速 1.0X」 */
    s_lbl_speed = label(s_btn_speed, 0, 0, F_TINY, C_GREEN, "1.0X");
    lv_obj_center(s_lbl_speed);

    lv_obj_t *lb = tick(p, 52, 174, 44, 16, C_HILITE, 8, NULL);
    lv_obj_add_event_cb(lb, ev_open_sd_list, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb1 = label(lb, 0, 0, F_TINY, C_CYAN, "本地");
    lv_obj_center(lb1);

    /* ★★ 10-11 第三次改第八行（兰兰：「连播，随机，单曲可以有位置做按键」）：
     *   把上一版的【一颗循环键】拆成【三颗并列键】。
     *   横向账（CONT_W=280 ⇒ 可用 4..276，缝 4）：
     *     倍速 4..48 ｜ 本地 52..96 ｜ 连播 100..156 ｜ 单曲 160..216 ｜ 随机 220..276
     *   每颗 56 px，题字 28 px（"连播/单曲/随机"实测都是 28）⇒ 左右各 14 px 余量。
     *   ★ 为什么拆：循环键要「点几下才知道现在在哪一档」，
     *     且从当前态走到目标态平均要点 1.5 下；三颗一眼看全、一下到位。*/
    for (int i = 0; i < 3; i++) {
        int x = 100 + i * 60;
        s_btn_mode3[i] = press_btn(p, x, 174, 56, 16, C_PANEL, 8,
                                   ev_mode_pick, (void *)(intptr_t)i);
        s_lbl_mode3[i] = label(s_btn_mode3[i], 0, 0, F_TINY, C_MUTED,
                               k_mode_short[i]);
        lv_obj_center(s_lbl_mode3[i]);
    }
    mode_sync();     /* ★ 必须按 NVS 里的真实模式点亮，不能默认第一颗亮 */

    /* ---- 第九行：音量「[−] 音量 N% [+]」（整行）----
     * ★★ 10-11 兰兰两次意见合并到这里：
     *   ① 「音量按键没有标签」⇒ 读数格里写成「音量 66%」，不再是光秃秃的 "66%"；
     *   ② 「底部有当前电台状态用不到，底部改为音量标签和+-」⇒ 这一行【整行】给音量，
     *      原来占这行的「小曲名」与「状态提示」都撤掉
     *      （曲名在第二行有更大的那个、且同样可点进「本列表」；状态提示与第五行的
     *        来源行信息重叠 —— 按本工程「一屏别放两处同样入口」的规矩收成一个）。
     *   横向账（4..276，缝 4）：
     *     [-] 4..84 ｜ 读数格 88..192 ｜ [+] 196..276
     *   两侧各 80 px 宽 —— 音量是全页最常改的东西，热区给足；
     *   中间 104 px 装最宽文案「音量 100%」= 68 px，余 36 px。
     *   ⚠️ 减号必须是 ASCII "-"（U+2212 数学减号字库里没有，会画成方块）。*/
    lv_obj_t *vdown = press_btn(p, 4, 192, 80, 16, C_HILITE, 8,
                                ev_vol_step, (void *)(intptr_t)(-1));
    {
        lv_obj_t *t = label(vdown, 0, 0, F_MID, C_TEXT, "-");
        lv_obj_center(t);
    }

    /* 中间是【读数】不是按键 ⇒ 用 C_PANEL 底色，一眼与两侧按钮区分开。
     * ★ 做法：拿 box 当容器再让 label 居中 —— 比"给 label 手工算坐标"稳，
     *   字号一换由布局保证居中，不用回来改坐标。*/
    {
        lv_obj_t *vc = box(p, 88, 192, 104, 16, C_PANEL, 8);
        s_lbl_vol = label(vc, 0, 0, F_BODY, C_TEXT, "音量 60%");
        lv_obj_center(s_lbl_vol);
        /* 初值必须问真正在用的音量，不能写死 —— 音量存 NVS，
         * 上次关机时是 30% 的话，重建这一页必须显示 30%。*/
        lv_label_set_text_fmt(s_lbl_vol, "音量 %d%%", app_audio_get_volume());
    }

    lv_obj_t *vup = press_btn(p, 196, 192, 80, 16, C_HILITE, 8,
                              ev_vol_step, (void *)(intptr_t)(1));
    {
        lv_obj_t *t = label(vup, 0, 0, F_MID, C_TEXT, "+");
        lv_obj_center(t);
    }

    /* ★★ 10-11：第九行原来的「小曲名 + 状态提示」【两个都撤掉】了。
     *   兰兰原话：「播放页底部有当前电台状态用不到，底部改为音量标签和+-」
     *   ⇒ 这一行整行让给上面的音量控件（[-] 音量 N% [+]）。
     *
     *   ★ 撤掉的这两样，信息一处都没丢：
     *     · 小曲名（s_lbl_playname）—— 第二行 s_np_name 是同一份内容、
     *       字号更大、**同样挂了 ev_open_own_list**（点它进「本列表」）
     *       ⇒ 这就是本工程一贯的「320×240 上不该有两处同样的入口」。
     *     · 状态提示（s_np_hint）—— 它显示的 "网络直播 · 64 kbps"/"本地 · 44.1 kHz"
     *       与第五行左半 s_np_meta 的来源信息重叠；唯一独有的是
     *       「从 12:34 接着听」那句续播提示。若以后发现这句有用，
     *       最省地方是并进 s_np_meta（第五行左半）而不是抢回这一行。
     *   ⚠️ 它们的 label 句柄、以及 player_refresh 里的对应分支都一并删了 ——
     *      留着句柄不清 NULL 就会被定时器写到已释放对象上（见 night_page_destroy
     *      那条注释里记的 use-after-free 教训）。*/

    /* ★★ 2.8：3.5 那张 164×94 的右卡（曲名 / 来源 / 状态）已经【整张拆掉】，
     *   它的三行信息分别搬到了：
     *     曲名 → 第二行（16px 大字，可点进「本列表」）
     *            ⚠️ 10-11 起【只有这一处】了：第九行左半那个小曲名已随音量行改版删除
     *     来源 → 第五行左半（s_np_meta）
     *     状态 → 第三行右半（s_np_state）
     *   ⇒ 这里不再建任何对象。（见文件上方「九行总账」。）*/

    /* ---- 倍速 / 频谱 / 本列表 三个旧键的归宿 ----
     *   倍速 → 第八行（s_btn_speed）
     *   频谱模式 → 【10-10 已整条撤销】：兰兰「频谱只保留一种柱状就可以了」
     *             ⇒ 不再有"切模式"这个动作，频谱区也不再是热区（见 SPEC_SHOW 注释）
     *   本列表 → 点曲名（第二行大字 s_np_name）
     *            ⚠️ 10-11 起只剩这一处入口（第九行那个小曲名已删，见 s_lbl_playname 注释）
     *   收藏 → 第一行（s_lbl_favbtn）
     *   ⇒ 3.5 右栏那一整串控件在 2.8 全部有了新家，一个功能都没丢
     *     （唯一主动删掉的是"频谱形态切换"——那是本轮新加又撤掉的，不是 3.5 的）。*/

    /* ★★ 2.8：3.5 在这里还有「三个操作键（上一个/暂停/下一个）」
     *   +「三个小键（收藏/本地/本列表）」，它们与第一行的传输键、
     *   第八行的倍速/本地重复 ⇒ 全部删掉，一个功能都没少：
     *     上一个 / 暂停 / 下一个 → 第一行
     *     收藏                  → 第一行
     *     本地                  → 第八行
     *     本列表                → 点曲名
     *   ⚠️ 这也是 2.8 排版的核心手法：**320×240 上不该有两处同样的入口**，
     *      3.5 的 480 宽能容下重复，2.8 容不下 —— 重复的必须收成一个。*/

    /* ★★ 2.8：3.5 的「倍速键（78×22）」和「频谱模式键（80×22）」也在这块
     *   右栏里，两者都已搬到新位置：
     *     倍速 → 上面第八行的 s_btn_speed（44×16，字只写档位「1.0X」）
     *     频谱模式 → 频谱区本身（spec_build 里的 272×48 透明热区 + 标题条）
     *   ⇒ 这里不再建任何对象，`int x = 256` 这个变量也随之消失
     *     （它正是「整栏在屏外」的根源坐标）。*/

    /* ★ 10-11 播放页手势（左右滑切台/曲、上下滑音量）——见 ev_play_gesture 上方。
     *   ⚠️⚠️ clear GESTURE_BUBBLE 是【必须】的，不是优化：
     *      LVGL 判定手势时会把对象一路往父级抬（lv_indev.c:1841），
     *      抬到【第一个关掉这个 flag 的祖先】才发事件。
     *      不关的话，手势直接送到屏幕 —— 这里挂的回调一次都不会被调用，
     *      而且没有任何报错、日志也看不到，是最难查的一类"没反应"。*/
    lv_obj_clear_flag(p, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(p, ev_play_gesture, LV_EVENT_GESTURE, NULL);
}

/* ============================================================
 *  页面四：夜间模式（含亮度调节）
 * ============================================================ */
static lv_obj_t *s_lbl_bright;
static lv_obj_t *s_lbl_sleep_left;   /* 睡眠定时剩余时间（10-04 新增）*/

/* ★★ 10-11 兰兰：「亮度也改按键，步进10」（同音量那条）。
 *   原来这里挂着滑块，用 ev_bright_changed 读 lv_slider 的值。*/
#define BRIGHT_STEP 10

static void ev_bright_step(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    int v = app_sys_get_brightness() + dir * BRIGHT_STEP;

    /* ⚠️⚠️ 下限必须是 10，不能是 0 —— 这是「一步走错就回不来」的操作：
     *   亮度 0 = 背光全灭 = 屏幕全黑，用户【看不见那个 + 键在哪】，
     *   只能靠重启或者摸黑乱点。滑块时代的最小值是 5（同样理由），
     *   换成按键后更容易一路按到底 ⇒ 这道闸门比那时更重要。
     *   （app_display.c 的 app_backlight_set 只把值夹在 0..100，不挡 0。）*/
    if (v < 10)  v = 10;
    if (v > 100) v = 100;

    app_sys_set_brightness(v);
    if (s_lbl_bright) lv_label_set_text_fmt(s_lbl_bright, "%d%%", v);

    /* ★ 10-11 补：留一行日志。
     *   为什么这条不能省（同 ev_vol_step / ev_mode_pick 都必须有）：
     *     本工程所有「可点时看不到屏」的控件都靠 tap_note 留痕，
     *     我在串口侧才能**证明**按下生效、以及读回真实落点坐标
     *     （`lv=(x,y)` 用于反查布局有没有偏）。
     *   亮度这三个键是夜间页唯一没留痕的控件 —— 补上之后，
     *   音量/模式/亮度/睡眠四组控件就都可用同一套手法验证了。
     *   ⚠️ 文案必须与读数区分开：这里是「亮度」，不要和「音量」混。*/
    tap_note(dir > 0 ? "亮度 +10" : "亮度 -10");
}

/* ---- 睡眠定时：只用固定档位 ----
 * ★★ 10-04 第二十七次（兰兰原话「定时自由按键还是取消了比较好，
 *   把时间固定按键改好了多几个选择即可」）：
 *   上一版的 －/数值/＋ 三颗【删掉了】。三条理由，都不是审美问题：
 *   ① 数值框太宽（248 px，占整行 2/3），而它显示的「45 分 (步长 5)」
 *      是全页信息密度最低的一块 —— 看得懂「步长 5」的人比看得懂
 *      「还有 47 分 12 秒」的人少得多，那一行才是真正要盯的。
 *   ② 它挡住了剩余时间那行（兰兰原话「输出的还有几分几秒自动停播
 *      被自由定时按键和框挡住了」）。
 *   ③ 固定档已经覆盖绝大多数场景（兰兰两次精简：先去掉 15，再去掉 45；
 *      10-04 15:20 又要求补上 240 = 4 小时，最终
 *      关闭/30/60/90/120/150/180/240）。
 *   ⇒ 结论：固定档 + 一个「关闭」，8 颗，分两行（4 + 4）。*/
#define SLEEP_N 8
static const int s_sleep_mins[SLEEP_N] = {0, 30, 60, 90, 120, 150, 180, 240};

/* 前向声明：page_night_build 在这些之后才用得到它们 */
static void sleep_left_cb(lv_timer_t *t);
static void ev_sleep_pick(int idx);

static void ev_sleep_pick(int idx)
{
    if (idx < 0 || idx >= SLEEP_N) return;
    app_sleep_set_minutes(s_sleep_mins[idx]);
}

/* 剩余时间那一行。每秒刷。*/
static void sleep_left_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_lbl_sleep_left) return;
    if (!app_sleep_is_on()) {
        /* 已到点：说清楚「停了」，别让用户以为定时还在跑 */
        lv_label_set_text(s_lbl_sleep_left,
                          app_sleep_fired() ? "已到点 · 播放已停止" : "");
        return;
    }
    int left = app_sleep_remaining_s();
    int m = left / 60, sec = left % 60;
    lv_label_set_text_fmt(s_lbl_sleep_left, "还有 %d 分 %02d 秒后自动停播", m, sec);
}

/* 睡眠定时的 1 秒心跳：先判到点（要在任何页面都生效），
 * 再刷夜间页那行字（只在夜间页刷，避免别的页白白重绘）。*/
static void sleep_timer_cb(lv_timer_t *t)
{
    (void)t;
    bool fired = app_sleep_tick();
    if (s_cur_page == 3 || fired) sleep_left_cb(t);
}

static void page_night_build(lv_obj_t *p)
{
    /* ★ 10-04 改：原来写死 "23:47" / "10 月 3 日 · 星期六" / "播放中 · 中国之声"，
     *   三行全是假的（兰兰看到方块就是这里）。现在初值给占位，由定时器刷真值。*/
    s_lbl_clock = label(p, 0, 4, F_BIG, C_WARM, "--:--");
    lv_obj_set_width(s_lbl_clock, CONT_W);
    lv_obj_set_style_text_align(s_lbl_clock, LV_TEXT_ALIGN_CENTER, 0);

    /* ★ 10-05 兰兰反馈「夜间页内时钟和当前日期字体可放大」，
     *   但补一句「夜间页时钟保持原状」⇒ 只放大【日期】，时钟维持 F_BIG(24px)。
     *   日期：F_TINY(12px) → F_MID(16px)。它整宽居中、上下都有留白
     *   （时钟在 y=10，状态行在 y=74），16px 字高 16，54+16=70 < 74 ⇒ 不碰。
     *   ⚠️ 时钟为什么不能放大（我查过，别再反复试）：
     *     要 32px 就得新增一档。实测体积（全字库 630 字）= 1.63 MB；
     *     而时钟只显示「23:47」⇒ 只有 0-9 和冒号 11 个字符，
     *     单独生成 32px 纯数字字库只要 13.9 KB —— 技术上可行。
     *     但 24→32 是 1.333 倍非整数缩放，像素风会被拉糊，
     *     而夜间页是这台机唯一「躺下来听」的场景，糊字最毁氛围。
     *     兰兰最终决定：保持 24px。留档说明以免以后重复评估。*/
    s_lbl_date = label(p, 0, 30, F_MID, lv_color_hex(0x8C8471), "日期等待联网校时");
    lv_obj_set_width(s_lbl_date, CONT_W);
    lv_obj_set_style_text_align(s_lbl_date, LV_TEXT_ALIGN_CENTER, 0);

    s_lbl_night_state = label(p, 0, 52, F_BODY, C_WARM, "未在播放");
    lv_obj_set_width(s_lbl_night_state, CONT_W);
    lv_obj_set_style_text_align(s_lbl_night_state, LV_TEXT_ALIGN_CENTER, 0);

    /* 细波浪
     * ★★ v1.35 砍掉：原来这里 for (i<40) 建了 40 个 box 当「波浪竖条」。
     *   夜间页进页面就崩的元凶之一 —— 每个 box 约 250~400 B，
     *   40 根就是 **10~16 KB**，而池只剩 13.7 KB。
     *   ⚠️ 纯装饰的东西永远不值 40 个对象。一条线足够表达「这里分隔一下」。
     *   （真要波浪感，用一张小图或改字体符号，不要用对象堆。）*/
    box(p, 0, 72, CONT_W, 1, lv_color_hex(0x2E2A20), 0);

        label(p, 12, 76, F_TINY, lv_color_hex(0x8C8471),
          "睡眠定时（分钟，到点自动停播）");
    /*★★★ 10-04：这排胶囊以前是【死的】——建了、写了「15/30/60/90 分」，
     *   点了会变色（grp_apply）但【不做任何事】，因为 on_change 没接。
     *   兰兰要的就是这个功能。现在接上 app_sleep。*/
    grp_init(&s_grp_sleep, 1, lv_color_hex(0x2E2A20), lv_color_hex(0x1B1811),
             C_AMBER, lv_color_hex(0x8C8471), "定时");
    /* ★ 「关闭」放第一个：它是「回到默认」的动作，放首位最符合直觉，
     *   也免得在最右边（靠近边缘、容易误触）。*/
    /* ★★ 文案只写数字，单位「分钟」提到小标题里 ——
     *   「180 分」比「180」宽 20 px，9 颗挤两行时这 20 px 就是余量。
     *   少两个字换回一整行的呼吸空间，划算。*/
    /* ★★★ 兰兰 10-04 报「点睡眠页就重启」的真正原因（v1.35 修）：
     *   这个数组声明成 mins[SLEEP_N]（= 8）却只给了 7 个初值，
     *   ⇒ 第 8 个元素编译器填 **NULL**。
     *   第 8 颗胶囊 grp_chip(..., mins[7]) ⇒ lv_label_set_text(标签, NULL)
     *   ⇒ 进夜间页时 LVGL v9 走到 strlen/lv_text_get_size(NULL)
     *   ⇒ 立刻崩。而开机建页时不崩，因为那段代码不碰文本。
     *
     *   ⚠️ 教训（并且我【自己又犯了一遍】，写完立刻被编译器抓住）：
     *      「数组长度不要直接写元素个数」这条建议本身有坑 ——
     *      改成 mins[SLEEP_N - 1] 后长度变成 7，可我照旧给了 8 个初值，
     *      编译器立刻报 excess elements（这正是我们想要的：编译期挡住）。
     *      ★★ 所以正确写法是：**长度仍然写元素个数，用「少一个」当护栏是不对的**，
     *         真正的护栏是下面那句 ——【改了 SLEEP_N 就必须同步核对这个数组的项数】，
     *         而 _arrlen_check.py 会自动检查声明长度与实际项数是否一致。
     *
     *   8 颗 = 关闭/30/60/90/120/150/180/240（最后 4 小时）。*/
    static const char *mins[SLEEP_N] = {
        "关闭", "30", "60", "90", "120", "150", "180", "240"
    };
    /* ★★★ 10-04 第二十七次：8 颗分两行（4 + 4）。
     *   ★ 兰兰原话「夜间页定时可以做60分钟，90分钟，120分钟，150分钟固定按键」
     *     —— 15 分和 45 分都去掉，只留下 30/60/90/120/150/180。
     *
     *   横向账（CONT_W = LCD_H_RES(480) － RAIL_W(56) = **424**，可用 384）：
     *     每行 4 颗 ⇒ step = 384 / 4 = 96、宽 91
     *     末颗右缘 = 20 + 3*96 + 91 = 399 ≤ 424 ✓（右缘余 25 px）
     *     最宽文案「180」= 3 位数字 ≈ 24 px，加内边距 24 ⇒ 需 48 ≤ 91 ✓（余 43 px，很宽裕）
     *   ⚠️ 6 颗一行是 NG 的：step=64 ⇒ 宽 59，而「180 分」要 68。
     *     上一版 5 颗时余量只剩 3 px —— 【文案去单位】就是为了把这个
     *     3 px 变成 23 px，不要再往回加「分」字。     *   ⚠️ 算横向账一律用 CONT_W，不是 480 —— 再上一版就是这里出的错
     *     （按 480 算末颗落在 452，被导航栏裁掉 28 px）。*/
#define SLEEP_X0    12
#define SLEEP_AVAIL (CONT_W - 2 * SLEEP_X0)      /* 256 */
#define SLEEP_PER_ROW 4
#define SLEEP_STEP  (SLEEP_AVAIL / SLEEP_PER_ROW) /* 64 */
#define SLEEP_CW    (SLEEP_STEP - 5)              /* 59 */
#define SLEEP_CH    30
    for (int i = 0; i < SLEEP_N; i++) {
        /* 行 1 = i 0~3（y 90），行 2 = i 4~7（y 124）。
         * ★ 用取余/整除算行号，不要写死两段循环 ——
         *   以后加第 10 档（210 分）只需把 SLEEP_N 改 10、这里不用动。
         * ★ 2.8：两行的 y 从 126/170 挪到 90/124（页高 212，下面还要留
         *   剩余时间 + 亮度标题 + 滑块三样）。*/
        int col = i % SLEEP_PER_ROW;
        int row = i / SLEEP_PER_ROW;
        grp_chip(&s_grp_sleep, p, SLEEP_X0 + col * SLEEP_STEP,
                 90 + row * (SLEEP_CH + 4), SLEEP_CW, SLEEP_CH,
                 12, F_BODY, mins[i]);
    }
    s_grp_sleep.on_change = ev_sleep_pick;
    /* 恢复上次设定：NVS 里存的是分钟数，要映射回胶囊下标。
     * ★ 用 s_sleep_mins 而不是 atoi_minutes(mins[i])：
     *   文案里已经没有数字了（只有「关闭」有中文），反查不回来；
     *   直接按下标查表，方向单一、不易错。*/
    {
        int sv = app_sleep_minutes();
        int sel = 0;                       /* 默认「关闭」 */
        for (int i = 1; i < SLEEP_N; i++) {
            if (sv == s_sleep_mins[i]) { sel = i; break; }
        }
        s_grp_sleep.sel = sel;
    }
    grp_apply(&s_grp_sleep);

    /* 剩余时间：给一个独立 label，每秒刷。
     * ★ 为什么单独一行而不是塞进胶囊里：胶囊是「设定值」，
     *   这一行是「还剩多久」，两件事混在一颗胶囊里会看不清。*/
    s_lbl_sleep_left = label(p, 12, 158, F_BODY, C_AMBER, "");

    /* ---- 亮度 ----
     * ★ 10-04 第二十七次整页重排：两行胶囊（126~208）占掉了上一版
     *   「自定义三颗」那排的位置，剩余时间和亮度区随之整体下移。
     *
     *   ★★ 纵向账（CONT_H = 320 － TOPBAR_H 38 = **282**，可用 y 0~282）：
     *       10  时钟（F_BIG 38px → 10~48）
     *       54  日期（F_TINY → 54~68）
     *       74  状态（F_BODY → 74~92）
     *       94  波浪分隔线（v1.35：只剩 1 条线，40 根竖条已砍）
     *      108  「睡眠定时」小标题（F_TINY → 108~120）
     *      126  胶囊行一 关闭/30/60/90（高 38 → 126~164）
     *      170  胶囊行二 120/150/180/240（高 38 → 170~208）
     *      214  剩余时间（F_BODY → 214~232）
     *      238  亮度小标题（F_TINY → 238~252）
     *      256  滑块（高 18 → 256~274）
     *       底部余量 8 px
     *   ⚠️ 这一页到底了（两版前余量还有 36 px）。以后想加任何一行之前，
     *      先回来看这张表；「先挤一挤」最多一次，第二次就得砍东西或分页。
     *   ⚠️ 横向账一律用 CONT_W(424) 不是 480，见上面 SLEEP_X0 那段。
     *   ⚠️ 底部那行「暖白去蓝光 · 触摸屏幕恢复 · 拖动滑块调亮度」早就删了：
     *      纵向没位置，而它说的三件事里
     *      「拖动滑块」正在滑块上方看得见、
     *      「暖白去蓝光」是硬件特性（背光本身就没有蓝光）、
     *      「触摸屏幕恢复」指的是息屏唤醒，那是全局行为不是本页功能。*/
    /* ★★ 10-11 兰兰：「亮度也改按键，步进10」。
     *   上一版这里是「标题行 + 256 px 宽的滑块」——
     *   与音量滑块同一个毛病（2.8 上拖动精度不够、且"拖不到头"），
     *   一并换成一排按键。
     *   纵向账（这一页本来就用满了 212）：
     *     176 ~ 190  「屏幕亮度」标题（F_TINY 行高 15）
     *     192 ~ 208  [-] 读数 [+]
     *   横向账（CONT_W=280，左右各留 12 ⇒ 12..268，缝 4）：
     *     [-] 12..84 ｜ 读数格 88..192 ｜ [+] 196..268
     *   与播放页的音量行同一套比例，两页手感一致。*/
    label(p, 12, 176, F_TINY, lv_color_hex(0x8C8471), "屏幕亮度");

    lv_obj_t *bdn = press_btn(p, 12, 192, 72, 16, lv_color_hex(0x1B1811), 8,
                              ev_bright_step, (void *)(intptr_t)(-1));
    {
        lv_obj_t *t = label(bdn, 0, 0, F_MID, C_AMBER, "-");
        lv_obj_center(t);
    }

    {
        /* 中间是读数格（不是按键）⇒ 用比按钮亮一档的底色区分。
         * 数值就是上面那行标题的答案，所以格里只放百分比、不再重复"屏幕亮度"。*/
        lv_obj_t *bc = box(p, 88, 192, 104, 16, lv_color_hex(0x2E2A20), 8);
        s_lbl_bright = label(bc, 0, 0, F_BODY, C_WARM, "85%");
        lv_obj_center(s_lbl_bright);
        /* 初值问真正的背光值（不存在 NVS，是 app_sys 里的运行时值）。*/
        lv_label_set_text_fmt(s_lbl_bright, "%d%%", app_sys_get_brightness());
    }

    lv_obj_t *bup = press_btn(p, 196, 192, 72, 16, lv_color_hex(0x1B1811), 8,
                              ev_bright_step, (void *)(intptr_t)(1));
    {
        lv_obj_t *t = label(bup, 0, 0, F_MID, C_AMBER, "+");
        lv_obj_center(t);
    }
}

/* ---- 夜间页的按需建 / 离开就删（v1.35）----
 * 与第 8 页 WiFi 设置同一套机制，理由也一样：这一页最重，
 * 常驻会把 LVGL 池抽干，进页面时崩在 lv_draw_add_task。
 *
 * ⚠️ 销毁时【必须把句柄全清 NULL】，否则三个 1 秒定时器
 *   （clock_cb / clock_date_cb / night_state_cb / sleep_timer_cb）
 *   下一拍还会往已释放的句柄上写 —— 那是 use-after-free，
 *   症状是「切走几秒后随机崩」，比现在难查十倍。
 *   这些定时器本来就有 if (s_lbl_xxx) 判空（见各自定义），
 *   清 NULL 就自动变成「不刷」。*/
static void night_page_destroy(void)
{
    if (!s_night_built) return;
    s_night_built = false;
    s_lbl_clock = NULL;
    s_lbl_date = NULL;
    s_lbl_night_state = NULL;
    s_lbl_sleep_left = NULL;
    s_lbl_bright = NULL;
    memset(&s_grp_sleep, 0, sizeof(s_grp_sleep));
    lv_obj_clean(s_pages[3]);
    /* 清理 grp 的点击参数池（grp_clicked 的 user_data 指向它，
     * 页面删了这些参数也作废，重建时由 arg_of() 重新分配）。*/
    s_arg_n = 0;
}

static void night_page_ensure(void)
{
    if (s_night_built) return;
    page_night_build(s_pages[3]);
    s_night_built = true;
    /* 建完立刻刷一次：不然要等下一个整秒才显示真实时间 */
    clock_cb(NULL);
    clock_date_cb(NULL);
    sleep_left_cb(NULL);
}

/* ============================================================
 *  页面五：系统状态（关于 + 体检 + WiFi）
 * ============================================================ */
#define STAT_ROWS 7      /* ★ 10-04：6 → 7，新增「电池」行。纵向账见 stat_key 上方 */

static lv_obj_t *s_lbl_stat_key[STAT_ROWS];
static lv_obj_t *s_lbl_stat_val[STAT_ROWS];
static lv_obj_t *s_lbl_scan;

static const char *stat_key[STAT_ROWS] = {
    "固件", "运行", "内存", "音频", "存储", "网络", "电池"
};

/* ---- 状态页纵向账（10-04 加了第 7 行「电池」后重算）----
 *   标题 18px → 38；「重新读取」那行到 40；
 *   7 行键值：y = 40 + i*24，行高 22 ⇒ 最后一行到 40+144+22 = 206；
 *   三个动作按钮从 210 起 ⇒ 还剩 4 px 余量。
 *   ⚠️ 原来是 6 行 / y=42 + i*27 / 行高 24（到 201）。加第 7 行时
 *   试过把行距压到 24：40+144+24 = 208 > 206（会压到按钮），
 *   压到 23 又让 22 高的行几乎贴在一起 ⇒ 最后取【行高 22 / 行距 24】。
 *   ⚠️ 行高 22 配 F_TINY(11px) 时上下各留 5.5 px，视觉上比原来紧一点，
 *   但不裁字。若以后再加第 8 行，必须重算这一段而不是接着往下堆。*/

/* 顶栏最右的 WiFi 标志 + 内存卡标志。
 * ★ 必须每页都刷、每秒都刷 —— 它不在「状态页」里，而是全局可见的指示灯。
 *   所以不能用 stat_refresh()（那个只在第 5 页跑），单独一个轻量函数。
 *
 * ★ 10-03 第九次：只改颜色，不改文案（兰兰要的是「绿色=已连，没标志=没连」，
 *   绿色 WiFi 图标本身就够表意，再写「已连」两个字是冗余）。*/
static void wifi_indicator_refresh(void)
{
    if (s_ic_wifi) {
        switch (app_net_link_state()) {
        case APP_NET_OK:
            lv_obj_set_style_text_color(s_ic_wifi, C_GREEN, 0);
            break;
        case APP_NET_CONNECTING:
            lv_obj_set_style_text_color(s_ic_wifi, C_AMBER, 0);
            break;
        case APP_NET_FAIL:
            lv_obj_set_style_text_color(s_ic_wifi, C_RED, 0);
            break;
        default:
            lv_obj_set_style_text_color(s_ic_wifi, lv_color_hex(0x3A424C), 0);
            break;
        }
    }
    /* 内存卡：没挂载就整个变暗（图标 + 文字一起）*/
    bool has = app_sd_is_mounted();
    if (s_ic_sd) {
        lv_obj_set_style_text_color(s_ic_sd, has ? C_GREEN : lv_color_hex(0x3A424C), 0);
    }
    if (s_lbl_sdinfo) {
        if (has) {
            uint32_t cap_mb = 0;
            const char *fs = NULL;
            app_sd_get_info(&cap_mb, &fs);
            /* 顶栏只有 38 px 高，写不下「29.1 GB」+ 文件系统名，只写容量 */
            lv_label_set_text_fmt(s_lbl_sdinfo, "%uG", (unsigned)((cap_mb + 512) / 1024));
            /* ★ 跟 SD 图标同色（绿）：兰兰 10-03 第三次反馈
             *   「保留的那个距离内存卡标志太远，而且不是绿色字体」*/
            lv_obj_set_style_text_color(s_lbl_sdinfo, C_GREEN, 0);
        } else {
            lv_label_set_text(s_lbl_sdinfo, "无卡");
            lv_obj_set_style_text_color(s_lbl_sdinfo, lv_color_hex(0x3A424C), 0);
        }
    }
}

/* 顶栏电池图标（10-04 新增）。
 * ★ 为什么只放图标不放数字：横账算过了，时钟右端 330、SD 图标 392、
 *   WiFi 450，中间只有 374~388 这一格 14 px —— 塞不下「85」两个字。
 *   ⇒ 顶栏用【颜色】表示档位，百分比数字放状态页与夜间页那两行整宽处。
 *
 * ★ 为什么不能用 s_batt_shown_pct 之外的方式去重：
 *   app_pwr_tick() 5 秒才真读一次 ADC，而本函数每秒都跑；
 *   不比对就每秒重设文字颜色 = 每秒标脏一块区域触发重绘。*/
static void batt_indicator_refresh(void)
{
    if (!s_ic_batt) return;

    int pct = app_pwr_batt_pct();
    bool present = app_pwr_batt_present();

    /* 档位（-1 = 无电池，0~3 = 满/中/低/极低）*/
    int step = -1;
    if (present) {
        if (pct > 50)      step = 0;
        else if (pct > 20) step = 1;
        else if (pct > 5)  step = 2;
        else               step = 3;
    }
    if (step == s_batt_shown_pct) return;
    s_batt_shown_pct = step;

    /* ⚠️ 符号名【必须去 lv_symbol_def.h 查】，别按"看起来该叫什么"拼 ——
     *   我写 LV_SYMBOL_BATTERY_3_BARS / _1_BAR 全是错的，真名是
     *   LV_SYMBOL_BATTERY_3 / _2 / _1（没有 _BARS / _BAR 后缀）。*/
    const char *icon;
    lv_color_t col;
    switch (step) {
    case 0:  icon = LV_SYMBOL_BATTERY_FULL;   col = C_GREEN;                break;
    case 1:  icon = LV_SYMBOL_BATTERY_3;      col = C_AMBER;                break;
    case 2:  icon = LV_SYMBOL_BATTERY_2;      col = C_RED; break;
    case 3:  icon = LV_SYMBOL_BATTERY_1;      col = C_RED; break;
    default: icon = LV_SYMBOL_BATTERY_EMPTY;  col = lv_color_hex(0x3A424C); break;
    }
    lv_label_set_text(s_ic_batt, icon);
    lv_obj_set_style_text_color(s_ic_batt, col, 0);

    /* 同步状态灯。★ 优先级：低电 > 播放中 > 连接中 > 已联网。
     *   低电必须压过其它状态 —— 电快没了的时候，
     *   不管在放什么，蓝绿色的「一切正常」都是误导。
     *   这里是本文件里【唯一】写 app_led_set_mode 的地方，
     *   别在别的函数里也写，否则两处打架、最后谁赢看调用顺序。*/
    if (step == 3) {
        app_led_set_mode(APP_LED_LOW_BATT);
    } else if (app_radio_is_playing() && !app_radio_is_paused()) {
        app_led_set_mode(APP_LED_PLAYING);
    } else {
        switch (app_net_link_state()) {
        case APP_NET_OK:         app_led_set_mode(APP_LED_WIFI);    break;
        case APP_NET_CONNECTING: app_led_set_mode(APP_LED_LINKING); break;
        default:                 app_led_set_mode(APP_LED_OFF);     break;
        }
    }
}

static void stat_refresh(void)
{
    app_status_t st;
    app_sys_get(&st);

    /* ★ 10-10：声音键的文字跟着 s_cfg 走（别的地方也可能改静音态，
     *   比如播放页拖音量时自动解除静音 ⇒ 状态页这里必须跟上）。*/
    snd_apply_label();

    uint32_t up = st.uptime_s;

    if (s_lbl_stat_val[0]) {
        lv_label_set_text_fmt(s_lbl_stat_val[0], "%s · %s", st.fw_ver, st.build_date);
    }
    if (s_lbl_stat_val[1]) {
        lv_label_set_text_fmt(s_lbl_stat_val[1], "%02u:%02u:%02u",
                              (unsigned)(up / 3600), (unsigned)((up / 60) % 60),
                              (unsigned)(up % 60));
    }
    if (s_lbl_stat_val[2]) {
        lv_label_set_text_fmt(s_lbl_stat_val[2], "内部 %u KB / PSRAM %u MB",
                              (unsigned)st.heap_internal_kb,
                              (unsigned)(st.heap_psram_kb / 1024));
    }
    if (s_lbl_stat_val[3]) {
        lv_label_set_text_fmt(s_lbl_stat_val[3], "ES8311 %s · 音量 %d%%",
                              st.audio_ready ? "就绪" : "未就绪", st.volume);
    }
    if (s_lbl_stat_val[4]) {
        if (st.sd_mounted) {
            lv_label_set_text_fmt(s_lbl_stat_val[4], "TF %u MB · 音频 %d 首",
                                  (unsigned)st.sd_cap_mb, st.sd_audio_count);
        } else {
            lv_label_set_text(s_lbl_stat_val[4], "未挂载内存卡");
        }
    }
    /* ★★ 10-04：电量行。顶栏只放得下 14 px 的图标（横账见 topbar_build），
     *   百分比数字只能放这里这一行整宽的位置。
     *   「无电池」和「0%」是两件事，必须分开写：
     *     无电池 = 现在插着 USB 供电；
     *     0%     = 插了电池但真的没电了。*/
    /* ★★ 10-04 第二次改：电量行做成【可点】，用来声明电池在不在。
     *   为什么必须让人点：原理图确认 TP4054 的 CE 脚硬接 GND = 常使能，
     *   没插电池时它照样往 BAT 线灌电流，把读数顶到 4.2V 附近
     *   ⇒ 兰兰实测「没插电池却显示 94% · 4.15 V」。
     *   而满电真电池 4.20V 与空载 4.15V 差距比 ADC 噪声还小，
     *   CHRG1 又没引到 MCU ⇒ 硬件上没有判据，只能让人自己声明。
     *   点一下就切「已接/未接」，不写 NVS（电池是插拔动作）。
     *   「未接」时这一行提示怎么点回来，别让兰兰以为自己点坏了。*/
    if (s_lbl_stat_val[6]) {
        if (app_pwr_batt_present()) {
            int mv = app_pwr_batt_mv();
            lv_label_set_text_fmt(s_lbl_stat_val[6], "%d%% · %d.%02d V（点击=未接）",
                                  app_pwr_batt_pct(), mv / 1000, (mv % 1000) / 10);
        } else {
            lv_label_set_text(s_lbl_stat_val[6], "未接（点击=已接）");
        }
        lv_obj_set_style_text_color(s_lbl_stat_val[6], C_TEXT, 0);
    }
    if (s_lbl_stat_val[5]) {
        if (app_net_scan_running()) {
            lv_label_set_text_fmt(s_lbl_stat_val[5], "扫描中…（上次 %d 个）", st.ap_count);
        } else {
            /* ★ 按「链路状态」分支，不要只判断字符串。
             *   「连接中」只在真的还在重试时出现；重试用完必须变成「连不上 + 原因」，
             *   否则会永远停在一句「连接中」，看着像卡住。*/
            switch (app_net_link_state()) {
            case APP_NET_OK:
                /* ★★ 10-11 改文案（原来 "%s · %s · %d dBm" = IP · SSID · dBm）。
                 *   实测 242px，而值框只有 216px ⇒ 必然折行盖住电池行
                 *   （兰兰报的「dB 挡住百分比」就是这一行）。
                 *   三者（IP 13 字 + SSID + dBm）在 14px 下无论如何塞不进 216px：
                 *   就算把值框加宽到框极限也只多 4px。⇒ **必须减一项**。
                 *   取「SSID · 信号强度」= 132px：这两项才是"一眼看状态"要的
                 *   （连着哪个网 + 信号好不好）。
                 *   ★ IP 没丢：第 8 页 WiFi 设置页显示「已连 SSID · IP · dBm」
                 *     （那行更宽且早就设了 LONG_DOT），要查 IP 去那儿看。
                 *   ⚠️ 别再加回 %s 拼 IP —— 那正是这次的病根。*/
                lv_label_set_text_fmt(s_lbl_stat_val[5], "%s · %d dBm",
                                      st.net_ssid, st.rssi);
                break;
            case APP_NET_CONNECTING:
                lv_label_set_text_fmt(s_lbl_stat_val[5], "连接中… %s", st.net_ssid);
                break;
            case APP_NET_FAIL:
                /* ★★ 10-11 同理：「连不上 · SSID · 原因」= 233px > 216 ⇒ 折行。
                 *   ⇒ 只留原因（7 字 ≈ 98px，实测 155px 含前缀）。
                 *   哪个 SSID 连不上：第 8 页 WiFi 设置页有完整那句。
                 *   失败时"为什么失败"比"是哪个网"更有用（网是你自己刚配的）。*/
                lv_label_set_text_fmt(s_lbl_stat_val[5], "连不上 · %s",
                                      app_net_reason_short(app_net_last_reason()));
                break;
            case APP_NET_NO_CRED:
                lv_label_set_text(s_lbl_stat_val[5], "未配置 · 内存卡里放 wifi.txt");
                break;
            default:
                lv_label_set_text(s_lbl_stat_val[5], "未初始化");
                break;
            }
        }
    }

    /* ⚠️ 顶栏时钟【不在这里刷】—— 10-04 兰兰报「首页标签栏时间没校准，差 4 分钟」。
     *   真因不是没对上时：是这个函数只在状态页被调用
     *   （stat_timer_cb 里 `if (s_cur_page == 4) stat_refresh();`），
     *   顶栏那个 label 在别的页上从建好起就没再被写过，
     *   一直停在上次进状态页那一刻的值 ⇒ 在状态页待过之后玩 4 分钟，就正好差 4 分钟。
     *   ★ 教训（和速查卡铁律 20 同源）：【定时器回调里写 label，
     *     必须确认那个回调在所有页面都会跑】，否则就是一个「建好就死」的死 label。
     *   现在统一搬到 clock_cb：1 秒一次、全页面生效，那里是唯一真相源。*/

    /* 扫描结束后把结果打到串口（屏幕放不下，串口可查） */
    static int last_aps = -1;
    if (!app_net_scan_running() && st.ap_count > 0 && st.ap_count != last_aps) {
        last_aps = st.ap_count;
        for (int i = 0; i < st.ap_count; i++) {
            char nm[40];
            int rssi = 0;
            if (app_net_scan_get(i, nm, sizeof(nm), &rssi)) {
                ESP_LOGI(TAG, "AP[%d] %s  %d dBm", i, nm, rssi);
            }
        }
    }
}

static void ev_scan_wifi(lv_event_t *e)
{
    (void)e;
    tap_note("扫描 WiFi");
    app_net_scan_start();
    if (s_lbl_scan) lv_label_set_text(s_lbl_scan, "扫描中…");
}

/* ★★ 10-04 第二次改：点「电池」那一行 = 声明「我插了 / 没插电池」。
 *
 *  为什么必须有这个（兰兰实测「没插电池却显示 94% · 4.15 V」）：
 *    原理图确认 **TP4054 的 CE 脚硬接 GND = 充电常使能**，
 *    没插电池时它照样往 BAT 线灌电流、把读数顶到 4.2V 附近，
 *    而满电真电池也是 4.20V —— **靠电压区分不了**，
 *    CHRG1 脚又没引到 MCU ⇒ 硬件上没有判据，只能让人自己说。
 *
 *  ★ 为什么不写 NVS：电池是插拔动作。固化到 flash 会出现
 *    「换了电池还记着上一块」的怪事，重启一次就恢复更符合直觉。
 *
 *  ⚠️ 纵向账：这一行不加任何新对象，复用已有的 label 当点击热区
 *     （池只剩 13.7 KB，used 87%，每个带样式对象 ≈ 350 B）。*/
static void ev_batt_toggle(lv_event_t *e)
{
    (void)e;
    bool now = !app_pwr_batt_present();
    app_pwr_batt_set_present(now);
    tap_note(now ? "已接电池" : "未接电池");
    /* 顶栏图标也走同一个判据，这里主动刷一次，别等下一秒的定时器 */
    s_batt_shown_pct = -2;
    batt_indicator_refresh();
    stat_refresh();
}

/* 从 SD 卡的 wifi.txt 重新连接。
 * ★ 有了这个按钮，换 WiFi / 首次往卡里放文件之后都不用重启机器。*/
static void ev_connect_wifi(lv_event_t *e)
{
    (void)e;
    tap_note("连 wifi.txt");
    esp_err_t r = app_net_connect_from_file("/sdcard/wifi.txt");

    if (s_lbl_stat_val[5]) {
        if (r == ESP_OK) {
            lv_label_set_text(s_lbl_stat_val[5], "已发起连接…");
        } else if (r == ESP_ERR_NOT_FOUND) {
            lv_label_set_text(s_lbl_stat_val[5], "内存卡根目录没有 wifi.txt");
        } else if (r == ESP_ERR_INVALID_ARG) {
            lv_label_set_text(s_lbl_stat_val[5], "wifi.txt 第一行是空的");
        } else {
            lv_label_set_text_fmt(s_lbl_stat_val[5], "连接失败 (%s)", esp_err_to_name(r));
        }
    }
    ESP_LOGI(TAG, "connect_from_file -> %s", esp_err_to_name(r));
}

static void ev_refresh_stat(lv_event_t *e)
{
    (void)e;
    tap_note("刷新状态");
    stat_refresh();
}

static void page_about_build(lv_obj_t *p)
{
    /* ★★ 2.8 重排（10-10）：3.5 的账（标题 8,6 ／ 重新读取 x=328 ／ 7 行宽 408 ／
     *   三键 y=210 高 40 ／ 两键 y=254）里，除了标题，**每一个元素都在
     *   280×212 之外** —— 兰兰说的「看不见的按键、超出窗口的按键」就是这一页。
     *   新账（页高 212）：
     *     2..22     标题行：系统状态（18px）+ 刷新键（右）
     *     24..162   7 行键值（步进 20、行高 18）
     *     166..188  动作键第一行：扫WiFi / 连wifi.txt / 提示音 / 配网
     *     189..209  动作键第二行：关机（宽 120，容得下「再点一次关机」）
     *   ※ 底部那行「版本号」不再单独占位 —— 「固件」那一行的值本来就是
     *     「vX.YY · 日期」（见 stat_refresh 的 s_lbl_stat_val[0]）。
     *   ※ 关机键【独占一行】而不是挤进标题行：它的文案会变成
     *     「再点一次关机」（6 字 = 84 px）和「正在关机…」，50 px 的小键装不下。*/
    label(p, 6, 2, F_MID, C_TEXT, "系统状态");

    lv_obj_t *rf = tick(p, 228, 2, 48, 20, C_HILITE, 10, NULL);
    lv_obj_add_event_cb(rf, ev_refresh_stat, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rfl = label(rf, 0, 0, F_TINY, C_TEXT, "刷新");
    lv_obj_center(rfl);

    /* 7 行键值。★ 10-04：y 从 42+i*27 改为 40+i*24、行高 24→22，
     *   否则加第 7 行会一路顶到 228，直接压到 y=210 的按钮上。
     *   完整纵向账见 stat_key 上面那段注释。
     *
     * ★★ 10-04 烧录后崩过一次（EXCVADDR=0x08 @ lv_draw_add_task，
     *   池 free=13636 B used=87%），原因就是这一圈加了 3 个带样式的对象。
     *   ⇒ **只有前 6 行画底 box，第 7 行（电池）不画**：
     *     省下一个 box（含 local style ≈ 350 B），
     *     而这一行是电池（状态页最不常看的一行），视觉上少个底框无感。
     *   ★★ 这是速查卡铁律「加 UI 前先算 lv_obj 数」的第四次应验：
     *     基线 already 87%，**每加一个带样式的对象都在吃崩溃额度**。
     *     下次再加行，请先想「哪一行可以不画底框」。*/
    for (int i = 0; i < STAT_ROWS; i++) {
        /* ★ 2.8：步进 24 → 20、行高 22 → 18、宽 408 → 268。
         *   7 行 ⇒ 24 + 6×20 = 144，末行下沿 162。
         *   ⚠️ 行高 18 配 14px 字（行高 17）⇒ 上下各留 0.5 px，不裁字。*/
        int y = 24 + i * 20;
        /* 第 7 行（i==6，电池）不画底 box —— 见上面说明 */
        if (i < 6) box(p, 6, y, 268, 18, C_PANEL, 6);
        s_lbl_stat_key[i] = label(p, 14, y + 1, F_TINY,
                                  i < 6 ? C_MUTED : C_AMBER, stat_key[i]);
        s_lbl_stat_val[i] = label(p, 56, y + 1, F_TINY, C_TEXT, "…");
        /* ★★ 10-11 两处修正（兰兰真机：「网络数据超出一行会换行到电池行，
         *   实测就是 dB 挡住了电池百分比」）：
         *   ① 宽 212 → 216：底框是 6..274，值从 x=56 起 ⇒ 右缘 272，
         *      正好把框用满（原来右边白留 6px）；键最多 2 字（28px，到 x=42），
         *      所以 x=56 是安全的，不用动。
         *   ② **必须显式设 LONG_DOT**：label 默认是 LV_LABEL_LONG_WRAP，
         *      一旦文案超宽就折到第二行 ⇒ 行距只有 20、行高 18 ⇒
         *      第二行正好压在下一行上（第 7 行电池还没底框，看着就是"电池被挡住"）。
         *      实测数值（用已生成字体的真实 adv_w 量，见 ESP32-Platform/_ui_width.py）：
         *        「192.168.1.111 · MyHome · -51 dBm」= **235px > 216** ⇒ 必然折行。
         *        （示例里的 SSID 是占位名 —— 发布物里不写真实网络名。）
         *      ⇒ 这是【全 7 行】的结构性护栏：以后任何行文案写长了，
         *        最坏是出省略号，绝不会再盖住下一行。
         *      ⚠️ 别把 ① 当成解法 —— 只加 4px 救不了 26px 的超出。*/
        lv_obj_set_width(s_lbl_stat_val[i], 216);
        lv_label_set_long_mode(s_lbl_stat_val[i], LV_LABEL_LONG_DOT);
        /* ★ 第 7 行（电池）挂点击 = 声明电池在不在。
         *   硬件判据不存在（TP4054 的 CE 常使能，空载也 4.15V），
         *   只能让人点。热区就用这个 label，**不加新对象**。*/
        if (i == 6) {
            lv_obj_add_flag(s_lbl_stat_val[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_lbl_stat_val[i], ev_batt_toggle,
                                LV_EVENT_CLICKED, NULL);
        }
    }

    /* ---- 动作键第一行（2.8：横向账 58+80+58+58 + 3×4 = 266，起点 x=6 ⇒ 6..272）----
     *   ★ 文案比 3.5 短：「扫描周围 WiFi」→「扫WiFi」、「连 wifi.txt」保留
     *     （它要指向卡里那个真实文件名，不能改名）。
     *   ★★ 10-10 真机截图后把缝从 3 px 加到 4 px：3 px 在 320 宽的屏上
     *     基本看不出来，「扫WiFi」和「连 wifi.txt」看着是连成一整条的。*/
    lv_obj_t *bs = tick(p, 6, 166, 58, 22, C_HILITE, 11, NULL);
    lv_obj_add_event_cb(bs, ev_scan_wifi, LV_EVENT_CLICKED, NULL);
    s_lbl_scan = label(bs, 0, 0, F_TINY, C_TEXT, "扫WiFi");
    lv_obj_center(s_lbl_scan);

    /* 连 wifi.txt：验证阶段的连网入口（发行版会换成扫描列表 + 屏幕键盘）*/
    lv_obj_t *bc = tick(p, 68, 166, 80, 22, lv_color_hex(0x1B2A3A), 11, NULL);
    lv_obj_add_event_cb(bc, ev_connect_wifi, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bcl = label(bc, 0, 0, F_TINY, C_CYAN, "连 wifi.txt");
    lv_obj_center(bcl);

    lv_obj_t *bb = tick(p, 152, 166, 58, 22, lv_color_hex(0x1B3A31), 11, NULL);
    lv_obj_add_event_cb(bb, ev_beep, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bbl = label(bb, 0, 0, F_TINY, C_GREEN, "提示音");
    lv_obj_center(bbl);

    /* ★★ 第二十二次新增「WiFi 设置」入口（第 8 页），第二十四次改了它的用途：
     *   兰兰否决了屏幕键盘（「又要键盘操作也不方便」），改成手机网页配网。
     *   所以这个键现在通到「开热点 → 手机连 → 自动弹网页」那一页。*/
    lv_obj_t *bw = tick(p, 214, 166, 58, 22, lv_color_hex(0x1B3A31), 11, NULL);
    lv_obj_add_event_cb(bw, ev_open_wifi_page, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bwl = label(bw, 0, 0, F_TINY, C_GREEN, "配网");
    lv_obj_center(bwl);

    /* ---- 关机键（10-04 新增；★ 2.8 挪到第二行独占）----
     * ★★ 3.5 那次教训（兰兰报「关机键挡住了下面的字，露出部分的兰兰」）：
     *   **根因不是坐标算错半格，是加按钮时没把这一行已有的元素列进来算账**。
     *   ⇒ 2.8 这里干脆让它【独占一行】，不再和任何元素挤：
     *     宽 120 是为了容得下它自己的第二态「再点一次关机」（6 字 = 84 px）。
     *
     * ⚠️ 这里【必须】是 tick(按钮) + 独立 label 两个对象，不能省掉 label：
     *   tick() 建的是 **box**，给它调 lv_label_set_text() 会按 label 的结构
     *   去解释一个 box 的指针 ⇒ LoadProhibited 崩在 set_text_internal
     *   （EXCVADDR=0x08）。省 350 B 不值得换一次必崩。
     *
     * ★★ 10-10 真机截图：原来 y=192 h=20 ⇒ 下沿正好 = CONT_H(212)，
     *   按钮底边【贴着屏底】看起来是被切了一半。
     *   ⇒ 抬到 y=189、下沿 209，留 3 px 余量。*/
    /* ---- 第二行：声音总开关（左） + 关机（右）----
     * ★★ 10-10：声音键是【这轮才补的】，原因见 ev_toggle_mute 上方那段。
     *   横向账：6 + 68 = 74 ｜ 缝 6 ｜ 80 + 120 = 200 ⇒ 6..200 ✓（< 280）
     *   为什么放第二行而不是第一行：第一行 4 个键已经占满 6..272，
     *   硬挤会把「连 wifi.txt」压到放不下（它是最长的文案）。*/
    lv_obj_t *bsnd = tick(p, 6, 189, 68, 20, lv_color_hex(0x1B2A3A), 10, NULL);
    lv_obj_add_event_cb(bsnd, ev_toggle_mute, LV_EVENT_CLICKED, NULL);
    s_lbl_snd = label(bsnd, 0, 0, F_TINY, C_GREEN, "有声");
    lv_obj_center(s_lbl_snd);

    s_btn_power = tick(p, 80, 189, 120, 20, lv_color_hex(0x2A1416), 10, NULL);
    lv_obj_add_event_cb(s_btn_power, ev_power, LV_EVENT_CLICKED, NULL);
    s_lbl_power = label(s_btn_power, 0, 0, F_TINY, C_RED, "关机");
    lv_obj_center(s_lbl_power);
}

/* ============================================================
 *  第 8 页（索引 7）：WiFi 配网
 * ============================================================
 *  ★★★ 2026-10-04 第二十四次：整页重做，屏幕自建键盘【全部删掉】。
 *
 *  兰兰原话：「不用手动输入 WiFi 又要键盘操作也不方便，
 *            还是市场上最主流的方式吧，比如扫码链接或者你觉得什么连接最好」
 *
 *  方案（原理见 app_prov.h）：设备开热点 → 手机连 → 系统自动弹网页
 *  → 网页里下拉选 SSID + 填密码 → 设备连上并存 NVS。
 *  屏幕这一页只管三件事：
 *     ① 告诉你「连哪个热点」
 *     ② 告诉你「网页没自动弹就手动开这个地址」
 *     ③ 显示连接结果
 *
 *  ★ 顺带把「点附近热点会重启」那条路径整个删掉（AP 列表不再存在）。
 *    那个崩溃我到最后也没抓到现场（两轮修复无效、串口监听 600 秒 0 字节），
 *    与其继续赌机制，不如把会崩的路拿掉 —— 现在这一页只有 9 个静态
 *    label + 3 个按钮，没有列表、没有滚动、没有动态建删。
 * ============================================================ */

/* 三个动作键的横向账（★★ 2.8：CONT_W = 280）
 *   4 + 88×3 + 4×2 = 276 ⇒ 4..276 ✓（3.5 那版是 8+136×3+4×2 = 424，
 *   整排右缘 424 在 280 宽的页里超出 144 px）*/
#define PW_BTN_W      88
#define PW_BTN_H      26
#define PW_BTN_Y     178
#define PW_BTN_X0      4
#define PW_BTN_GAP     4

/* 纵向账（页内坐标，★★ 2.8 重排；3.5 那版 tip 在 y=200、按钮在 y=214，
 * 而 2.8 的页只有 212 高 ⇒ 两者都在页外）：
 *   2   标题（18px）+ 返回键（右）  → 22
 *   26  状态行（14px）              → 40
 *   44  步骤 1（14px）              → 58
 *   62  热点名（22px，绿色）        → 84
 *   88  步骤 2（14px）              → 102
 *   104 步骤 3（14px）              → 118
 *   124 手动地址 192.168.4.1（22px，青色）→ 146
 *   152 兜底提示（14px）            → 166
 *   178 三个动作键（26 高）         → 204
 *   底部余量 8 px */
#define PW_STEP1_Y    44
#define PW_SSID_Y     62
#define PW_STEP2_Y    88
#define PW_STEP3_Y   104
#define PW_URL_Y     124
#define PW_TIP_Y     152

static lv_obj_t *s_pw_stat;        /* 状态行（五态 + 配网进展）*/
static lv_obj_t *s_pw_step1;       /* 第 1 步：连热点 */
static lv_obj_t *s_pw_ssid;        /* 热点名（大字，绿色）*/
static lv_obj_t *s_pw_step2;       /* 第 2 步 */
static lv_obj_t *s_pw_step3;       /* 第 3 步 / 结果 */
static lv_obj_t *s_pw_url;         /* 192.168.4.1（大字，青色）*/
static lv_obj_t *s_pw_tip;         /* 兜底提示 */
static lv_obj_t *s_pw_b1;          /* 开始配网 / 已开则变「关掉」 */
static lv_obj_t *s_pw_b2;          /* 重新扫描 */
static lv_obj_t *s_pw_b3;          /* 连 wifi.txt */

/* 前向声明：handler 的定义在挂上去之后（这一段整体替换自文件后段）*/
static void wifi_page_refresh(void);
static void ev_pw_start(lv_event_t *e);
static void ev_pw_scan(lv_event_t *e);
static void ev_pw_back(lv_event_t *e);

/* 内存账：这一页对象少，但仍然每步打一次 —— 万一以后又往这页加东西，
 * 有这行日志才知道该不该加（iron rule 20：别猜，看数字）。*/
static void wifi_mem_note(const char *when)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    ESP_LOGI(TAG, "wifi mem @%-10s free=%6u B  used=%2u%%  peak=%u B",
             when, (unsigned)m.free_size, (unsigned)m.used_pct,
             (unsigned)m.max_used);
}

/* 三个动作键 + 返回键共 4 份共享样式（第二十三次的教训：绝不用
 * lv_obj_set_style_*，那会给每个对象各 malloc 一份 local style）。*/
typedef struct {
    lv_style_t st;
    bool       inited;
} pw_style_t;
static pw_style_t s_pws_btn[3];    /* 绿 / 青 / 灰 */
static pw_style_t s_pws_back;      /* 返回键：灰字 */

static void pw_style_init(pw_style_t *s, lv_color_t fg)
{
    if (s->inited) return;
    s->inited = true;
    lv_style_init(&s->st);
    lv_style_set_bg_color(&s->st, C_HILITE);
    lv_style_set_bg_opa(&s->st, LV_OPA_COVER);
    lv_style_set_radius(&s->st, 5);
    lv_style_set_pad_ver(&s->st, 0);
    lv_style_set_text_font(&s->st, F_TINY);
    lv_style_set_text_color(&s->st, fg);
    lv_style_set_text_align(&s->st, LV_TEXT_ALIGN_CENTER);
}

static void pw_styles_init(void)
{
    /* ⚠️ 颜色表不能写成 `static const lv_color_t t[] = { C_GREEN, ... }`：
     *   C_* 是 lv_color_hex(0x…) 宏，展开成函数调用（对结构体逐字段赋值），
     *   而 C 的静态初始化要求常量表达式 ⇒ gcc 报
     *   `initializer element is not constant`，且报错行指向宏定义那几行，
     *   离真正出错点几百行（第二十三次踩过）。所以逐个赋值。*/
    pw_style_init(&s_pws_btn[0], C_GREEN);
    pw_style_init(&s_pws_btn[1], C_CYAN);
    pw_style_init(&s_pws_btn[2], C_MUTED);
    pw_style_init(&s_pws_back,  C_MUTED);
}

/* 一个可点的 label 当按钮（1 个对象顶 box+label 的 2 个）*/
static lv_obj_t *pw_btn(lv_obj_t *parent, int x, int y, int w, int h,
                        const char *txt, const lv_style_t *st,
                        lv_event_cb_t cb)
{
    if (!parent) return NULL;
    lv_obj_t *l = lv_label_create(parent);
    if (!l) {
        ESP_LOGE(TAG, "pw_btn: 池满，建不出按钮");
        return NULL;
    }
    if (st) lv_obj_add_style(l, st, 0);
    lv_label_set_text(l, txt ? txt : "");
    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, h);
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(l, LV_DIR_NONE);
    lv_obj_add_event_cb(l, cb, LV_EVENT_PRESSED, NULL);
    return l;
}

/* ---- 三个动作键 ---- */
static void ev_pw_start(lv_event_t *e)
{
    (void)e;
    if (app_prov_is_active()) {
        app_prov_clear_result();
        app_prov_stop();
        tap_note("关掉配网");
    } else {
        esp_err_t r = app_prov_start();
        if (r == ESP_OK) {
            app_net_scan_start();   /* 网页里的下拉列表要用 */
            tap_note("开始配网");
        } else {
            char m[56];
            snprintf(m, sizeof(m), "开不了热点 %.36s", esp_err_to_name(r));
            tap_note(m);
        }
    }
    wifi_page_refresh();
    wifi_mem_note("after prov");
}

static void ev_pw_scan(lv_event_t *e)
{
    (void)e;
    tap_note("重新扫描");
    app_net_scan_start();
    wifi_page_refresh();
}

static void ev_pw_back(lv_event_t *e)
{
    (void)e;
    tap_note("返回状态页");
    goto_page(4);
}

static void wifi_page_refresh(void)
{
    if (!s_pw_stat) return;

    /* 扫描完按信号排序 + 同名去重（app_sys 里做，UI 只读）。
     * ⚠️ last_running 之前是函数级 static —— 重进页面时不重置，
     *   会拿上一轮的残留判「扫描是不是刚结束」，排序不执行、顺序是乱的。
     *   现在它是本页的 static，wifi_page_destroy 负责归零。*/
    static int last_running = 1;
    bool running = app_net_scan_running();
    if (last_running && !running) {
        app_net_scan_sort_by_rssi();
        last_running = 0;
    } else if (running) {
        last_running = 1;
    }

    app_status_t st;
    app_sys_get(&st);
    bool prov = app_prov_is_active();
    int  pr = app_prov_result();

    /* ---- 状态行：配网结果优先，其次五态 ---- */
    if (pr == 2) {
        lv_label_set_text_fmt(s_pw_stat, "连上了 %s · %s", st.net_ssid, st.net_ip);
    } else if (pr == 1) {
        lv_label_set_text(s_pw_stat, "正在用你选的网络连接…");
    } else if (pr == 3) {
        lv_label_set_text_fmt(s_pw_stat, "没连上：%s",
                              app_net_reason_short(app_net_last_reason()));
    } else if (pr == 4) {
        lv_label_set_text(s_pw_stat, "网页提交的内容不对，请重试");
    } else if (running) {
        lv_label_set_text(s_pw_stat, "正在扫描周围网络…（约 3 秒）");
    } else {
        switch (app_net_link_state()) {
        case APP_NET_OK:
            /* ★★ 10-11：去掉「已连」前缀（原来 "已连 %s · %s · %d dBm"）。
             *   实测（_ui_width.py，用已生成字体的真实 adv_w）：
             *     带前缀 = **275px**，而本行只有 **268px**（页内容区 6..274）
             *     ⇒ 省略号会吃掉行尾（正好是 dBm 的最后一个字符）。
             *   去掉 3 个字的冗余前缀后 = **243px**，余 25px ⇒ 三者全可见。
             *   ★ 为什么敢去掉：这一行本来就有五种互斥文案（正在连接/连不上/
             *     未配置…），能连上时行色还是绿的 ⇒ 「已连」三个字是重复信息；
             *     而「哪个网 + 什么 IP + 信号多强」才是要看的。
             *   ★ 状态页（第 5 页）那行已改成「SSID · dBm」，IP 就靠这里看全。*/
            lv_label_set_text_fmt(s_pw_stat, "%s · %s · %d dBm",
                                  st.net_ssid, st.net_ip, st.rssi);
            break;
        case APP_NET_CONNECTING:
            lv_label_set_text_fmt(s_pw_stat, "正在连接 %s…", st.net_ssid);
            break;
        case APP_NET_FAIL:
            lv_label_set_text_fmt(s_pw_stat, "连不上 %s · %s",
                                  st.net_ssid,
                                  app_net_reason_short(app_net_last_reason()));
            break;
        case APP_NET_NO_CRED:
            lv_label_set_text(s_pw_stat, "还没连过网 —— 下面点「开始配网」");
            break;
        default:
            lv_label_set_text(s_pw_stat, "网络未初始化");
            break;
        }
    }

    /* ---- 三个步骤行 ---- */
    if (s_pw_step1) {
        if (prov) {
            int c = app_prov_clients();
            if (c > 0) lv_label_set_text_fmt(s_pw_step1, "1. 手机已连上热点（%d 台）", c);
            else       lv_label_set_text(s_pw_step1, "1. 请用手机连下面这个热点");
        } else {
            lv_label_set_text(s_pw_step1, "1. 点「开始配网」，然后手机连这个热点");
        }
    }
    if (s_pw_ssid) {
        if (prov) lv_label_set_text(s_pw_ssid, app_prov_ssid());
        else      lv_label_set_text(s_pw_ssid, "（还没开热点）");
    }
    if (s_pw_step2) {
        lv_label_set_text(s_pw_step2, "2. 连上后手机会自动弹出配网页");
    }
    if (s_pw_step3) {
        if      (pr == 2) lv_label_set_text(s_pw_step3, "3. 已连上，关掉网页就行");
        else if (pr == 1) lv_label_set_text(s_pw_step3, "3. 正在连接，请稍等…");
        else if (pr == 3) lv_label_set_text(s_pw_step3, "3. 密码不对？换个网络再试");
        else              lv_label_set_text(s_pw_step3, "3. 网页里选 WiFi、填密码、点连接");
    }
    if (s_pw_url) lv_label_set_text(s_pw_url, APP_PROV_IP_STR);
    if (s_pw_tip) {
        int n = st.ap_count;
        if (prov && n > 0)      lv_label_set_text_fmt(s_pw_tip, "已扫到 %d 个网络，网页下拉里选", n);
        else if (n > 0)         lv_label_set_text_fmt(s_pw_tip, "附近 %d 个网络（网页里会列出来）", n);
        else                    lv_label_set_text(s_pw_tip, "没扫到也能配，网页里可手输名称");
    }
    if (s_pw_b1) lv_label_set_text(s_pw_b1, prov ? "关掉配网" : "开始配网");
    if (s_pw_b2) lv_label_set_text(s_pw_b2, running ? "扫描中…" : "重新扫描");
}

static void wifi_page_ensure(void)
{
    if (s_pw_stat) return;                    /* 已在页内 */
    lv_obj_t *p = s_pages[7];
    if (!p) return;
    pw_styles_init();

    /* 标题 + 返回（★ 2.8：返回键从 x=336 挪到 210 —— 336 完全在页外；
     *   宽度也按页内可用宽给）*/
    label(p, 6, 2, F_MID, C_TEXT, "WiFi 配网");
    (void)pw_btn(p, 210, 2, 66, 20, "‹ 返回", &s_pws_back.st, ev_pw_back);

    /* 状态行 26..40 */
    s_pw_stat = label(p, 6, 26, F_BODY, C_GREEN, "…");
    lv_obj_set_width(s_pw_stat, 268);
    lv_label_set_long_mode(s_pw_stat, LV_LABEL_LONG_DOT);

    s_pw_step1 = label(p, 6, PW_STEP1_Y, F_TINY, C_MUTED, "");
    lv_obj_set_width(s_pw_step1, 268);
    lv_label_set_long_mode(s_pw_step1, LV_LABEL_LONG_DOT);

    /* 热点名用 22px 大字 —— 这是用户唯一需要照着打的东西，必须醒目。
     * ★ 2.8：F_BIG 在 2.8 上就是最高档 22px（3.5 时代这里是 38px 档，
     *   2.8 没有那一档，22px 已经是「最大可用」）。*/
    s_pw_ssid  = label(p, 6, PW_SSID_Y, F_BIG, C_GREEN, "");
    lv_obj_set_width(s_pw_ssid, 268);
    lv_label_set_long_mode(s_pw_ssid, LV_LABEL_LONG_DOT);

    s_pw_step2 = label(p, 6, PW_STEP2_Y, F_TINY, C_MUTED, "");
    lv_obj_set_width(s_pw_step2, 268);
    s_pw_step3 = label(p, 6, PW_STEP3_Y, F_TINY, C_MUTED, "");
    lv_obj_set_width(s_pw_step3, 268);

    s_pw_url   = label(p, 6, PW_URL_Y, F_H2, C_CYAN, APP_PROV_IP_STR);
    lv_obj_set_width(s_pw_url, 268);

    s_pw_tip   = label(p, 6, PW_TIP_Y, F_TINY, C_MUTED, "");
    lv_obj_set_width(s_pw_tip, 268);
    lv_label_set_long_mode(s_pw_tip, LV_LABEL_LONG_DOT);

    /* 三个动作键（横向账见上面 PW_BTN_* 的注释） */
    s_pw_b1 = pw_btn(p, PW_BTN_X0, PW_BTN_Y, PW_BTN_W, PW_BTN_H,
                     "开始配网", &s_pws_btn[0].st, ev_pw_start);
    s_pw_b2 = pw_btn(p, PW_BTN_X0 + PW_BTN_W + PW_BTN_GAP, PW_BTN_Y,
                     PW_BTN_W, PW_BTN_H, "重新扫描", &s_pws_btn[1].st, ev_pw_scan);
    s_pw_b3 = pw_btn(p, PW_BTN_X0 + (PW_BTN_W + PW_BTN_GAP) * 2, PW_BTN_Y,
                     PW_BTN_W, PW_BTN_H, "连 wifi.txt", &s_pws_btn[2].st,
                     ev_connect_wifi);
    if (!s_pw_b1 || !s_pw_b2 || !s_pw_b3) {
        ESP_LOGE(TAG, "wifi 页按钮没建全（池满）");
    }
    wifi_mem_note("page built");
}

/* ★ 离开第 8 页就把整页对象删掉，把 LVGL 池还回去。
 *   挂在 goto_page 里而不是返回键里 —— 万一是从左边导航栏切走的，
 *   那样就漏掉了（第二十二次真机崩在这：常驻时池从 26.9 KB 掉到 9.5 KB）。*/
static void wifi_page_destroy(void)
{
    s_pw_stat = NULL;
    s_pw_step1 = s_pw_step2 = s_pw_step3 = NULL;
    s_pw_url = s_pw_tip = NULL;
    s_pw_ssid = NULL;
    s_pw_b1 = s_pw_b2 = s_pw_b3 = NULL;
    lv_obj_clean(s_pages[7]);
    wifi_mem_note("page gone");
}

/* 从状态页那个 WiFi 键进来 */
static void ev_open_wifi_page(lv_event_t *e)
{
    (void)e;
    tap_note("WiFi 配网");
    /* ★ 这一页是【按需建、离开就删】的 */
    wifi_page_ensure();
    app_net_scan_start();      /* 进页就扫一次：网页里的下拉列表要用 */
    wifi_page_refresh();
    goto_page(7);
}


/* ============================================================
 *  定时器
 * ============================================================
 * ★★★ 10-04 第二十九次：splash 只能【撤一次】，而且必须把动画定时器一起带走。
 *
 * 症状（兰兰真机报）：「板子不停地发出启动的三声音符」，
 *   串口日志里 `splash done -> main` 每 3.5 秒重复一次。
 *
 * 真因是【同一个回调被两个定时器各建了一次】：
 *   splash_build() 里建了 3500 ms 的 s_splash_timer，
 *   ui_init() 后面又建了一个 2600 ms 的 s_splash_timer  ← 历史遗留
 * 后者把前者的【句柄覆盖】了，前者再也删不掉。
 * 而 splash_done_cb 里是 `lv_timer_del(s_splash_timer)`：
 *   第 2.6 秒那次删掉的是 2.6 s 那个（= 自己，正好收工）；
 *   3.5 s 那个再触发时 s_splash_timer 已是 NULL，`lv_timer_del(NULL)`
 *   在 v9 里是 no-op ⇒ 【它自己永远删不掉自己】⇒ 每 3.5 秒重复一次，
 *   每次都重新 lv_screen_load(主屏) + app_audio_beep_async()。
 *
 * ⇒ 修法三条：① ui_init 里那行多余的 timer 建法删掉（splash_build 已建）
 *             ② 回调加幂等标志 s_splash_done，第二次进来直接返回
 *             ③ 顺手把 60 ms 动画定时器也删掉 —— 它从来没被删过，
 *               而 splash 的子对象在 lv_screen_load 之后已经被释放，
 *               悬空指针每 60 ms 写一次样式就是「海报页卡顿」的来源。*/
static void splash_done_cb(lv_timer_t *t)
{
    (void)t;
    if (s_splash_done) return;          /* 幂等：谁来都不再重放一遍 */
    s_splash_done = true;

    if (s_splash_timer) { lv_timer_del(s_splash_timer); s_splash_timer = NULL; }
    /* ★ 60 ms 动画推送定时器：以前只在注释里说「splash_done_cb 会 del 它」，
     *   实际根本没删。splash 一撤，lv_screen_load(s_main_scr) 会连带释放
     *   splash 的子对象，s_spl_ring[] / s_spl_bar[] 立刻变悬空指针 ——
     *   动画回调还在跑，每 60 ms 往已释放的对象里写样式。*/
    if (s_spl_timer) { lv_timer_del(s_spl_timer); s_spl_timer = NULL; }
    for (int i = 0; i < SPL_RING_N; i++) s_spl_ring[i] = NULL;
    for (int i = 0; i < 5; i++)           s_spl_bar[i]  = NULL;
    s_spl_greet = NULL;

    lv_screen_load(s_main_scr);
    app_backlight_set(app_sys_get_brightness());
    ESP_LOGI(TAG, "splash done -> main");

    /* 开机提示音：不接音源也能证明「喇叭真的会响」*/
    app_audio_beep_async();
}

/* 状态刷新：只在对应页面可见时做，别的时候完全静音。
 * 例外：WiFi 标志是全局指示灯，每页每秒都要刷。*/

static void stat_timer_cb(lv_timer_t *t)
{
    (void)t;
    wifi_indicator_refresh();
    /* ★★ 10-04：电池图标。放在这里而不是某个页面的 refresh 里 ——
     *   顶栏是全局指示区，任何页面都要有。
     *   （同一个坑的第四次：顶栏时钟死 label、首页音乐库「未插卡」、
     *     夜间页假日期，都是「建好之后没人刷」。）*/
    batt_indicator_refresh();
    /* ★ 低电自动关机必须挂在【全页面每秒都跑】的这个回调里。
     *   app_pwr_tick() 内部自己按 5 秒节流，这里只是入口。*/
    app_pwr_tick();
    if (s_cur_page == 4) stat_refresh();
    /* ★ 第 7 页 WiFi 设置：每秒刷一次。
     *   扫描与连接都是异步的（app_sys 里跑独立任务），
     *   所以这一页靠「每秒读一次真实状态」来更新，
     *   与状态页那一行是同一套机制，只是页面不同。*/
    if (s_cur_page == 7) wifi_page_refresh();
    if (s_cur_page == 2) player_refresh();   /* 播放页：进度环 / 电平表按秒走 */

    /* 首页：跟着【真实播放状态】走，别停在旧数据上。
     * 两种会「不同步」的情形：
     *   ① 在播放页按了「下一台」→ 首页大卡还写着上一台；
     *   ② 电台流断了 / 起播失败 → 圆钮还写着「暂停」。
     * 只在真的变了才写 label，否则每秒重绘一次首页是白费。*/
    if (s_cur_page == 0) {
        int cur = app_radio_station_current();
        if (cur >= 0 && cur != s_hr_sel) {
            s_hr_sel = cur;
            home_update_card(cur);
        }
        if (s_playbtn_lbl) {
            bool run = app_radio_is_playing() && !app_radio_is_paused();
            const char *want = run ? "暂停" : "播放";
            const char *have = lv_label_get_text(s_playbtn_lbl);
            if (have == NULL || strcmp(have, want) != 0) {
                lv_label_set_text(s_playbtn_lbl, want);
            }
        }
        /* ★★ 10-04 第二十九次：首页「音乐库」卡那行字要跟着 SD 挂载状态走。
         *  症状：兰兰真机报「首页音乐库显示未插内存卡」，可日志里明明是
         *    `XSSD: SD mounted at /sdcard  cap=29819 MB` + 音频数 337 —— 卡好好的。
         *  真因是【时序】：app_main() 里 ui_init() 在第 3 步、app_sd_mount() 在第 4 步，
         *    建首页那张卡时 SD 还没挂 ⇒ label 写下「未插内存卡」，
         *    而这个 label 之后谁也不刷（首页分支只刷播放按钮和台名）。
         *  ⇒ 挂载状态一变就刷一次那张卡。顺带把「拔卡」也管了。*/
        bool sd_now = app_sd_is_mounted();
        if (sd_now != s_last_sd_mounted) {
            s_last_sd_mounted = sd_now;
            home_cards_refresh();
        }
    }
}

/* ============================================================
 *  频谱专用快速定时器（100 ms）
 * ============================================================
 *  ★ 为什么不能挂在 stat_timer_cb（1 秒）里：频谱一秒才动一次，
 *    看着就是「一格一格跳」，不像频谱。100 ms 一次是流畅与 CPU 的平衡点
 *    —— 一次只是改 12~13 个 box 的 pos/size，不做布局计算，开销可以忽略。
 *  ★ 只在【频谱可见的页】才动手：不在播放页/首页时直接返回，
 *    省得给看不见的控件白改样式（LVGL 改样式会标脏区域，触发重绘）。*/
static void spec_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_cur_page == 2) {
        spec_refresh();
    } else if (s_cur_page == 0) {
        home_spec_refresh();
    }
}

/* ============================================================
 *  拖动专用定时器（20 ms）—— ★ 第二十二次从频谱定时器里拆出来
 * ============================================================
 * ★ 为什么必须独立：
 *   之前 seek_drag_poll() 挂在 100 ms 的 spec_timer_cb 上，
 *   而那个定时器只在播放页/首页干活 —— 于是在别的页面上拖动
 *   依然是 100 ms 一拍（虽然坐标本身已是 33 ms 粒度）。
 *   兰兰 10-04 报「拖动有作用了，但不灵敏」，这一拍就是主因。
 *
 * ★ 20 ms 而不是 10 ms：indev 读周期已经是 10 ms，
 *   轮询再快也只是重复读同一个坐标，纯烧 CPU。
 *   20 ms ⇒ 一秒 50 次，手指划过 244 px 的条每秒更新 50 px，够跟手。
 *
 * ★ 空闲时几乎零成本：第一行就 return，
 *   只有「手指真的按在条上」或「正在拖」才做后面的事。*/
static void seek_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_cur_page != 2) return;      /* 不在播放页就不管 */
    if (!app_radio_is_playing() && !s_seek_drag) return;
    seek_drag_poll();
}

/* ---- 取当前时刻（分钟）----
 * ★ 顶栏与夜间页共用一份：以前夜间页是从 23:47 起每秒自己 +1 的假时钟，
 *   顶栏也没时间。现在有 SNTP 就用真时间；还没校上时退回「开机后的小时数」，
 *   至少两个页面显示的是同一个东西，不会一个真一个假。*/
/* ---- 取当前时刻（分钟）；没对上时返回 -1 ----
 * ★ 为什么要 -1 而不是「退回开机后的小时数」：
 *   10-03 兰兰真机看到顶栏是「00:00」—— 那个 0 点其实是「开机 0 分钟」，
 *   长得跟真的凌晨一模一样，纯误导。宁可显示「--:--」，
 *   一眼就知道「时间还没对上，等 WiFi 连上」。*/
static int now_minutes(void)
{
    time_t t = time(NULL);
    if (t > 1700000000) {                 /* 2023-11 之后 = 真的对上了时 */
        struct tm tmv;
        localtime_r(&t, &tmv);
        return tmv.tm_hour * 60 + tmv.tm_min;
    }
return -1;
}

/* ---- 时钟：顶栏 + 夜间页大钟（10-04 合并）----
 * ★★★ 兰兰报「首页标签栏时间没校准，查 4 分钟，夜间页时间是正确的」。
 *   两个数用同一个 now_minutes()、同一份系统时间，函数上不可能差 4 分钟 ——
 *   差别在【谁在刷】：
 *     夜间页的钟在 clock_cb 里每秒刷（10-04 新加的），所以它对；
 *     顶栏的钟写在 stat_refresh() 里，而那个函数只有状态页会调
 *     （stat_timer_cb: `if (s_cur_page == 4)`），
 *     所以在首页/发现/播放/夜间页上它是【建好之后再没被写过】的死 label，
 *     显示的是上次进状态页那一刻的时间 ⇒ 「差 4 分钟」＝ 他在状态页之后玩了 4 分钟。
 *   ⇒ 现在顶栏也归 clock_cb 管，周期 1 秒（原来 60 秒，顶栏跳分钟看着发闷）。
 *   改口径必须同时改注释，否则下一个人会把它搬回去。*/
static void clock_cb(lv_timer_t *t)
{
    (void)t;
    int m = now_minutes();
    /* ⚠️ 缓冲给 16 而不是刚好 5+1：gcc 对 `%02d` 会按「int 全量范围」算最坏长度，
     *   声明 8 字节会被 -Werror=format-truncation 判可能截断（速查卡老坑）。
     *   这里 m 逻辑上最大 23*60+59，给足就不会误报。*/
    char buf[16];
    if (m < 0) snprintf(buf, sizeof(buf), "--:--");
    else       snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);

    if (s_lbl_clock)    lv_label_set_text(s_lbl_clock, buf);      /* 夜间页大钟 */
    if (s_lbl_topstat)  lv_label_set_text(s_lbl_topstat, buf);    /* 顶栏      */
}

/* ---- 夜间页：真日期 + 星期（10-04 新增）----
 * ★★★ 为什么加这个：兰兰报「夜间页面有方块」。
 *   真因不是字库缺字 —— 是【日期被写死成 "10 月 3 日 · 星期六"】，
 *   那是昨天，而且"月/日"里的空格与"·"是半角，跟周围字体对不上，
 *   在 480×320 上看着就像方块。改成都从系统时间算。
 * ⚠️ 与顶栏共用 now_minutes 那份判断：时间没对上时宁可显示占位，
 *   也不要显示一个「看起来很像真的」的错日期。*/
static void clock_date_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_lbl_date) return;
    time_t now = time(NULL);
    if (now <= 1700000000) {              /* 还没校上时 */
        lv_label_set_text(s_lbl_date, "日期等待联网校时");
        return;
    }
    struct tm tmv;
    localtime_r(&now, &tmv);
    static const char *wd[7] = {"星期日","星期一","星期二","星期三",
                                "星期四","星期五","星期六"};
    int w = tmv.tm_wday; if (w < 0 || w > 6) w = 0;
    lv_label_set_text_fmt(s_lbl_date, "%d 月 %d 日 · %s",
                          tmv.tm_mon + 1, tmv.tm_mday, wd[w]);
}

/* ---- 夜间页：状态行（10-04 改）----
 * ★★★ 兰兰报「夜间页面还有当前播放中的显示」。
 *   现在这行改成【只显示播放状态，不显示台名】：
 *     理由 —— 台名要在夜间页显示，就得把【台单里所有台名的字】都塞进字库，
 *     而字库是我们自己扫源码生成的，只扫 UI 里出现的字。
 *     一旦这里显示台名，699 个台名（含繁体「上海戲曲廣播」）的字都要进主字库，
 *     52px 档会多几百 KB（速查卡铁律 13 踩过这个坑）。
 *   现在改成：状态行只说「在播 / 暂停 / 空闲」，台名去播放页看。
 *   ⇒ 顺便也解决了兰兰说的某个台名里的字画不出来（那个字是繁体，
 *     在主字库里没有 ⇒ 方块）。*/
static void night_state_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_lbl_night_state) return;
    if (app_radio_is_playing()) {
        lv_label_set_text(s_lbl_night_state, "正在收听");
    } else if (app_radio_is_paused()) {
        lv_label_set_text(s_lbl_night_state, "已暂停");
    } else {
        lv_label_set_text(s_lbl_night_state, "未在播放");
    }
}

/* ============================================================
 *  入口
 * ============================================================ */
void ui_init(void)
{
    splash_build();

    s_main_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_main_scr);
    lv_obj_set_style_bg_color(s_main_scr, C_BG, 0);
    lv_obj_set_style_bg_opa(s_main_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_main_scr, LV_OBJ_FLAG_SCROLLABLE);

    topbar_build(s_main_scr);
    nav_build(s_main_scr);

    for (int i = 0; i < PAGES_N; i++) {
        lv_obj_t *p = lv_obj_create(s_main_scr);
        lv_obj_remove_style_all(p);
        lv_obj_set_pos(p, CONT_X, CONT_Y);
        lv_obj_set_size(p, CONT_W, CONT_H);
        lv_obj_set_style_bg_color(p, lv_color_hex(s_page_bg[i]), 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
        s_pages[i] = p;
    }
    page_home_build(s_pages[0]);
    page_find_build(s_pages[1]);
    page_play_build(s_pages[2]);
    /* ★ v1.35：夜间页（第 4 页）也【不在这里建】—— 和第 8 页 WiFi 一样，
     *   「进页才建、离开就删」（钩子在 goto_page 里）。
     *   兰兰真机报「点击睡眠页就重启」：这一页最重（时钟+8 颗胶囊+滑块），
     *   常驻占掉池的一大块，等它第一次显示时 draw task 申请不到就崩。*/
    page_about_build(s_pages[4]);
    page_sd_build(s_pages[5]);
    page_list_build(s_pages[6]);   /* 全屏电台列表：栏目 / 省份 / 收藏 共用 */
    /* ★ 第 8 页 WiFi 设置【不在这里建】—— 它是「进页才建、离开就删」。
     *   常驻的开销是 17 KB，而 LVGL 池只有 ~100 KB，used 直接从 75% 冲到 91%，
     *   splash 一撤就崩在 lv_draw_add_task。详见 goto_page 里的注释。*/

    for (int i = 1; i < PAGES_N; i++) lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);

    /* ★★★ v1.40：开机就要把【首页】那一套建出来。
     *   为什么不能靠 goto_page(0) 顺带建：s_cur_page 初值本来就是 0，
     *   而 goto_page 开头有 `idx == s_cur_page` 就 return 的守卫
     *   ⇒ 开机时 goto_page(0) 会被直接挡掉，首页网格永远建不出来，
     *      兰兰一开机看到的会是一个没有电台名的空网格。
     *   ⇒ 这里显式建一次。其余两套（第 1 页发现、第 6 页分类）
     *      天然由第一次切过去时建，不急。*/
    grid_pages_ensure(0);

    /* ★★★ 10-04 第二十九次：这里原来还有一行
     *      s_splash_timer = lv_timer_create(splash_done_cb, 2600, NULL);
     *   已删除。splash_build() 里已经建过 3500 ms 的那一个，
     *   这一行把它的【句柄覆盖】掉 ⇒ 旧那个再也删不掉 ⇒
     *   splash_done_cb 每 3.5 秒自己触发一次（重复撤屏 + 重复开机音）。
     *   留着这行的教训：splash 的计时器【只有 splash_build 一处】能建。*/
    s_stat_timer   = lv_timer_create(stat_timer_cb, 1000, NULL);
    s_clock_timer  = lv_timer_create(clock_cb, 1000, NULL);
    /* ★★ 10-04：夜间页的日期与状态行各挂一个定时器。
     *   为什么要单独挂：原来这两行是建页时写死的假值，从来没被刷新过
     *   （兰兰看到的方块 + 假日期就是它）。现在都改成从系统时间/播放状态实时取。
     *   日期 60 秒足够（跨零点时顶栏那一跳会带着它一起变），
     *   状态 1 秒 —— 播放状态变化要立刻反映。*/
    s_date_timer   = lv_timer_create(clock_date_cb, 60000, NULL);
    s_nstate_timer = lv_timer_create(night_state_cb, 1000, NULL);
    /* ★★ 10-04 新增：睡眠定时。
     *   这个回调【在所有页面都会跑】（没有 if (s_cur_page == N)）——
     *   千万别加页判断！到点停播必须在你停在任何一页时都生效
     *   （兰兰可能听完歌切到首页才发现声音停了）。
     *   ★ 这正是第二十五次「顶栏时钟死 label」的同一个坑，反面教材。*/
    s_sleep_timer  = lv_timer_create(sleep_timer_cb, 1000, NULL);
    s_spec_timer   = lv_timer_create(spec_timer_cb, 100, NULL);
    /* ★ 拖动轮询独立 20 ms（第二十二次）。见 seek_timer_cb 的注释：
     *   之前挂在 100 ms 频谱定时器上，是「不灵敏」的主因。*/
    s_seek_timer   = lv_timer_create(seek_timer_cb, 20, NULL);
    /* ★ 第二十四次：WiFi 页的 60 ms 待办定时器已删。屏幕键盘去掉之后，
     *   这一页不再需要在事件回调与对象增删之间绕圈子。*/

    /* 内存体检：LVGL 用它自己的内置池（sdkconfig 里 192 KB、不自动扩容）。
     * 池见底时 lv_draw_add_task() 的 lv_malloc_zeroed() 返回 NULL，
     * 而 release 构建下 LV_ASSERT_MALLOC 是空操作 → 往 NULL 写 → 无限重启。
     * 以后再加 UI，先看这行日志的 used%/peak。*/
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);

    /* ★★ 10-10：内存来源从 builtin 池改成 CLIB malloc 之后，
     *   LVGL 9 的 lv_mem_monitor() 变成【空实现】——
     *   lv_mem.c 的 lv_mem_monitor_core() 在非 builtin 模式下直接
     *   lv_memzero(mon_p, sizeof(*mon_p)) ⇒ total_size/free_size 恒为 0。
     *   ⚠️ 这就是个陷阱：total_size==0 是「走系统堆」的标志，
     *      不是「池空了」。若还照旧用 free_size<10240 判，free 恒 0
     *      ⇒ 每次开机都刷一条 "池危险" 的 E 日志，把真正的异常淹掉。
     *   所以：total_size==0 ⇒ 走 else 分支只报来源；真余量一律看
     *   app_stlist.c 的 heap_caps 内存体检（PSRAM 8 MB 在内，不再有 57.5 KB 天花板）。*/
    if (mon.total_size == 0) {
        ESP_LOGI(TAG, "LVGL 内存来源=CLIB malloc（无内建静态池）；余量看 IDF 堆体检");
    } else {
        ESP_LOGI(TAG, "LVGL pool: size=%u B  free=%u B  used=%u%%  frag=%u%%  peak=%u B",
                 (unsigned)mon.total_size, (unsigned)mon.free_size,
                 (unsigned)mon.used_pct, (unsigned)mon.frag_pct, (unsigned)mon.max_used);

        /* ★★★ 10-04 加的「崩溃预警」（本轮真的崩了一次才补的）
         *  v1.31 首次烧录就 Guru：EXCVADDR=0x08 @ lv_draw_add_task，池 free=13636B。
         *  根因就是「基线 already 87%，我又加了 8 个带样式的对象」。
         *  ⇒ 光在注释里写「加 UI 前先算」是靠不住的 —— 这次还是漏了。
         *     改成一个【会在串口说话的自证开关】：
         *       free < 16 KB  ⇒ 黄色警告（还能跑，但已在悬崖边）
         *       free < 10 KB  ⇒ 红色错误（几乎必崩，此时该减对象而不是继续加）
         *  下一次再崩，串口里第一眼就能看到这行，
         *  而不是花半小时去反推「这轮到底加了几个对象」。
         *  ⚠️ 阈值是按 112 KB 池的经验值：
         *     崩的那版 free=13636，peak=88768；正常跑 free 稳定在 26 KB 上下。*/
        if (mon.free_size < 10240) {
            ESP_LOGE(TAG, "★★ LVGL 池危险！free=%u B（<10 KB）—— 立刻要减 lv_obj，"
                          "再加 UI 必崩（EXCVADDR=0x08 @ lv_draw_add_task）",
                     (unsigned)mon.free_size);
        } else if (mon.free_size < 16384) {
            ESP_LOGW(TAG, "★ LVGL 池偏紧 free=%u B（<16 KB）—— 加 UI 前先减对象",
                     (unsigned)mon.free_size);
        }
    }

    stat_refresh();
    player_refresh();          /* 播放页先按「空闲」态画一遍，别停在假数据上 */
    wifi_indicator_refresh();  /* 顶栏 WiFi 标志立刻有值，别停在占位符 */
    /* 首页大卡：开机先按「正在播的那台 / 台单第 1 台」画一遍，
     * 别停在「正在载入台单…」*/
    if (app_radio_station_count() > 0) {
        s_hr_sel = 0;
        home_update_card(0);
    }
    if (s_lbl_favcnt) {
        lv_label_set_text_fmt(s_lbl_favcnt, "%d 个常用台", app_fav_count());
    }
    /* 第 7 页先按「收藏」建一次，开机就是有内容的那一屏 */
    s_list_mode = LIST_BY_FAV;
    s_list_title = "我的收藏";
    list_page_refresh();

    ESP_LOGI(TAG, "UI built (7 pages, %d stations, %d cats, %d provs, %d favs)",
             app_radio_station_count(), NET_CAT_N, NET_PROV_N, app_fav_count());
}

/* ============================================================
 *  调试用：串口切页 + 越界体检（10-10 加）
 * ============================================================
 *  为什么需要这两条：
 *    2.8 重排后最典型的错误【不是报错】而是「对象整块在屏外」——
 *    3.5 遗留的右栏（x=256 起）和 x=328 的返回键都是这样，
 *    肉眼看不出来、编译器也不管。把它们做成串口可调，
 *    于是可以先 `page 3` 切页、再截图，也可以直接让板子
 *    把越界对象一条条报出来，不用人眼找。
 *  ⚠️ 只给调试用；发布版留着也不影响（无副作用、不占常驻内存）。
 * ============================================================ */

void ui_debug_goto(int idx)
{
    if (idx < 0 || idx >= PAGES_N) {
        ESP_LOGW(TAG, "page: 越界 %d（有效 0..%d）", idx, PAGES_N - 1);
        return;
    }
    if (idx == s_cur_page) {
        ESP_LOGI(TAG, "page: 已经在第 %d 页", idx);
        return;
    }
    /* ★ 调试专用：走这条路进第 5 页时，顺手刷一遍 SD 目录。
     *   产品里 SD 页的刷新挂在「点卡内 / 点本地」那两个入口上，
     *   goto_page 本身不刷 ⇒ 串口跳过去会一直停在「读取中…」，
     *   截图看到的是空列表，容易误判成「SD 页坏了」。*/
    if (idx == 5) ui_sd_page_refresh();
    /* ★ 同理：第 8 页（配网）也是「按需建、离开就删」，
     *   只有状态页那颗「配网」键会建它。串口直跳会跳到一张空页
     *   （dump 报「直接子对象 0 个」就是它）。调试路径上补一次。*/
    if (idx == 7) { wifi_page_ensure(); wifi_page_refresh(); }
    goto_page(idx);
    ESP_LOGI(TAG, "page -> %d", idx);
}

/* 递归走一遍某页的对象树，把「右下角超出内容区」的对象打出来。
 * 判据：x + w > CONT_W 或 y + h > CONT_H（内容区左上角当原点）。
 * 为什么用「右下角超界」：3.5 遗留的错法全是「起点就用了 480 宽的
 * 坐标」，只有右下角判据抓得到；左上角为负是另一种错（align 给正值），
 * 一并报出来。深度到 3 层（页 → 卡 → 卡内 label → 更内层）。*/
/* 对象类型名（给 dump 用）。
 * ⚠️ 不能用 lv_obj_get_class(c)->name：lv_obj_class_t 的【结构体定义】
 *    在 lv_obj_class_private.h（私有头），lvgl.h 只给了前置声明
 *    ⇒ 直接读 ->name 编译期就报「incomplete typedef」。
 *    用公开的 lv_obj_check_type() 逐个比，反而更稳。*/
static const char *obj_kind(lv_obj_t *c)
{
    if (lv_obj_check_type(c, &lv_label_class))  return "label";
    if (lv_obj_check_type(c, &lv_button_class)) return "button";
    if (lv_obj_check_type(c, &lv_slider_class)) return "slider";
    if (lv_obj_check_type(c, &lv_obj_class))    return "obj";
    return "widget";
}

static int s_dump_hits;
static void dump_walk(lv_obj_t *o, int ox, int oy, int depth)
{
    uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(o, i);
        if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
        int x = ox + lv_obj_get_x(c);
        int y = oy + lv_obj_get_y(c);
        int w = lv_obj_get_width(c);
        int h = lv_obj_get_height(c);
        if (x + w > CONT_W || y + h > CONT_H || x < 0 || y < 0) {
            s_dump_hits++;
            ESP_LOGW(TAG, "  [越界] %-8s x=%d y=%d w=%d h=%d | 右=%d(应<=%d) 下=%d(应<=%d)",
                     obj_kind(c),
                     x, y, w, h, x + w, (int)CONT_W, y + h, (int)CONT_H);
        }
        /* ★★ 不能往下钻进【可滚动容器】：滚动容器里那些「屏外的帧」
         *   是【故意】摆在外面的（hstrip 的胶囊带比屏宽、网格的 flow
         *   容器比屏高）—— 钻进去会把它们全报成越界，把真问题淹没。
         *   实测第一次跑 dump 就报了 8 条假越界：
         *     x=60 w=2268（胶囊带的整体宽）、x=8 w=1 h=7424（网格 spacer）。
         *   ⇒ 容器自身仍然要判（它才是真正可能摆错的那个），只是不展开。*/
        if (!lv_obj_has_flag(c, LV_OBJ_FLAG_SCROLLABLE) && depth < 3) {
            dump_walk(c, x, y, depth + 1);
        }
    }
}

void ui_debug_dump(int idx)
{
    if (idx < 0 || idx >= PAGES_N) idx = s_cur_page;
    s_dump_hits = 0;
    lv_obj_t *p = s_pages[idx];
    ESP_LOGW(TAG, "dump 第 %d 页（内容区 %dx%d，直接子对象 %u 个）",
             idx, (int)CONT_W, (int)CONT_H, (unsigned)lv_obj_get_child_count(p));
    dump_walk(p, 0, 0, 0);
    if (s_dump_hits == 0) ESP_LOGI(TAG, "  ✓ 无越界对象");
    else                  ESP_LOGW(TAG, "  ✗ 共 %d 个越界对象", s_dump_hits);
}

/* ============================================================
 *  触摸自检层（10-10 加，调试用；串口 `tcal` 开 / `tcal 0` 关）
 * ============================================================
 *  ★ 为什么需要它（兰兰 10-10 报「点提示音却显示扫描中 / 点配网无反应」）：
 *    本板触摸的朝向有 3 位（swap / mirror_x / mirror_y）= **8 档**，
 *    而 esp_lcd_touch 的软件变换顺序是【先镜像、后交换】——
 *      managed_components/espressif__esp_lcd_touch/esp_lcd_touch.c
 *        : esp_lcd_touch_get_coordinates()
 *          if (mirror_x) x = x_max - x;     // 基准 240
 *          if (mirror_y) y = y_max - y;     // 基准 320
 *          if (swap_xy)  { x <-> y; }       // 最后才交换
 *    所以「照显示侧那三位的值抄给触摸」必然错：
 *    镜像作用在一个【还没交换、量程也还是 240x320】的坐标系上，
 *    基准完全不匹配 ⇒ 出来的点是「翻转 + 平移」，不是简单的差半格。
 *    ⇒ 只能【实测定档】。人眼看不见"点这里却算成那里"，
 *      就做个页面让板子把【LVGL 实际收到的坐标】写在屏上：
 *      屏上画 5 个已知位置的黄块 A..E，依次点，下半屏记 6 行读数。
 *      拿"我按的位置"跟"板子记的坐标"一比，档位就定了。
 *  ⚠️ 只建在 lv_layer_top() 上：不碰 s_pages、不参与产品逻辑、随时可删。
 * ============================================================ */
/* ★★ 10-10 第二轮：靶标从 5 颗扩到【9 颗 3×3】。
 *   为什么：5 颗只有 2 个 x 值 × 3 个 y 值，**分不出"倾斜"**——
 *   上一轮四角里 A(靶标 y=20) 读到 18、B(靶标 y=20) 读到 0，同一条靶标线
 *   差了 18 px，这只能是"屏 y 还随 x 变"（面板贴合有小角度）。
 *   3×3 有 3×3 个格点，够做二元线性拟合（含倾斜项）。*/
#define TCAL_MAX  9        /* 靶标颗数（3×3）*/
#define TCAL_SLOT 3        /* 屏上只留最近 3 次读数（9 行 14px 字放不下）*/
static lv_obj_t *s_tcal_root = NULL;
static lv_obj_t *s_tcal_slot[TCAL_SLOT];
static lv_obj_t *s_tcal_prog = NULL;
static lv_obj_t *s_tcal_dot  = NULL;  /* 跟手红点：手指在哪它就画在哪 */
static int       s_tcal_n = 0;      /* 槽位游标 */
static int       s_tcal_seen = 0;   /* 累计按了几次 */

static void tcal_pressed_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    /* ★ 事件回调里要拿"这次按在哪"，唯一正确的方式是问【活动 indev】；
     *   不能读 lv_event_get_target() 的坐标 —— 那是对象的位置，不是触点。*/
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t  p = { 0, 0 };
    if (indev) lv_indev_get_point(indev, &p);

    /* ★★ 10-10 加的【跟手红点】——比 9 个靶标直观得多：
     *   手指在屏上划，红点实时跟着走。这样一眼能看出两件事：
     *     ① 红点跟不跟手（不跟 = 采样还是"只在按下那一刻读一次"）；
     *     ② 红点偏到哪（跟手指差多少、往哪个方向差）。
     *   ⇒ 只在 PRESSED / PRESSING 更新位置，**不写记录**（记录会刷屏）。*/
    if (s_tcal_dot) {
        lv_obj_set_pos(s_tcal_dot, p.x - 5, p.y - 5);
    }
    if (code != LV_EVENT_PRESSED) return;    /* 下面都是"按下那一刻"才做的事 */

    if (s_tcal_n >= TCAL_SLOT) {         /* 满了整体上移一行，永远留最近 3 次 */
        for (int i = 1; i < TCAL_SLOT; i++) {
            lv_label_set_text(s_tcal_slot[i - 1], lv_label_get_text(s_tcal_slot[i]));
        }
    } else {
        s_tcal_n++;
    }
    int idx = s_tcal_n - 1;
    if (s_tcal_slot[idx]) {
        lv_label_set_text_fmt(s_tcal_slot[idx], "%d)  x=%3d  y=%3d",
                              s_tcal_seen + 1, (int)p.x, (int)p.y);
    }
    s_tcal_seen++;
    if (s_tcal_prog) {
        lv_label_set_text_fmt(s_tcal_prog, "已按 %d/%d", s_tcal_seen, TCAL_MAX);
    }
    ESP_LOGW(TAG, "TCAL[%d] LVGL 读到 x=%d y=%d", s_tcal_seen, (int)p.x, (int)p.y);
}

void ui_debug_tcal(int on)
{
    if (!on) {
        if (s_tcal_root) { lv_obj_delete(s_tcal_root); s_tcal_root = NULL; }
        for (int i = 0; i < TCAL_SLOT; i++) s_tcal_slot[i] = NULL;
        s_tcal_prog = NULL;
        s_tcal_dot  = NULL;
        s_tcal_n = 0;
        s_tcal_seen = 0;
        ESP_LOGW(TAG, "tcal: 自检层已关闭");
        return;
    }
    if (s_tcal_root) {                   /* 已经开着 ⇒ 先删掉重建（幂等，可反复调）*/
        lv_obj_delete(s_tcal_root);
        s_tcal_root = NULL;
    }
    for (int i = 0; i < TCAL_SLOT; i++) s_tcal_slot[i] = NULL;
    s_tcal_prog = NULL;
    s_tcal_dot  = NULL;
    s_tcal_n = 0;
    s_tcal_seen = 0;

    /* ★ 整屏一个可点对象：只要触点落在屏内，事件一定到它手上。
     *   （若按下后【一条记录都没有】，说明坐标已被算到屏外 —— 那本身
     *     就是"档位严重不匹配"的强信号，不是"没反应"。）*/
    lv_obj_t *r = lv_obj_create(lv_layer_top());
    s_tcal_root = r;
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(r, 0, 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x0B0E12), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    /* ★ PRESSED = 按下那一刻（记一行读数）；PRESSING = 按住期间的每次采样
     *   （只挪红点）。两个都必须接 —— 只接 PRESSED 的话，
     *   按住拖动时红点不动，就分不清"拖不动"和"触摸只有单击有效"。*/
    lv_obj_add_event_cb(r, tcal_pressed_cb, LV_EVENT_PRESSED,  NULL);
    lv_obj_add_event_cb(r, tcal_pressed_cb, LV_EVENT_PRESSING, NULL);

    /* ★★ 跟手红点（10-10）：屏上唯一一个会随手移动的东西。
     *   兰兰看它就能判断"触摸到底准不准、跟不跟手"，不用再看数字。*/
    s_tcal_dot = lv_obj_create(r);
    lv_obj_remove_style_all(s_tcal_dot);
    lv_obj_set_size(s_tcal_dot, 10, 10);
    lv_obj_set_style_bg_color(s_tcal_dot, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_bg_opa(s_tcal_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_tcal_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(s_tcal_dot, LV_OBJ_FLAG_CLICKABLE);   /* 别抢事件 */
    lv_obj_set_pos(s_tcal_dot, -20, -20);                    /* 先藏在屏外 */

    lv_obj_t *t = lv_label_create(r);
    lv_obj_set_style_text_font(t, F_TINY, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0x5EEAD4), 0);
    lv_label_set_text(t, "按住划动看红点  或依次点 1-9");
    lv_obj_set_pos(t, 6, 31);          /* 让开第一行靶标（它们 y 到 29）*/

    /* ★★ 靶标 = 9 颗 3×3，取满全量程且格点均匀。
     *   布局（块 22×22，编号写在块内居中）：
     *     x 中心 20 / 160 / 300   （块 9..31 / 149..171 / 289..311）
     *     y 中心 18 / 112 / 206   （块 7..29 / 101..123 / 195..217）
     *   为什么不是四角 5 颗（上一版）：
     *     5 颗只有 2 个 x 值 ⇒ **分不出"屏 y 还随 x 变"**（上一轮同一条靶标线
     *     上 A 读到 18、B 读到 0，差 18 px）。3×3 才能拟合含倾斜项的二元线性。*/
    static const int MK[TCAL_MAX][2] = {
        { 20,  18}, {160,  18}, {300,  18},
        { 20, 112}, {160, 112}, {300, 112},
        { 20, 206}, {160, 206}, {300, 206},
    };
    for (int i = 0; i < TCAL_MAX; i++) {
        lv_obj_t *m = lv_obj_create(r);
        lv_obj_remove_style_all(m);
        lv_obj_set_size(m, 22, 22);
        lv_obj_set_pos(m, MK[i][0] - 11, MK[i][1] - 11);
        lv_obj_set_style_bg_color(m, lv_color_hex(0xFACC15), 0);
        lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(m, 3, 0);
        lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);  /* 让事件落到整屏那张 */

        lv_obj_t *n = lv_label_create(m);   /* ★ 编号是【块的子对象】⇒ 居中即块心 */
        lv_obj_set_style_text_font(n, F_TINY, 0);
        lv_obj_set_style_text_color(n, lv_color_hex(0x2A2000), 0);
        lv_label_set_text_fmt(n, "%d", i + 1);
        lv_obj_center(n);
    }

    /* 读数区：3 行 × 15 px，摆在第一行与第二行靶标之间的空档。
     * F_TINY(xs_font_14) 的 line_height 正好 15 ⇒ 46 + 3×15 = 91 < 101（第二行靶标）。*/
    for (int i = 0; i < TCAL_SLOT; i++) {
        lv_obj_t *s = lv_label_create(r);
        lv_obj_set_style_text_font(s, F_TINY, 0);
        lv_obj_set_style_text_color(s, lv_color_hex(0xE5E7EB), 0);
        lv_label_set_text(s, "--");
        lv_obj_set_pos(s, 76, 46 + i * 15);
        s_tcal_slot[i] = s;
    }

    /* 进度行：第三行靶标到 y=217 ⇒ 218 起放得下一行 15 px（到 233 ≤ 240）。*/
    s_tcal_prog = lv_label_create(r);
    lv_obj_set_style_text_font(s_tcal_prog, F_TINY, 0);
    lv_obj_set_style_text_color(s_tcal_prog, lv_color_hex(0xFACC15), 0);
    lv_label_set_text_fmt(s_tcal_prog, "已按 0/%d", TCAL_MAX);
    lv_obj_set_pos(s_tcal_prog, 6, 218);

    ESP_LOGW(TAG, "tcal: 自检层已开（1..9 共 %d 颗靶标）", TCAL_MAX);
}
