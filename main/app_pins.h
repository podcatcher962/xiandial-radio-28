/*
 * es28 Suite —— 引脚分配（LCDWiki ES3C28P / 2.8″ 240x320）
 *
 * ★★★ 唯一真源：ESP32-Platform/ES3C28P-熟悉笔记.md 第三节（含一手材料出处）。
 *     ⚠️ 这块板与拾声 3.5″【大量撞号但功能完全不同】——
 *        IO38/39 这里=SD D0/D1（拾声是触摸 I2C）、IO41=SD CMD（拾声是背光）、
 *        IO42=RGB 灯（拾声是屏 DC）、IO45/46=背光/DC（拾声是自由脚）。
 *        ⇒ 改本文件前先确认自己在哪块板上（详见笔记第四节）。
 *
 * ⛔ 绝对不能用：IO33~IO37（8MB OPI PSRAM 占用）、IO26~IO32（Flash 未引出）。
 *
 * ★★★ 命名约定（10-10 定，为了「跨板复用」）：
 *   屏幕相关用 LCD_PIN_*（两块板的屏协议本来就不同，必须各写一套）；
 *   音频 / SD / 电池 / 灯 这些【芯片相同、只是脚不同】的部分，
 *   一律沿用拾声 lingqian-radio 的宏名（PIN_I2S_* / PIN_PA_EN / PIN_SD_* …）
 *   ⇒ 拾声的 app_audio.c / app_sd.c 搬过来【一行都不用改】。
 *   这条约定是刻意的：名字对不上，复制模块时就得改代码，改一次就埋一次坑。
 */
#pragma once

/* ★ 10-11：本文件要给出电池 ADC 的【通道】宏（见下面 BAT_ADC_CHAN），
 *   而 ADC_UNIT_x / ADC_CHANNEL_x 由这个头提供。纯声明，无副作用。 */
#include "esp_adc/adc_oneshot.h"

/* ===================== 显示屏（ILI9341V · 4-line SPI）===================== */
#define LCD_HOST            SPI2_HOST       /* S3 上 SPI2 即通用高速口 */
#define LCD_PIN_SCK         12
#define LCD_PIN_MOSI        11
#define LCD_PIN_MISO        13
#define LCD_PIN_CS          10
#define LCD_PIN_DC          46              /* ⚠️ strapping 脚（勿上电拉死） */
#define LCD_PIN_RST         (-1)            /* ★ 本板【没有】独立屏复位脚（与 CHIP_PU 共用）
                                             *   第三方实测 config.h 亦为 GPIO_NUM_NC ⇒ 别去驱动它 */
#define LCD_PIN_BL          45              /* 背光，高=亮；⚠️ 也是 strapping 脚 */

/* ---- 面板物理分辨率（与拾声同名，UI 层共用这两个宏）---- */
#define LCD_PANEL_W         240
#define LCD_PANEL_H         320

/* ---- LVGL 逻辑分辨率 ----
 * ★ 与拾声同一套思路：逻辑 = 面板原生竖屏，flush 里【零旋转】。
 *   拾声是 320x480，这里是 240x320 —— UI 布局按此重排（不是缩放）。*/
#define LCD_H_RES           320
#define LCD_V_RES           240

/* ★★★ 10-10 转【横屏 320x240】新增：交换行列（MADCTL 的 MV 位）。
 *   竖屏（240x320）时它是 0；横屏必须 1 —— 否则 320 宽的画布被塞进
 *   240 的扫描方向里，整屏错位。
 *   ⚠️ 它是【独立于 MX/MY 的第三位】：朝向一共 3 位 = 8 档，
 *      定档阶段由 app_display_set_orient_disp/touch() 运行时切换。*/
#define LCD_SWAP_XY         1

#define LCD_SPI_CLK_HZ      (40 * 1000 * 1000)   /* 第三方实测 40MHz */
#define LCD_SPI_MODE        0

/* ★★★ 镜像（= ILI9341 MADCTL 的 MX/MY 位）
 *
 *   【竖屏定案（已作废，留作锚点）】屏 mx=1 my=0（MADCTL 0x48）字正、触摸 swap0 mx0 my0 点哪是哪。
 *
 *   【横屏（320x240）★★★ 关键规则：MV=1 时，MX/MY 必须"同真同假" ★★★
 *     · MV=1（交换行列）+ MX/MY【全 0】或【全 1】 ⇒ 正常横屏（两者相差 180°）
 *     · MV=1 + MX/MY【只有一个为 1】           ⇒ **镜像画面**（靠 MADCTL 换不出来）
 *     同规则在竖屏段：MV=0 + MX/MY【只有一个为 1】⇒ 正常；全 0 或全 1 ⇒ 镜像。
 *     （自检：竖屏定案 0x48 = MV0+MX1 ⇒ 落在"只有一个为 1"⇒ 正常 ✓ 一致。）
 *
 *   ⚠️⚠️ 10-10 我踩的坑：想当然地"把竖屏那组镜像值照搬到横屏"，得到
 *      swap1+mx1 my0 —— **它恰好是横屏里的镜像支**（正是"只有一个为 1"）。
 *      于是我给兰兰的 A、B 两档**全是镜像**，怎么按都不对（兰兰报"显示镜像"）。
 *      ⇒ 教训：**换 sector（MV 翻转）时，MX/MY 的关系要跟着"取反"，不能照抄。**
 *
 *   当前取【正常横屏 · 甲】：swap=1 mx=0 my=0 （= 标准表 rotation 1 = MADCTL 0x28）
 *   另一支（相差 180°）：swap=1 mx=1 my=1 （= rotation 3 = 0xE8）
 *   ⚠️ swap=0 在横屏是【非法档】：320 宽画布塞进 240 扫描方向 ⇒ 整屏斜纹花屏。 */
#define LCD_MIRROR_X        0
#define LCD_MIRROR_Y        0

/* 竖屏 180° 翻转开关（显示与触摸【必须同时翻】）—— 首版无翻转 */
#define LQ_PORTRAIT_FLIP    0

/* ===================== 触摸（FT6336G · I2C）===================== */
/* ★ 与 ES8311 codec【共用这一条 I2C 总线】（地址不同：FT6336 0x38，ES8311 0x18）
 *   ⇒ 代码里【共享同一个总线句柄】，别各建一个。 */
#define TP_PIN_SDA          16
#define TP_PIN_SCL          15
#define TP_PIN_RST          18
#define TP_PIN_INT          17
#define TP_I2C_ADDR         0x38            /* ⚠️ 真机已确认（日志 touch ready addr=0x38）*/
#define TP_I2C_FREQ_HZ      400000
/* ★★★ 触摸镜像 —— 10-10 真机【定案：与屏【不同】值】
 *   实测：屏在 MODE2（LCD_MIRROR_X=1）时字是正的，但触摸在同一档是【反的】
 *         （手指点左边，红点跑到右边）⇒ 触摸要用 0。
 *
 *   ⚠️ 这条推翻了上一版我立的"显示与触摸必须同档"的编译期断言 —— 那句的前提就错了：
 *        · ILI9341 的 MADCTL MX 位 = 【面板扫描方向】；
 *        · FT6336 的 mirror_x  = 【触摸芯片自己坐标系的方向】。
 *      两者由各自的排线/贴合方向决定，谁大谁小【互相推不出来】，只能真机上看。
 *      ⇒ 已删掉那句断言（错误前提的断言比没有断言更坏：它会把正确解判定为编译错误）。
 *
 *   ⚠️ 但"屏翻触摸也得翻"这件事本身仍成立 —— 只是【增量必须同步】而不是【数值必须相等】。
 *      将来若要转屏（如竖转横），改 LCD_* 时必须同步改 TP_*，并真机复验。
 */
/* ★★★ 触摸映射 —— 10-10 晚【第三次改判】：框架的 swap/mirror 不够用，
 *      真实映射搬到自己写的 tp_process_xy()（见 app_display.c）。
 *
 *   下面这三行现在**已停用**（全 0，只留给日志与调试对比
 *   —— app_display.c 的 touch ready 日志和 `tmap` 命令还引用它们）：
 *
 *   为什么框架那套不够：
 *     esp_lcd_touch 的软件变换只有【mirror + swap】，**做不出比例**：
 *       mirror：x = x_max - x ；y = y_max - y ；swap：x <-> y
 *     而串口 `tcal` 自检层量出来这块屏【两个方向的响应不一样】：
 *       · 控制器第 2 分量 ry → 屏 x ：1:1      （斜率 0.982）
 *       · 控制器第 1 分量 rx → 屏 y ：**0.733**（只有 y 向 240 像素的 73%）
 *     ⇒ 光靠"翻转"永远凑不出 0.733，必须自己线性映射。
 *
 *   ⚠️ 这段保留了两代被推翻的结论，因为它教的是【方法】：
 *     · 第一代"显示与触摸必须同档"——前提就错：ILI9341 的 MADCTL MX 是
 *       面板扫描方向，FT6336 的 mirror_x 是芯片坐标系方向，由各自排线决定，
 *       数值互不相干（铁律 86/94）。
 *     · 第二代"定案 {1,1,1}"—— 只凭"点上去红点能在附近出现"就收工，
 *       **没有量过响应比例**；等界面元素变小（按键 58×22 px）就暴露了：
 *       点右边的键算到左边的键上、点提示音触发扫WiFi。
 *     ⇒ 教训：坐标这类配置**不能只看"像不像"**，要让板子把读数打出来、
 *       对着【已知位置】逐个比对。`tcal` 就是为此做的（铁律 101 同类）。
 *
 *   ⚠️ 将来转屏（如再改 LCD_*）时：要**重新标定 tp_process_xy() 的四个
 *      系数**，而不是来改这三行。 */
#define TP_SWAP_XY          0
#define TP_MIRROR_X         0
#define TP_MIRROR_Y         0

/* ★★★ 触摸控制器的【原生坐标量程】—— 10-10 新增，修一个真 bug ★★★
 *   用途：esp_lcd_touch 的镜像/交换是【软件实现】（FT5x06 驱动没提供
 *         set_mirror_x/set_swap_xy 硬件回调），它换算镜像用的是
 *         `value = x_max - value` —— 所以 **x_max/y_max 必须是控制器的
 *         原生量程，绝不能填逻辑（横屏）尺寸**。
 *   ⚠️ 我转横屏时把 x_max/y_max 跟着 LCD_H_RES 写成了 320/240 ⇒ 镜像一打开就
 *      算出越界坐标（如 240-319 = 负值回绕成 6 万多）⇒ 触点乱跳。
 *      **这不是屏/触摸坏了，是量程填错。**
 *   FT6336 按竖屏贴合报告：X ∈ 0..239、Y ∈ 0..319（= 面板原生 240x320）。*/
#define TP_NATIVE_W         240
#define TP_NATIVE_H         320

/* ===================== 音频（ES8311 codec + FM8002E 功放）=====================
 * ★★ 与拾声【完全同款芯片，连 PA_EN 都是同一个脚号 IO1】，拾声踩过的坑全部有效：
 *    ① PA_EN【低电平使能】；② 开关机防爆音时序；③ 采样率不带猜（回声源拿）。 */
#define PIN_PA_EN           1               /* 功放使能脚 */
#define PA_EN_ACTIVE        0               /* 功放开：输出低 */
#define PA_EN_IDLE          1               /* 功放关：输出高（静音） */
#define PIN_I2S_MCLK        4
#define PIN_I2S_BCLK        5
#define PIN_I2S_WS          7
#define PIN_I2S_DOUT        8               /* ESP32 -> codec（★ 已定死，见笔记第六节） */
#define PIN_I2S_DIN         6               /* codec -> ESP32 */
#define I2C_NUM_CODEC       0               /* 与触摸共用 I2C0 */

/* ===================== SD 卡（SDIO 4-bit）===================== */
#define PIN_SD_CLK          38
#define PIN_SD_CMD          40
#define PIN_SD_D0           39
#define PIN_SD_D1           41
#define PIN_SD_D2           48
#define PIN_SD_D3           47

/* ===================== 其它 ===================== */
#define PIN_BAT_ADC         9               /* 电池电压（ADC1_CH8） */

/* ★★★★ 10-11 【必须由板级定义，别让 app_pwr.c 自己写死】
 *   `app_pwr.c` 是跨板同步文件（`_sync_core.py` 清单第 49 行），
 *   它原来无条件写死 `ADC_CHANNEL_7`（= 3.5″ 板的 IO8）；
 *   而【本板 IO8 是 I2S 的 DOUT】⇒ 电池检测把这个脚配成模拟输入
 *   ⇒ IO8 的 OEN_SEL 变 1、GPIO 输出使能位被清 0 ⇒ **引脚不推线**
 *   ⇒ 整机完全没声音（只剩功放开关的「啪啪」声）。
 *   实测证据：IO8 监视器在 `XSPWR: pwr init done` 之后 100 ms 内抓到状态翻转
 *   （1465 ms 正常 → 1866 ms 变成 OEN_SEL=1/使能位=0）。
 *   ⇒ app_pwr.c 已改为 `#ifndef` 兜底，各板在【本文件】给出自己的通道。
 *   ⚠️ BAT_ADC_UNIT/CHAN 必须与 PIN_BAT_ADC 指向同一个脚（app_pwr.c 里有运行时自检）。*/
#define BAT_ADC_UNIT        ADC_UNIT_1
#define BAT_ADC_CHAN        ADC_CHANNEL_8   /* = IO9（⚠️ 3.5″ 板是 CH7/IO8，两边不同）*/
#define PIN_LED_RGB         42              /* WS2812B 单线 RGB */
#define PIN_BOOT_BTN        0

/* 扩展排针（1.25mm 4P）：IO2 / IO3 / IO14 / IO21
 * ★ 这 4 个是【自由脚】，扩充外设用它们（拾声那块只有 IO45/46 两个）。 */
