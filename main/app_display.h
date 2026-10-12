#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

/* 屏 + 触摸 + LVGL 全套初始化（失败返回错误码） */
esp_err_t app_display_init(void);

/* 背光亮度 0~100 */
void app_backlight_set(int percent);

/* LVGL 显示句柄 */
lv_display_t *app_display_get(void);

/* I2C0 主总线句柄（触摸与 ES8311 共用）—— 供音频模块挂 codec 用 */
i2c_master_bus_handle_t app_i2c_bus_get(void);

/* 触摸句柄（调试用：直接读原始点数/坐标） */
esp_lcd_touch_handle_t app_touch_get(void);

/* ★★ 10-10 2.8 适配：3.5 横版 UI（ui_xiandial.c 的 tap_note）调这个打调试日志。
 *   3.5 是【自建 LVGL indev】⇒ 能把原始/逻辑坐标都存下来；2.8 走 lvgl_port 托管，
 *   拿不到原始读数 ⇒ 这里给退化实现：rx/ry 填逻辑坐标，保证调用方不炸。 */
void app_touch_get_last(int *rx, int *ry, int *lx, int *ly, bool *pressed);

/* ★★ 10-10 调试用：运行时改【显示 + 触摸】的镜像（mx/my 同时作用于两者）。
 *
 * 为什么要有这条通路：
 *   「画面方向对不对」只有人眼能判，而我没有眼睛。
 *   朝向一共 4 种组合，若每换一档都要「改代码 → 编译 → 烧录 → 等人看」，
 *   一轮 3 分钟、4 轮十几分钟，还要人陪着。
 *   改成 BOOT 键一按换一档，用户自己按到看起来正常为止，一次烧录就够了。
 *
 * ★ 显示与触摸【必须同时翻】（拾声那条铁律：只翻一侧 ⇒ 屏是正的、点在对面）。
 *   所以这里一个函数把两边一起改，不提供"只改显示"的口子。*/
/* ★★★ 10-10 转横屏新增：朝向 = 3 位（swap / mx / my）= 8 档，运行时可切。
 *
 *  ★ 显示与触摸【分成两个入口】，不是图省事少写一个参数：
 *    它们是两个【独立物理量】—— 屏侧由面板排线定、触摸侧由贴合方向定，
 *    谁也推不出谁，而且竖屏定档时它们的值本来就不一样
 *    （屏 mx=1 字正／触摸 mx=0 点哪是哪，见铁律 86）。
 *    横屏要重新定档 ⇒ 必须能分别枚举，否则凑不到"字正 + 点哪是哪"。
 *
 *  ★ 用法（朝向自测版）：BOOT 键短按切【显示】档、长按切【触摸】档，
 *    屏上打出当前两个档位；用户按到看起来正常为止。
 *    定档后把值写回 app_pins.h 的 LCD_SWAP_XY / LCD_MIRROR_X / LCD_MIRROR_Y
 *    与 TP_SWAP_XY / TP_MIRROR_X / TP_MIRROR_Y，并把自测版撤掉。 */
void app_display_set_orient_disp(bool swap, bool mx, bool my);
void app_display_set_orient_touch(bool swap, bool mx, bool my);

/* 老入口：显示 + 触摸一起翻（swap 取编译期默认）。
 * ⚠️ 只在"确定两者同档"时用；横屏定档阶段请用上面两个独立入口。 */
void app_display_set_mirror(bool mx, bool my);
void app_display_get_mirror(bool *mx, bool *my);

/* ★★ 截图（调试用）：把当前 LVGL 活动帧缓冲（240x320 RGB565，行主序）
 *   拷到 dst（需 >= LCD_H_RES * LCD_V_RES * 2 = 153600 字节）。
 *   返回 false = 还没出过帧（display ready 之前）或取不到缓冲。
 *
 *   为什么需要它：板子没有硬件 UART，控制台是 USB-Serial-JTAG，
 *   而【屏幕内容只有人眼能看】。有了这条，串口就能把"屏上现在长什么样"
 *   原样取回 PC —— 大量"我看不到屏"的判断题都变成可离线核对的图片。
 *   （拾声 3.5 早就靠这条通道做真机验收，见 app_stlist.c 的 lq_shot。）
 *
 *   ★ 必须持 LVGL 锁（lvgl_port_lock）时调用：拷的是 LVGL 正在用的缓冲，
 *     不持锁会拿到"半新半旧"的一帧。 */
bool app_display_snapshot(void *dst);
