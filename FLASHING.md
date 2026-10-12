# 烧录指引 · Flashing Guide

> 中文版说明 → [README.md](README.md)

## 硬件要求

| 项 | 规格 |
|---|---|
| 板子 | LCDWiki **ES3C28P**（ESP32-S3） |
| 屏幕 | 2.8 英寸 IPS TFT，320×240，ILI9341V，4-line SPI |
| 触摸 | FT6336G 电容触摸（I²C） |
| 内存 | 8 MB PSRAM |
| Flash | **16 MB** |
| 调试口 | 板载 USB（Serial-JTAG） |

**必须用 USB 数据线**（有些线只能充电）。找不到串口就换线试 —— 这是最常见的原因。

---

## ⚠️ 先看这一段，否则可能刷坏

ESP32-S3 的 flash 里要放**四段**东西，各有固定地址：

| 地址 | 内容 | 文件 |
|---|---|---|
| `0x0` | bootloader | `bootloader.bin` |
| `0x8000` | partition table | `partition-table.bin` |
| `0x10000` | application | `es28-suite.bin` |
| `0x710000` | **语音模型**（SPIFFS） | `model.bin`（**6 MB**） |

**把 `es28-suite.bin` 单独刷到 `0x0` 是错的** —— 那是把应用程序写进引导程序区，
板子会起不来（串口无日志、屏幕全黑）。

若不想记地址，**用合并镜像**（`XianDial-Radio28-v2.8.0-merged.bin`，约 13 MB）：
它已经把四段拼好（**含 6 MB 语音模型**），烧录地址 `0x0`，一把刷完。

> ★ **`model.bin` 是可选的**：不刷它也能开机，只是唤醒词与命令词不工作
> （模型缺失是**软失败** —— 界面照常，语音静默不可用，不会崩、不会白屏）。
>
> ★ **刷 `model.bin` 不会清掉你的设置**：NVS 在 `0x9000`，模型分区在 `0x710000`，
> 两者互不相干。配网信息、音量、收藏、续播进度都留着。
> 反之，**`erase_flash` 会把它们全清掉**。

---

## 方式 A：网页烧录（不用装任何工具）

1. 打开 <https://espressif.github.io/esptool-js/>（Chrome / Edge）
2. 点 `Connect`，选你的串口
3. 文件选**合并镜像**（`…-merged.bin`）
4. 芯片型号 **ESP32-S3**，地址 **`0x0`**
5. `Start`，等它跑完

> 需要较新的 Chrome / Edge（要支持 Web Serial）。
> 合并镜像约 13 MB，网页烧录会比命令行慢一些。

---

## 方式 B：命令行

```bash
pip install esptool

# 找串口号
#   Windows : COM4
#   macOS   : /dev/cusbmodem*  或 /dev/tty.usbserial-*
#   Linux   : /dev/ttyACM0
esptool.py --list-ports
```

把下面的 `COM4` 换成你的串口号。

### B1 · 合并镜像（推荐）

```bash
# 换版本时建议先擦除（会清掉 WiFi 配置、收藏与续播进度）
esptool.py --chip esp32s3 --port COM4 erase_flash

esptool.py --chip esp32s3 --port COM4 --baud 460800 \
    write_flash 0x0 XianDial-Radio28-v2.8.0-merged.bin
```

### B2 · 四个分段文件

```bash
esptool.py --chip esp32s3 --port COM4 --baud 460800 write_flash \
    0x0       bootloader.bin \
    0x8000    partition-table.bin \
    0x10000   es28-suite.bin \
    0x710000  model.bin
```

> 只想更新固件、不想重烧 6 MB 模型时，**只刷前三段**即可（模型没变就没必要重写）。

### 看日志

```bash
esptool.py --chip esp32s3 --port COM4 monitor
```

正常启动会打印：版本号、台单数量、WiFi 状态、模型挂载结果。
**「台单 0 条」与「模型 0 项」都是预期的** —— 前者等你自己导入，
后者说明你没刷 `model.bin`。

退出 monitor：`Ctrl+]`。

> ⚠️ **打开串口这个动作本身会让板子复位** —— USB-Serial-JTAG 把 DTR/RTS 的跳变
> 当成复位请求。所以"日志从头开始"不等于"板子自己重启了"，
> 想抓真正的复位原因，得先开监听、再插线。

---

## 刷完第一次开机

1. 屏幕出现提示后，用手机连它广播的 WiFi 热点（名字形如 `拾声-XXXX`）
2. 浏览器打开 `http://192.168.4.1`，填你家 WiFi 的名称和密码，提交
3. 机器重启，之后就正常用了

**没配网也能继续** —— 机器会停在「0 台」等你导入台源，不会白屏。

---

## 导入台源

**① 生成台单** —— 做成一份 TSV：

```
台名<TAB>栏目<TAB>地区<TAB>URL
```

> ★ 不想手写就用 **`XianForge.html`**（Release 资产里，或仓库 `tools/` 下）：
> 双击用浏览器打开，把 M3U / TXT / 粘贴的播放列表拖进去，它转成 `stations.tsv`。
> 完全离线，不上传任何数据。

**② 放进卡里** —— 把 `stations.tsv` 复制到 TF 卡**根目录**。

**③ 插卡开机** —— 机器自动读取。

卡读不出、文件格式不对都不会白屏，只会停在「0 台」并提示导入。

> ★ **64 GB 及以上的卡必须先格成 FAT32** —— 固件里的 FatFs 没有开 exFAT，
> 出厂预格式化的 exFAT 卡会直接挂载失败。

---

## 台单文件格式

路径固定为 **`/sdcard/stations.tsv`**（TF 卡根目录）：

```
台名<TAB>栏目<TAB>地区<TAB>URL
```

| 项 | 要求 |
|---|---|
| 编码 | UTF-8 **无 BOM** |
| 换行 | LF（**不要** CRLF） |
| 分隔符 | **TAB**（不是空格） |
| 栏目 | 13 个标准名之一（见 README） |
| 地区 | 42 个标准名之一，或 `全国` |
| 空行 / `#` 开头 | 忽略，可当注释 |

> **BOM 和 CRLF 是最常见的坑**：带 BOM ⇒ 第一个台名多一个方块；
> CRLF ⇒ URL 尾部挂个 `\r`，症状是「别的台都好，就这几台不行」，极难查。

---

## 常见问题

| 现象 | 原因 |
|---|---|
| 串口列表里找不到板子 | 换一根**数据线**；或按住 BOOT 键再插线 |
| 烧录报 `Failed to connect` | 同上；或手动进下载模式：按住 `BOOT` → 点 `EN/RESET` → 松开 `BOOT` |
| 刷完屏幕全黑、串口无日志 | ★ 地址刷错了。用合并镜像，或检查 B2 的四个地址 |
| 一直重启 | 烧录不完整，重新 `erase_flash` 再刷 |
| 卡读不出 / 0 台 | 卡没格 FAT32（≥64 GB）；或用了 CRLF / 带 BOM；或文件不在卡根目录 |
| 没有声音 | 2.8 **开机默认音量 50%、不静音**；若仍无声，看是不是按了静音、音量被调到 0，或这台源本身没声音 |
| 语音没反应 | 没刷 `model.bin`；或**正在播放**（播放时固件会自动关麦，这是设计） |
| 某些台播不了 | 源本身的问题（见 README「已知限制」），不是固件 bug |

---

## 从源码编译

需要 [ESP-IDF 6.1](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)。

### ① 取依赖

```bash
idf.py set-target esp32s3
```

`managed_components/`（LVGL、esp-sr 等）会在第一次 build 时按
`dependencies.lock` 自动拉取，需要网络。

### ② 生成语音模型镜像

```bash
python tools/_prep_voice_models.py
```

它从 `managed_components/espressif__esp-sr` 里挑出唤醒词与命令词模型，
生成 `build/model.bin`（SPIFFS 镜像，6 MB）。
**不跑这一步，build 会因为缺 `_voice_spiffs/` 而失败**（那是 CMake 的
`spiffs_create_partition_image` 要读的暂存目录）。

### ③ 编译

```bash
idf.py build
```

产物：`build/bootloader/bootloader.bin`、`build/partition_table/partition-table.bin`、
`build/es28-suite.bin`、`build/model.bin`。

### ④ 出合并镜像

```bash
esptool --chip esp32s3 merge-bin -o merged.bin \
    --flash-mode dio --flash-freq 80m --flash-size 16MB \
    0x0      build/bootloader/bootloader.bin \
    0x8000   build/partition_table/partition-table.bin \
    0x10000  build/es28-suite.bin \
    0x710000 build/model.bin
```

---

© 永远的兰兰 · Lanlan Eternal
