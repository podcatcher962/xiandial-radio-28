/*
 * es28 Suite —— 显示 / 触摸 驱动层（LCDWiki ES3C28P · 2.8″）
 *
 * 屏：ILI9341V，4-line SPI，面板原生 240x320 竖屏，**逻辑按横屏 320x240 用**。
 *     ★ 横屏靠 MADCTL 的 MV 位（swap_xy=1）实现，见 app_pins.h 的 LCD_SWAP_XY：
 *       swap=0 会把 320 宽的画布塞进 240 的扫描方向 ⇒ 斜纹花屏。
 *     ★ 镜像值见 app_pins.h 的 LCD_MIRROR_X/Y（横屏定档中）。第三方 TFT_eSPI
 *       的 MIRROR_X 与 esp_lcd 的 MADCTL 位语义无关，别互相推。
 * 触摸：FT6336G（FT5x06 同族），I2C，与 ES8311【共用同一条 I2C 总线】。
 *     ★ 量程填【原生】240x320（TP_NATIVE_*），不是逻辑尺寸，否则软件镜像越界。
 *
 * 复用说明：结构与拾声 lingqian-radio/main/app_display.c 同源，但屏协议、
 * 分辨率、引脚面全不同 ⇒ BSP 各写一套（见 多产品共存方案.md 第四节）。
 */
#include <string.h>

#include "app_display.h"
#include "app_pins.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "ES28DISP";

/* ---------- 背光 (LEDC PWM) ---------- */
#define BL_LEDC_TIMER    LEDC_TIMER_0
#define BL_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BL_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define BL_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define BL_FREQ_HZ       5000

static esp_lcd_panel_io_handle_t s_io       = NULL;
static esp_lcd_panel_handle_t    s_panel    = NULL;
static lv_display_t             *s_disp     = NULL;
static i2c_master_bus_handle_t   s_i2c_bus  = NULL;
static esp_lcd_touch_handle_t    s_touch    = NULL;
static lv_indev_t               *s_indev    = NULL;

/* ---------- 背光 ---------- */
static void backlight_init(void)
{
    ledc_timer_config_t tcfg = {
        .speed_mode      = BL_LEDC_MODE,
        .duty_resolution = BL_LEDC_DUTY_RES,
        .timer_num       = BL_LEDC_TIMER,
        .freq_hz         = BL_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    ledc_channel_config_t ccfg = {
        .gpio_num   = LCD_PIN_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel    = BL_LEDC_CHANNEL,
        .timer_sel  = BL_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
}

void app_backlight_set(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    uint32_t max  = (1u << BL_LEDC_DUTY_RES) - 1u;
    uint32_t duty = max * (uint32_t)percent / 100u;
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}

i2c_master_bus_handle_t app_i2c_bus_get(void) { return s_i2c_bus; }
lv_display_t *app_display_get(void)           { return s_disp; }
esp_lcd_touch_handle_t app_touch_get(void)    { return s_touch; }

/* ★★ 10-10 2.8 适配：见 app_display.h 的说明（退化实现，保住调用方）。 */
void app_touch_get_last(int *rx, int *ry, int *lx, int *ly, bool *pressed)
{
    lv_point_t p = { 0, 0 };
    if (s_indev) {
        lv_indev_get_point(s_indev, &p);
    }
    if (rx)      *rx      = (int)p.x;
    if (ry)      *ry      = (int)p.y;
    if (lx)      *lx      = (int)p.x;
    if (ly)      *ly      = (int)p.y;
    if (pressed) *pressed = false;
}

/* ---------- 朝向（镜像）----------
 * ★★★ 10-10 真机踩坑：本来是只在 lcd_init() 里调一次
 *     esp_lcd_panel_mirror(s_panel, LCD_MIRROR_X, false)，
 *     结果屏上文字【左右反】。读 esp_lvgl_port 源码找到真因：
 *       esp_lvgl_port_disp.c : lvgl_port_disp_rotation_update()
 *         case LV_DISPLAY_ROTATION_0:
 *             esp_lcd_panel_swap_xy(c, rotation.swap_xy);
 *             esp_lcd_panel_mirror(c, rotation.mirror_x, rotation.mirror_y);
 *     它在 add_disp 时会【无条件执行一次】这两句。而我把结构体里的
 *     mirror_x 传成了 false ⇒ 等于把刚设好的 MX 位又抹掉，
 *     实际生效的 MADCTL 退回 0x08（只剩 BGR 位）⇒ 左右反。
 *   ⇒ 教训与拾声「MADCTL 不可靠就别交给它」同一类：
 *     【先搞清谁是最后写入者】，再决定在哪一层设。这里最后写入者是 lvgl_port。
 *     （lcd_init 里那次调用保留无妨，反正会被正确地重设一遍。） */
static int s_swp   = LCD_SWAP_XY;
static int s_mir_x = LCD_MIRROR_X;
static int s_mir_y = LCD_MIRROR_Y;

/* ★★★ 10-10 转【横屏 320x240】新增：朝向一共 3 位（swap / mx / my）= 8 档。
 *
 *  ★ 为什么把【显示】与【触摸】拆成两个独立入口：
 *    它俩是【两个独立的物理量】—— 显示侧由面板排线/扫描方向决定，
 *    触摸侧由触摸屏贴合方向决定，**谁也推不出谁**（铁律 86 的教训：
 *    竖屏定案时它俩的值就不一样 —— 屏 mx=1 字正，而触摸 mx=0 才点哪是哪）。
 *    ⇒ 必须能分别枚举。只要有一条"必须同步"的假设，就会像铁律 85 那样
 *      把唯一正确的那组配置挡在门外。
 *
 *  ★ 为什么做成运行时可切：横屏的 MADCTL 组合我一次推不出来（竖屏那次也推错了）。
 *    做成 8 档让眼睛定档，一次烧录试完 —— 见铁律 84：
 *    「配置写了 ≠ 生效了，现象没变时改成把候选态全列出来让设备自证」。 */
void app_display_set_orient_disp(bool swap, bool mx, bool my)
{
    s_swp   = swap ? 1 : 0;
    s_mir_x = mx ? 1 : 0;
    s_mir_y = my ? 1 : 0;
    if (s_panel) {
        esp_lcd_panel_swap_xy(s_panel, swap);
        esp_lcd_panel_mirror(s_panel, mx, my);
    }
    ESP_LOGW(TAG, "★ 显示朝向 = swap=%d mx=%d my=%d", (int)swap, (int)mx, (int)my);
}

void app_display_set_orient_touch(bool swap, bool mx, bool my)
{
    /* ⚠️ 10-10 之后**正常产品路径不要调它**：
     *   触摸的真实映射已经在 tp_process_xy() 里做完（含比例），
     *   这里再开 flags 那三位 = 在上面【叠加】一次翻转/交换，
     *   结果是"两次变换"，越调越乱。
     *   保留它只为调试对比（串口 `tmap`），默认三位全 0。*/
    if (s_touch) {
        esp_lcd_touch_set_swap_xy(s_touch, swap);
        esp_lcd_touch_set_mirror_x(s_touch, mx);
        esp_lcd_touch_set_mirror_y(s_touch, my);
    }
    ESP_LOGW(TAG, "★ 触摸朝向 = swap=%d mx=%d my=%d", (int)swap, (int)mx, (int)my);
}

/* 保留的老入口：显示 + 触摸一起翻（swap 取编译期默认）。
 * ⚠️ 只在"确定两者同档"时才用；横屏定档阶段请用上面两个独立入口。 */
void app_display_set_mirror(bool mx, bool my)
{
    app_display_set_orient_disp((LCD_SWAP_XY != 0), mx, my);
    app_display_set_orient_touch((LCD_SWAP_XY != 0), mx, my);
}

void app_display_get_mirror(bool *mx, bool *my)
{
    if (mx) *mx = (s_mir_x != 0);
    if (my) *my = (s_mir_y != 0);
}

/* ---------- 截图（调试用，供串口把屏上画面取回 PC）----------
 * ★ 实现方式与拾声 3.5 不同，是【被结构逼出来的】：
 *   3.5 那边 LVGL 的帧缓冲是本工程自己 malloc 的（s_buf1），拷它就行；
 *   2.8 走 lvgl_port 托管，缓冲由它内部按 buffer_size 分配、指针不外露
 *   ⇒ 只能问 LVGL 要"当前活动缓冲"（lv_display_get_buf_active, LVGL 9.6）。
 *   ⚠️ 为什么不开 CONFIG_LV_USE_SNAPSHOT 走 lv_snapshot_take()：
 *     那是"把对象树重新渲染成一张图"，每次截图会重绘一整帧；
 *     直接拷现成的活动缓冲是零渲染开销，调试口更该轻。
 *   ⚠️ stride 不能假设等于 w*2 —— 拷贝要【逐行按 stride 走】，
 *     否者遇到带对齐的缓冲会出斜纹（这种图最容易让人误判成 UI 画错）。 */
bool app_display_snapshot(void *dst)
{
    if (!s_disp || !dst) return false;
    lv_draw_buf_t *b = lv_display_get_buf_active(s_disp);
    if (!b || !b->data) return false;

    uint32_t w = b->header.w, h = b->header.h;
    /* ★ 10-10 加：调试口的第一眼判据。
     *   缓冲若不是整屏（比如部分缓冲 320×40），截图就只会顶上一条是真的、
     *   下面全是垃圾 —— 那图看起来和"花屏"一模一样，极易误判成屏坏了。
     *   全屏配置下这行应打印 320x240；一旦又变回 320x40，就是 buffer_size 被人动过。*/
    static bool s_snap_logged = false;
    if (!s_snap_logged) {
        s_snap_logged = true;
        ESP_LOGW(TAG, "snapshot 缓冲 = %ux%u (应 = %dx%d)%s",
                 (unsigned)w, (unsigned)h, LCD_H_RES, LCD_V_RES,
                 (w == (uint32_t)LCD_H_RES && h == (uint32_t)LCD_V_RES)
                     ? "" : "  ★★ 不是整屏！截图会只有顶上一条是真的");
    }
    if (h > (uint32_t)LCD_V_RES) h = LCD_V_RES;   /* 防御：绝不越界读缓冲 */
    if (w > (uint32_t)LCD_H_RES) w = LCD_H_RES;
    /* ⚠️ LVGL 9.6 的 lv_draw_buf_t【没有 stride 字段】（只有 header/data/
     *    data_size/unaligned_data/handlers）⇒ 必须用官方函数把宽度换算成
     *    行字节数，别自己写 w*2 —— 那在带对齐的缓冲上会出斜纹。 */
    uint32_t stride = lv_draw_buf_width_to_stride(w, b->header.cf);
    size_t   row    = (size_t)w * 2u;                 /* RGB565 */
    const uint8_t *src = (const uint8_t *)b->data;
    uint8_t       *out = (uint8_t *)dst;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(out + (size_t)y * row, src + (size_t)y * stride, row);
    }
    return true;
}

/* ---------- I2C 总线（触摸 + ES8311 共用）---------- */
static esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source              = I2C_CLK_SRC_DEFAULT,
        .i2c_port                = I2C_NUM_0,
        .scl_io_num              = TP_PIN_SCL,
        .sda_io_num              = TP_PIN_SDA,
        .glitch_ignore_cnt       = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_i2c_bus);
}

/* ---------- SPI 屏 ---------- */
static esp_err_t lcd_init(void)
{
    /* 1. SPI 总线 */
    spi_bus_config_t buscfg = {
        .sclk_io_num     = LCD_PIN_SCK,
        .mosi_io_num     = LCD_PIN_MOSI,
        .miso_io_num     = LCD_PIN_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        /* ★ 10-11：与 80 行部分缓冲配套（25,600 B < 51,200 ⇒ 单次传输、不分块）。
         *   ⚠️ 试过把它改成整屏 153,600 —— 并不解卡死问题，已回退（见 disp_cfg 注释）。 */
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "spi bus");

    /* 2. panel IO（SPI） */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = LCD_PIN_DC,
        .cs_gpio_num       = LCD_PIN_CS,
        .pclk_hz           = LCD_SPI_CLK_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(
                            (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io),
                        TAG, "panel io");

    /* 3. 面板 */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,               /* -1：本板无独立屏复位脚 */
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(s_io, &panel_cfg, &s_panel),
                        TAG, "panel ili9341");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");  /* rst=-1 ⇒ 空操作 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    /* ★★★ 10-10 真机定案：必须 invert = true（原为 false，被错的注释误导）。
     *   现象取证（技能「坑 2」的标准手法）：真机照片 vs 帧缓冲截图逐色比对 ——
     *     深底 0x0B0E12 ↔ 近白底、白字 0xF2F5F8 ↔ 近黑字、
     *     品牌绿 0x34D399 ↔ 品红 0xCB2C66、琥珀 0xFBBF24 ↔ 蓝 0x0440DB，
     *     **每条都恰好是它的补色** ⇒ 只可能是面板 INVON/INVOFF 状态反了。
     *   ⚠️ 为什么以前没发现：截图（app_display_snapshot）读的是 **LVGL 帧缓冲**，
     *     面板反色不在缓冲里体现 ⇒ 「截图永远是对的、屏上永远是补色」这个分裂
     *     一直没人对上号（兰兰 10-10 报「你截图是深色，我看屏上是白背景」才暴露）。
     *   ⚠️ 原注释「第三方实测 INVERT_COLOR=false ⇒ 不反色」不可信：厂家 config 是套模板的，
     *     未必与实际出货板一致；**以真机实测为准**（同 3.5″ 板的结论）。*/
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "panel invert");
    /* ★ 初始朝向：与 app_pins.h 一致（运行时可用 app_display_set_orient_*() 改）。
     * ⚠️ lvgl_port 在 add_disp 时会用 disp_cfg.rotation 再设一遍 ⇒ 这里只是
     *    让"add_disp 之前"的短暂画面也是对的；真正的最后写入者是 lvgl_port。 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, LCD_SWAP_XY), TAG, "panel swap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, LCD_MIRROR_X, LCD_MIRROR_Y), TAG, "panel mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "panel on");

    ESP_LOGI(TAG, "panel ready %dx%d (ILI9341 4-line SPI, panel native %dx%d, swap=%d mx=%d my=%d)",
             LCD_H_RES, LCD_V_RES, LCD_PANEL_W, LCD_PANEL_H,
             LCD_SWAP_XY, LCD_MIRROR_X, LCD_MIRROR_Y);
    return ESP_OK;
}

/* ---------- 触摸 ---------- */
/* ============================================================
 *  触摸坐标映射（★ 10-10 真机标定，**取代**框架自带的 swap/mirror）
 * ============================================================
 *  ★ 为什么框架的 flags 不够用：
 *    esp_lcd_touch 的软件变换只有【mirror + swap】，**做不出比例**：
 *      if (mirror_x) x = x_max - x;   // 只有"翻转"，没有"缩放"
 *      if (mirror_y) y = y_max - y;
 *      if (swap_xy)  { x <-> y; }
 *    而这块屏实测两个方向的响应【不一样】（tcal 自检层量出来的）：
 *      · 控制器第 2 分量 ry → 屏 x ：1:1        （斜率 0.982）
 *      · 控制器第 1 分量 rx → 屏 y ：**0.733**  —— rx 的响应跨度只有
 *        y 向 240 像素的 73%，光靠"翻转"永远凑不出来。
 *    ⇒ 把 flags 那三位全关，改用 process_coordinates 自己映射。
 *
 *  ★ 时机（这一点必须清楚，否则会双重变换）：
 *    esp_lcd_touch.c : esp_lcd_touch_get_coordinates() 的顺序是
 *      ① tp->get_xy()                  ← 控制器原始值
 *      ② process_coordinates()         ← **本回调，拿到的是原始值**
 *      ③ mirror_x / mirror_y / swap_xy ← 框架的三步（已全关）
 *    所以这里同时负责方向（轴交换/翻转）和比例（缩放），一步到位。
 *
 *  ★ 参数怎么来的（别凭手感改）：
 *    串口 `tcal` 在屏上画 5 个已知位置的黄块 → 依次按 → 板子把
 *    LVGL 实际收到的坐标打出来 → 拿「手指实际按的位置 ↔ 控制器原始值」
 *    做最小二乘拟合。三组 y 值的残差 < 2 px。要重标定就再跑一遍 tcal。
 * ============================================================ */
/* ★★★ 10-10 第三轮（3×3 九点靶标 · 反解原始值后最小二乘）★★★
 *
 *   经历（每轮都是真机数据，别凭手感改）：
 *     · 第一轮 5 点、y 只到 128 ⇒ 靠外推，Y 斜率 -1.364 偏大，
 *       真机把 rx=196.85 映到 0.0、rx=21.63 映到 239.0 —— **正好压在屏的两条
 *       边界上被钳位**，回代反解时把钳位值当真实值，越修越偏。
 *     · 第二轮改 3×3 九点、取满全量程，两端不再一起钳位 ⇒ 才拿到干净比例。
 *
 *   ★★ 判据（每次换系数前必须先看）：**读数里出现 0 或 239 就是钳位**，
 *      那一行不能进拟合。第二轮 9 点里只有第 2、3 点（右上/右中）的 y=0 是钳位，
 *      其余都是真值。
 *   ★★ 另一件事：**明显离群的单个点要剔除**——第二轮第 9 点（右下）反解出的
 *      ry=298.5，而同列另两点是 314.6/316.6，差 16 px，只能是指尖按偏
 *      （3×3 靶标就这点好处：**同列三点互为参照**，离群一眼能看出来）。
 *
 *   拟合结果（剔除钳位与离群点后，对原始值做最小二乘）：
 *     X：残差 ≤ 3.4 px（屏 x ← raw 第 2 分量，正向）
 *     Y：残差 ≤ 8.1 px（屏 y ← raw 第 1 分量，反向）
 *   倾斜项量出来很小（X 向 ≈ 0、Y 向 0.018），**先不加**，保持轴对齐好维护；
 *   若兰兰仍报"上下偏"，再把 Y 的倾斜项补上。*/
#define TP_KX_A    0.9611f   /* 屏 x = A * ry + B（正向）*/
#define TP_KX_B   -3.35f
#define TP_KY_A   -1.1009f   /* 屏 y = A * rx + B（反向：rx 越大 y 越小）*/
#define TP_KY_B    243.35f

static void tp_process_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                          uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    (void)tp; (void)strength; (void)max_point_num;
    for (int i = 0; i < (int)*point_num; i++) {
        float rx = (float)x[i];              /* 控制器原始第 1 分量 */
        float ry = (float)y[i];              /* 控制器原始第 2 分量 */
        float sx = TP_KX_A * ry + TP_KX_B;
        float sy = TP_KY_A * rx + TP_KY_B;
        /* 夹到屏内：标定参数是拟合出来的，边缘可能算出 -3 或 322 */
        if (sx < 0.0f)                    sx = 0.0f;
        if (sx > (float)(LCD_H_RES - 1))  sx = (float)(LCD_H_RES - 1);
        if (sy < 0.0f)                    sy = 0.0f;
        if (sy > (float)(LCD_V_RES - 1))  sy = (float)(LCD_V_RES - 1);
        x[i] = (uint16_t)(sx + 0.5f);
        y[i] = (uint16_t)(sy + 0.5f);
    }
}

static esp_err_t touch_init(void)
{
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_cfg.scl_speed_hz = TP_I2C_FREQ_HZ;

    esp_lcd_panel_io_handle_t tp_io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io),
                        TAG, "touch io");

    esp_lcd_touch_config_t tp_cfg = {
        /* 量程填【控制器原始值的量程】。10-10 之后它只被框架那三步
         * （已全关）里的 `x_max - v` 用到 ⇒ 现在是纯声明；
         * 但必须留着：万一以后要退回框架 flags 路线，它得是原始量程，
         * 填逻辑尺寸会像 10-10 那样越界（见 app_pins.h 的 TP_NATIVE_W 注释）。*/
        .x_max        = TP_NATIVE_W,
        .y_max        = TP_NATIVE_H,
        .rst_gpio_num = TP_PIN_RST,
        .int_gpio_num = TP_PIN_INT,
        .levels       = { .reset = 0, .interrupt = 0 },
        /* ★★★ 10-10：flags 三位【全关】—— 方向与比例都交给 process_coordinates。
         *   别在这里开任何一位：开了就是"变换两次"，而二次变换比原始问题更难查
         *   （第一次的错被第二次盖住，看起来像"参数不对"，会让人去调参数）。*/
        .flags        = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .process_coordinates = tp_process_xy,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &s_touch),
                        TAG, "touch ft5x06");
    ESP_LOGI(TAG, "touch ready (FT5x06 map, addr=0x%02X, native=%dx%d, swap=%d mirror_x=%d mirror_y=%d)",
             TP_I2C_ADDR, TP_NATIVE_W, TP_NATIVE_H, TP_SWAP_XY, TP_MIRROR_X, TP_MIRROR_Y);
    return ESP_OK;
}

/* ---------- 全套初始化 ---------- */
esp_err_t app_display_init(void)
{
    /* 背光先建但保持 0，等一切就绪再点屏（防上电闪白） */
    backlight_init();
    app_backlight_set(0);

    ESP_RETURN_ON_ERROR(i2c_bus_init(), TAG, "i2c bus");
    ESP_RETURN_ON_ERROR(lcd_init(), TAG, "lcd init");
    ESP_RETURN_ON_ERROR(touch_init(), TAG, "touch init");

    /* LVGL 端口 */
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority  = 4;
    port_cfg.task_stack     = 8192;
    port_cfg.task_affinity  = 0;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port");

    /* ★★★★ 10-11 【真·卡死根因，实测确认】缓冲【不能】用整屏 320×240，必须部分缓冲。
     *
     *   症状：开机 UI built 后约 5 s 整体卡死（首帧 WDT 在 7396 ms）；看门狗每 5 s
     *     一次、IDLE0 饿死、taskLVGL 在 CPU0 死转；addr2line 卡在
     *     lvgl_port_task → lv_timer_handler → lv_display_refr_timer → refr_area
     *     → wait_for_flushing (lv_refr.c:1500 `while(disp->flushing);`)。
     *   取证（10-11 在 esp_lvgl_port 里插计数器，直接观测被测对象）：
     *     `FLUSH#1 area=(0,0)-(319,239) 320x240 bytes=153600 | READY累计=0`
     *   ⇒ 第一帧那一次 SPI 传输的【完成中断回调 on_color_trans_done 从未到达】，
     *     而 esp_lcd_panel_draw_bitmap 返回 ESP_OK（不是没排上队）。
     *     `flushing` 是 0/1 标志（lv_refr.c:1422），一次不来就永远为 1 ⇒ 忙等。
     *
     *   已实测排除的两个"看着像"的嫌疑（改了都没用）：
     *     ① full_refresh —— 去掉后卡死时间几乎不变（7396 ms vs 7410 ms）。
     *     ② max_transfer_sz=51200 把整屏拆成多块 DMA —— 改成整屏 153600 后
     *       【依然】卡在同一处 ⇒ 与"分块"无关。
     *   对照：老的「320×40 部分缓冲 ×2」一次 flush 只有 25,600 B，能跑
     *     （10-10 兰兰看到"频谱在动"就是那一版）。
     *   ⇒ 结论：本板 + 本 IDF + esp_lcd SPI 这条路上，【单次 flush 字节数太大
     *     （153,600）就收不到完成中断】。故退回部分缓冲。
     *
     *   ⚠️ 副作用：app_display_snapshot() 读 lv_display_get_buf_active() 只能拿到
     *     40 行 ⇒ 截图只有顶上一条（不是屏坏了）。要整帧截图另开
     *     CONFIG_LV_USE_SNAPSHOT 走 lv_snapshot_take()（代价：lvgl 全量重编 ~32 min）。
     *   详见 .workbuddy/memory/2026-10-11.md */
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = s_io,
        .panel_handle  = s_panel,
        .buffer_size   = LCD_H_RES * 40,          /* ★ 部分缓冲：一次 flush 仅 25,600 B */
        .double_buffer = true,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .monochrome    = false,
        /* ★★★ 镜像【必须写在这里】（不能只写在 lcd_init 里）——
         *   见上面 app_display_set_mirror() 的注释：lvgl_port 在 ROTATION_0
         *   时会用本结构体的值【重设】面板镜像，传 false 就会把 MX 抹掉。 */
        .rotation      = { .swap_xy  = (LCD_SWAP_XY != 0),
                           .mirror_x = (LCD_MIRROR_X != 0),
                           .mirror_y = (LCD_MIRROR_Y != 0) },
        /* ★ 10-11：full_refresh 已去掉（实测与本卡死无关，见上面 disp_cfg 注释）。
         *   ⚠️ SPI 屏在 lvgl_port 里被标成 LVGL_PORT_DISP_TYPE_OTHER ⇒
         *   lvgl_port_flush_callback() 末尾那个
         *     `if (RGB || (DSI && (direct_mode||full_refresh))) lv_disp_flush_ready()`
         *   【不会进入】⇒ 刷新完成只能靠 ISR 回调 on_color_trans_done。
         *   所以【只要那次回调不来，LVGL 就死在 wait_for_flushing 里】。 */
        .flags         = { .buff_dma = true, .buff_spiram = true, .swap_bytes = true },
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(s_disp != NULL, ESP_FAIL, TAG, "lvgl add disp");

    lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = s_disp,
        .handle = s_touch,
    };
    s_indev = lvgl_port_add_touch(&touch_cfg);
    ESP_RETURN_ON_FALSE(s_indev != NULL, ESP_FAIL, TAG, "lvgl add touch");

    /* ★★★ 10-10 真机修：强制【轮询】模式，别用中断事件模式。
     *
     *   lvgl_port_add_touch() 里有一句：
     *     if (handle->config.int_gpio_num != GPIO_NUM_NC) lv_indev_set_mode(EVENT);
     *   本板 TP_PIN_INT=17（接了）⇒ 它默认给的是 LV_INDEV_MODE_EVENT。
     *   EVENT 模式下 LVGL **只在 INT 脚电平跳变时读一次点**。而手指按住不动时
     *   INT 一直停在同一个电平 ⇒ 没有新的跳变 ⇒ 不再采样。
     *
     *   实测表现（兰兰 10-10 报的「点击会延迟响应 / 不灵敏 / 点不准」）：
     *     · 按一下要等一下才响应（在等下一次中断沿）；
     *     · 按住拖动基本失灵，必须抬手再按。
     *
     *   ⇒ 改成 LV_INDEV_MODE_TIMER：由 LVGL 定时器按默认 30 ms 读一次。
     *     320×240 这么小的屏，30 ms 轮询的开销可以忽略，换来的是"按哪读哪"。
     *   ⚠️ 必须在 LVGL 锁内改（lv_indev_set_mode 会碰 indev 内部状态）。 */
    if (lvgl_port_lock(0)) {
        lv_indev_set_mode(s_indev, LV_INDEV_MODE_TIMER);
        lvgl_port_unlock();
    }
    ESP_LOGW(TAG, "★ 触摸采样 = 轮询 TIMER(30ms)（lvgl_port 因有 INT 脚本来会给 EVENT）");

    ESP_LOGI(TAG, "display ready: %dx%d (横屏 320x240, SPI 40MHz)", LCD_H_RES, LCD_V_RES);

    app_backlight_set(85);
    return ESP_OK;
}
