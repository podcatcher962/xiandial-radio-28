#pragma once

#include "lvgl.h"

/* 启动「拾声」界面（开机 splash → 首页） */
void ui_init(void);

/* 刷新「城市声音集 · 本地音频」页（重新读一遍 SD 卡目录）。
 * 从首页卡片进入该页时会自动调用一次；外部改了卡内容也可手动调。*/
void ui_sd_page_refresh(void);

/* ★ 10-06：台单热重载后重建网格（由 app_stlist.c 的 st_reload 命令调用）*/
void ui_reload_stations(void);

/* ★ 10-10 调试用（串口 `page <n>` / `dump <n>`）：
 *   ui_debug_goto —— 切到第 n 页（0 首页 1 发现 2 播放 3 夜间 4 状态
 *                    5 本地音频 6 全屏列表 7 配网）。
 *   ui_debug_dump —— 打印该页所有「右下角超出内容区」的对象，传 -1 = 当前页。
 *   仅供真机迭代排查，不参与产品逻辑。*/
void ui_debug_goto(int idx);
void ui_debug_dump(int idx);

/* ★ 10-10 调试用（串口 `tcal` / `tcal 0`）：触摸自检层。
 *   on=1 在 lv_layer_top() 上盖一张全屏自检页（5 个已知位置的黄块 A..E
 *          + 下半屏 6 行读数），每按一次就把 LVGL 收到的坐标写进下一行；
 *   on=0 拆掉它。
 *   用途：本板触摸的 swap/mirror 三档是【实测定档】的（框架的软件变换
 *   顺序是"先镜像后交换"，照显示侧抄必然错），拿它一次读出映射关系。*/
void ui_debug_tcal(int on);
