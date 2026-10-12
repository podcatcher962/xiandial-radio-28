# ============================================================
#  es28 Suite —— main 组件（2.8″ · 复现完整拾声集）
#
#  ★★★ 结构（10-10 定）：服务层是【共享内核 · 只有一份真源】
#     真源 = xiandial-hw/firmware/lingqian-radio/main（3.5 工程）
#     同步 = 由 ESP32-Platform/_sync_core.py 复制到本目录（幂等、可 --check）
#     ⇒ 下面列出的服务层 .c/.h 在本目录里是【生成物】，不要手工改；
#       要改服务层就改 3.5 那边，然后跑一次同步脚本（否则下次同步被静默撤销）。
#
#  本目录【手工维护】的只有板相关的这几样：
#     app_pins.h（2.8 引脚） · app_display.c/.h（ILI9341 4-line SPI + FT6336）
#     app_sd.c（2.8 的 SDIO 脚） · es28_main.c · fonts/（字号档位将来按 240x320 重定）
#
#  ⚠️ 为什么不直接「引用」3.5 目录里的源文件（那样连复制都省了）：
#     C 的 #include "xxx.h" 会【先找源文件自己所在目录】。3.5 的 app_sys.c
#     写的是 #include "app_pins.h"，若直接编译 3.5 目录下那个文件，
#     它会用【3.5 的引脚头】—— 在 2.8 上就是一组错误的脚，而且【编得过】，
#     只有真机上才炸。复制过来，才让"当前目录"落在 2.8 这边。
#     这是本结构选择里唯一的技术理由，也是它必须用脚本而不是手工的原因。
#
#  ⚠️ 清单是扫 main/*.c 的 #include 反查出来的，加新文件要照这个路子补，
#     否则报「xxx.c includes xxx.h ... not in the requirements list」——一次只报一个。
# ============================================================

# ⚠️ 这里【不写】共享内核目录的本机绝对路径 —— 发布物里不该出现任何个人路径。
#   （3.5 仓的 main/CMakeLists.txt 同样没有这条；两边口径一致。）
#   服务层真源在拾声集（XianDial Suite）工程里，由 ESP32-Platform/_sync_core.py
#   单向复制过来；日常开发只跟同步脚本打交道，不需要这个变量。

list(APPEND ES28_SRCS
    # ---- 2.8 专属（板相关 / 尺寸相关，各自维护）----
    "es28_main.c"
    "app_display.c"                # ILI9341 4-line SPI + FT6336 触摸
    "app_sd.c"                     # 2.8 版（引脚从 app_pins.h 取；3.5 那份写死了脚）
    "ui_xiandial.c"                # ★★★ 2.8 版拾声完整界面 = 3.5 的【横版】那一套。
                                   #   真源 = 3.5 的 ui_xiandial.c（横屏 480x320，8 个页面），
                                   #   已按 320x240 重排。字号也从 24/16/12 映射到 2.8 的
                                   #   22/18/14（全文件字号引用只有 extern 段与 F_* 宏两处）。
                                   #   ⚠️ 与 3.5 那份是【同名不同物】：改这边别同步回去，
                                   #      也别让 3.5 把它同步过来（两块屏尺寸不同）。
    # ---- 共享服务层（真源在 3.5 工程，由 _sync_core.py 同步）----
    "app_audio.c"                  # ES8311 + FM8002E（与 3.5 同款芯片，只换脚）
    "app_sys.c"                    # WiFi STA 扫描/连接
    "app_prov.c"                   # SoftAP + captive portal 网页配网
    "app_radio.c"                  # 电台核心
    "app_hls.c"                    # m3u8 拉流（HLS 直播）
    "app_fav.c"                    # 收藏
    "app_hist.c"                   # 播放历史
    "app_resume.c"                 # ★ 10-11：本地音频续播进度（按完整路径记"听到第几秒"）
    "app_sleep.c"                  # 睡眠定时
    "app_pwr.c"                    # 电量 + deep sleep 关机
    "app_led.c"                    # RGB 状态灯（当前 XS_LED_ENABLE=0，功能停用）
    "app_settings.c"               # 全局设置（音量/背光/静音，存 NVS）
    "app_voice.c"                  # ★ 10-11：语音（ESP-SR）—— 唤醒词 + 命令词
                                   #   ⚠️ 模型不在这里，在独立 `model` 分区（见 partitions.csv）
    "app_stlist.c"                 # 台单加载 + 访问器（服务层本体）
                                   #   ⚠️ 它同时挂着串口调试命令（lq_shot/ui_demo
                                   #      等）——那些只被 app_st_start_serial_cmd()
                                   #      引用，2.8 不调用 ⇒ 整条链被链接器丢弃，
                                   #      不会因缺 UI 符号而链接失败。
    "lingqian_data.c"              # 灵签签文数据
    "xs_http.c"                    # 明文 HTTP + 迷你 JSON（天气/行情）
    "xs_gbk.c" "xs_gbk_table.c"    # GB2312→UTF-8（行情接口返回 GBK）
    "voice_speak.c"                # 语音播报（预生成原子 + 板上拼接）
    # ---- 中文字库（霞鹜文楷 LXGW WenKai，OFL-1.1）----
    #   ★★ 10-11 复核（读 TTF name 表，不凭注释）：nameID13 = SIL OFL 1.1，
    #      nameID0 = "Copyright 2021-2026 LXGW" ⇒ 允许嵌入固件与子集化。
    #      ⚠️ 它带【保留字体名】(霞鹜/LXGW)，附加许可只覆盖"重编译"与
    #         "网页字体投递"，不含固件嵌入子集 ⇒ 子集版不得用那个名字。
    #         我们的符号名是 xs_font_*（中性）⇒ 天然合规。
    #      许可证文本随仓库分发：lingqian-radio/OFL-1.1-LXGW-WenKai.txt
    #      （已核对与上游 OFL.txt 逐字节一致）。
    #   ★★ 10-11 扩展字符集，10-12 复核后【更正下面这段】：
    #      · xs_font_14 / 18 / 22 —— **保持 2118 个源码字不动**。
    #        10-11 先试过"并进 GB2312 一级（2175 → 4660 字）"，实测把 14px 那档的
    #        line_height 从 **15 顶到 16~17**，而 2.8 的界面是 240px 高逐像素排的
    #        ⇒ 行高 +2px 会动整个布局。**已回滚**，改走字体回退链：
    #          xs_font_14/18/22 .fallback → xs_font_cjk12
    #        主字库由此"小而准"（字号/行高/字宽与改动前逐值相同 = 零布局扰动）。
    #        ⚠️ 早先那段"并入 GB2312 一级"的注释是**当时的计划**，别照它推理现状。
    #      · xs_font_cjk12 → GB2312 一级+二级（符号区 + 6763 汉字）
    #        ⇒ 卡里叫「泸沽湖」「涿州」不再显示方块（地名是二级字密集区）。
    #        （自用版还额外并了台单那 750 字，见下面的 if/else；发布版那份没有）
    #      ⚠️ 但它**不含**两级之外的繁体 —— 台名的繁体由**台名字库**负责
    #        （发布版那份是 GB2312∪cp950，16124 字），两份各管一段。
    #      ⚠️ 22px 不并的理由不变：它只画大标题（就那几个字）。

    #      真要并，改 _gen_fonts_es28.py 的 G.MAIN_GB2312_SIZES 即可。
    "fonts/xs_font_14.c"
    "fonts/xs_font_18.c"
    "fonts/xs_font_22.c"
    # ★★ 10-11 删除 fonts/xs_font_30.c：
    #    它在【2.8 工程里零引用】（grep 0 处、链接 map 0 行 ⇒ 被 --gc-sections
    #    丢掉、一个字节都没进固件），却是个 4.88 MB 的 C 源文件、每次全量
    #    编译都要啃一遍。3.5 工程那份是【活的】（ui_lingqian.c 的 F_HERO
    #    题字/竖排签诗在用）⇒ 只删 2.8 这份副本，3.5 那边原样保留。
    "fonts/xs_font_clk72.c"        # 时钟专用 72px（只含 0-9 : -）
    # ★★ 10-10 新增：SD 页大字符集（GB2312 符号区+汉字）。
    #   SD 页要显示【运行时才知道的文件名】，主字库那 2037 个界面字不够
    #   ⇒ 真机上「扒」「乱」这类字全是方块。用 _gen_fonts_es28.py cjk 烘。
    #   ⚠️ 它【不在这里】列 —— 自用版/发布版是两份不同的 .c
    #      （自用含台单 750 字，发布版不含），跟着下面的台单 if/else 一起选。
    #   ⚠️ 符号名是 cjk12，实际字号 14px（_gen_fonts_es28.py 的 G.CJK_SIZE）
)

# ============================================================
#  ★★★ 台单形态开关：自用版 / 发布版
# ============================================================
#  判据 = 「main/net_stations_pub.c 在不在」，与 3.5 工程完全同源。
#
#  ★★ 这个 if/else 是【发布形态的唯一真相源】，四件事必须同源（铁律 43）：
#     ① 台单        net_stations.c（1254 条真台源） ↔ net_stations_pub.c（0 条）
#     ② 台名字库    fonts/xs_font_st16.c（按本机台单用字烘 776 字）
#                   ↔ fonts/xs_font_pub_st16.c（GB2312 全集 ∪ cp950 全，16124 字）
#     ③ SD 大字符集 fonts/xs_font_cjk12.c（∪ 台单那 750 字）
#                   ↔ fonts/xs_font_pub_cjk12.c（只有国标字集）
#     ④ 收藏播种    由 XS_FAV_PUBLISH 宏关掉（见 app_fav.c）
#     ★ 少切任何一件都等于没置空：台单文件可以不传，可字库里的**字形**
#       照样把台单带出去（3.5 那边为这件事写过血泪注释）。
#     ★ ②③ 两对文件的【符号名完全相同】（xs_font_st16 / xs_font_cjk12）
#       ⇒ ui_xiandial.c 的 extern 与 F_* 宏一个字都不用改；
#       两版不会同时进编译，所以不存在符号冲突。
#
#  ⚠️⚠️ 注意：本工程树里【放了这个 pub 文件】⇒ 默认就是发布形态。
#     这是照 3.5 的约定来的（那边工程恒为发布形态）。
#     要编【自用版】（= 手上这块开发板要听 1254 个台）：
#         python _build_es28_self.py      ← 它把 pub 文件改名 .HOLD → 编 → 恢复
#         ⚠️ 直接跑 _build_es28.py 编出来的是【0 条台单】的发布版。
#     ⚠️ 改完这里要 touch 本文件，否则 CMake 的 if(EXISTS) 不重新求值。
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/net_stations_pub.c")
    # ---- 发布版：0 条内置台单 ----
    list(APPEND ES28_SRCS "net_stations_pub.c"
                          "fonts/xs_font_pub_st16.c"      # ②
                          "fonts/xs_font_pub_cjk12.c")    # ③
    set(ES28_ST_MSG "发布版 · 0 条台单 · 台名字库=GB2312∪cp950")
    set(XS_FAV_PUBLISH 1)
else()
    # ---- 自用版：1254 条真实台源 ----
    list(APPEND ES28_SRCS "net_stations.c"
                          "fonts/xs_font_st16.c"
                          "fonts/xs_font_cjk12.c")
    set(ES28_ST_MSG "自用版 · 1254 条 · 台名字库=按本机台单用字烘")
    set(XS_FAV_PUBLISH 0)
endif()
message(STATUS "es28：台单用【${ES28_ST_MSG}】")

idf_component_register(
    SRCS
        ${ES28_SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        # ---- 外设驱动（IDF 6.x 已拆成独立组件）----
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc        # 背光 PWM
        esp_driver_spi         # ILI9341 SPI
        esp_driver_i2s         # app_audio.c：I2S0
        esp_driver_sdmmc       # app_sd.c：SDIO 四线
        sdmmc                  # app_sd.c + 格式探测（sdmmc_cmd.h）
        esp_driver_usb_serial_jtag  # app_stlist.c：串口命令通道
        esp_driver_pcnt        # app_audio.c：pclk 引脚时钟探针（数 IO4/5/7/8 边沿）
        esp_adc                # app_pwr.c：电池电压
        # ---- 网络 ----
        esp_wifi
        esp_netif
        esp_event
        esp_http_server        # app_prov.c：配网页
        lwip                   # app_prov.c：DNS 劫持
        esp_http_client        # app_hls.c：m3u8 拉流
        mbedtls                # app_hls.c：HTTPS 证书包
        # ---- 存储 / 系统 ----
        nvs_flash
        fatfs
        esp_timer
        esp_app_format         # esp_app_desc.h
        # ---- 显示与界面 ----
        esp_lcd
        lvgl
        espressif__esp_lvgl_port
        espressif__esp_lcd_ili9341
        espressif__esp_lcd_touch
        espressif__esp_lcd_touch_ft5x06
        # ---- 音频 ----
        espressif__esp_codec_dev
        espressif__esp_audio_codec
        # ---- 语音（ESP-SR，10-11 加）----
        #   ★ 为什么写 `espressif__esp-sr` 而不是 `esp-sr`：
        #     IDF 组件管理器把「espressif/esp-sr」这个注册名映射成组件名
        #     `espressif__esp-sr`，REQUIRES 里必须用后者（前者会报找不到组件）。
        #   ⚠️ 模型【不】随本组件链进 app —— 它们是数据文件，走 `model` 分区
        #      （见 partitions.csv + _prep_voice_models.py）。
        espressif__esp-sr
)

# 发布版不播种收藏（app_fav.c 里的真实台名曾泄漏进 .bin，见 3.5 工程同名注释）
target_compile_definitions(${COMPONENT_LIB} PRIVATE XS_FAV_PUBLISH=${XS_FAV_PUBLISH})

# ★ 10-11：xs_cfg 的默认值按产品给（app_settings.c 里是 #ifndef 兜底）。
#   2.8 拾声 = 开机就有声（音量 50、不静音）——
#   兰兰 10-11：「第一次启动默认声音为 0%，应该起码是 50%」。
#   ⚠️ 3.5 四合一【故意不传这两个宏】，它保留 100/静音（灵签要"默认关声音"）。
#   不写这两行的后果：2.8 刷完固件设置页仍显示「声音 已静音」，
#   而用户会以为"改了没生效"。
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    XS_CFG_DEF_VOLUME=50
    XS_CFG_DEF_MUTE=0
    # ★★★ 10-12：产品名开关 —— 配网页标题/大标题印的是哪个产品。
    #   app_prov.c 是共享内核（真源在 3.5），不传这个宏它默认印
    #   「拾声集 XianDial Suite」—— 那是 3.5 四合一的号，2.8 是单产品「拾声」。
    #   ⚠️ 直接改 app_prov.c 会被 _sync_core.py 静默撤销，必须走宏。
    #   ⚠️ 3.5 不传 ⇒ 它编出来的字节与本条加入前完全相同。
    XS_PROD_SUITE=0
    # ★★★ 10-11：打开麦克风（ES8311 ADC）通路 —— 语音功能要用。
    #   ⚠️ 必须只有 es28 传这个：app_audio.c 是两板共用的（见 _sync_core.py），
    #     3.5 不传 ⇒ 它编出来的音频模块与今天完全一致（不多占 8 KB 内部 DMA）。
    #   详细说明见 app_audio.h 的 AUD_MIC_ENABLE 段。
    AUD_MIC_ENABLE=1
    # ★★★ 10-11：起播前让语音让路（=调 app_voice_stop()：关麦 + 采样率还原 44.1 kHz）。
    #   为什么必须有：S3 上麦克风（ES8311 ADC）与喇叭（DAC）共用一组 I2S，
    #     TX/RX 共享 BCLK/WS ⇒ **16 kHz 听 与 44.1 kHz 播无法并存**；
    #     若语音还在听的时候直接起播，两边都会去 close/open codec ⇒ 冲突。
    #   对应 app_radio.c 里 `#ifdef APP_RADIO_PLAY_HOOK` 那两段（调用点 + 前置声明）。
    #   ⚠️ 3.5 工程【不】传这个宏 ⇒ 真源里那两段不进代码，
    #      两份 app_radio.c 仍然逐字节一致（共享内核要求，见 _sync_core.py）。
    APP_RADIO_PLAY_HOOK=app_voice_stop
)

# ============================================================
#  ★★★ 10-11 晚：把语音模型做成 **SPIFFS 镜像**，烧进 `model` 分区
# ============================================================
#  为什么是 SPIFFS（读源码定案，三条路都试过）：
#    · packed/mmapped（`esp_srmodel_init`）⇒ `set_model_base_path(NULL)`
#      ⇒ MultiNet7 建 FST 时路径拼不出来 ⇒ 打印 `no (null)/fst/...` 然后**空指针崩**。
#    · SD 卡模式（`MODEL_IN_SDCARD`）⇒ 路径有了、FST 建起来了，
#      但接着 `read_spiffs_params`（hufzip_model.c:323）**除零崩**
#      —— 因为 `libhufzip` 要读 **SPIFFS 的参数**来解压模型数据，
#      而 SD 卡模式没有 SPIFFS（用 `strings` 在 libhufzip.a/libmultinet.a 里
#      都能查到 `spiffs` 符号，不是猜的）。
#    · **SPIFFS 模式** ⇒ 两条都满足：`srmodel_spiffs_init()` 会把分区挂到
#      `/srmodel` 并设好基路径（路径能解析），同时 `esp_spiffs_info()` 有值
#      （hufzip 能拿到参数）。这正是组件 `model/CMakeLists.txt` 特意
#      `REQUIRES spiffs` 的原因。
#  目录内容由 `_prep_voice_models.py` 的 `stage_spiffs()` 准备
#  （里面同时放了目录树和根目录平铺的 `*_MODEL_INFO_`，两条读取路径都能满足）。
#  ⚠️ 分区表里 `model` 那行的 subtype 必须是 `spiffs`（0x82），
#    `spiffs_create_partition_image` 才认得出它。
#  ⚠️ 生成物是 `build/model.bin` ⇒ 烧到 0x710000（`_flash_es28.py voice` 负责）。
spiffs_create_partition_image(model
    ${CMAKE_CURRENT_LIST_DIR}/../_voice_spiffs
    FLASH_IN_PROJECT)

