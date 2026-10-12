# Third-party notices / 第三方组件声明

This file lists the third-party components that are bundled with this
project, or were used to generate parts of it. **Their licences do not
change the MIT licence of the firmware source itself** — see
[LICENSE](LICENSE).

本文件列出随本项目分发、或用于生成本项目部分内容的第三方组件。
**这些组件的授权不影响固件源码本身的 MIT 授权** —— 见 [LICENSE](LICENSE)。

---

## 1. Fonts / 字体

The glyph data embedded in the firmware is generated from the following
fonts. All are released under the **SIL Open Font License 1.1**, which
permits embedding and subsetting into a product on condition that the
licence text is distributed with it. That is why the OFL texts ship in
this repository and in every release package.

固件内嵌的字形数据由以下字体生成，均以 **SIL Open Font License 1.1**
授权。该授权允许把字体嵌入、子集化到产品中，条件是随产品一并分发授权
文本 —— 这就是 OFL 文本随本仓库与每个发布包一起分发的原因。

| Font | Upstream project | Licence text | Used for |
|---|---|---|---|
| **LXGW WenKai** (霞鹜文楷) | `lxgw/LxgwWenKai` | [`OFL-1.1-LXGW-WenKai.txt`](OFL-1.1-LXGW-WenKai.txt) | 主字库（14/18/22 px）、台名字库（16 px）、SD 页大字符集（14 px） |
| Source Han Sans SC (思源黑体) | `adobe-fonts/source-han-sans` | [`OFL-1.1-Source-Han-Sans.txt`](OFL-1.1-Source-Han-Sans.txt) | 补字兜底（仅当主字体缺字时才用到） |

No font file is redistributed here. Only subsetted glyph outlines are
embedded in the firmware binary; the original TTFs are not part of this
repository.

本仓库不分发字体文件本身，只在固件二进制中嵌入子集化的字形轮廓；
原始 TTF 文件不属于本仓库。

### Reserved font names / 关于保留字体名

LXGW WenKai's licence carries **Reserved Font Names** (`霞鹜`, `霞鶩`,
`落霞孤鹜`, `落霞孤鶩`, `LXGW`). Its additional permission covers
recompilation and web-font delivery, but **not** the embedding of a subset
into a firmware image — such a subset counts as a *Modified Version* and
must not use the reserved names.

霞鹜文楷带**保留字体名**。它的附加许可覆盖「重编译」与「网页字体投递」
两种情形，但**不含**把子集嵌入固件镜像 —— 那种子集属于 *Modified
Version*，不得使用保留字体名。

That is why every font symbol in this firmware is named `xs_font_*`
(neutral, no reserved name), and why the font is referred to as
"OFL-1.1 subset from LXGW WenKai" rather than by its reserved name in the
binary.

所以本固件里的字体符号一律命名为 `xs_font_*`（中性，不含保留名），
二进制中也不以保留名指代该字体。

---

## 2. Speech models / 语音模型（ESP-SR）

The voice features (wake word + command words) are built on
**ESP-SR** (`espressif/esp-sr`, version `^2.6.0`), distributed by
Espressif Systems under the **ESPRESSIF MIT License**:

```
 ESPRESSIF MIT License

 Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

 Permission is hereby granted for use on all ESPRESSIF SYSTEMS products,
 in which case, it is free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to
 deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

| Model | Kind | Upstream | Size |
|---|---|---|---|
| `wn9_himiaomiao_tts` | WakeNet 9 — 唤醒词「Hi，喵喵」 | `espressif/esp-sr` | 0.28 MB |
| `mn7_cn` | MultiNet7 — 中文命令词识别 | `espressif/esp-sr` | 2.55 MB |

**How they are distributed / 分发方式**：the models are **data**, not
source. They are packed into an SPIFFS image (`model` partition,
`0x710000`, 6 MB) and shipped inside the release firmware
(`…-model.bin` / the merged image), *not* in this source repository. The
repository only contains the generator that extracts them from the
`espressif/esp-sr` component:

模型是**数据**而不是源码：它们被打成 SPIFFS 镜像（`model` 分区，
`0x710000`，6 MB），随**发布固件**（`…-model.bin`／合并镜像）分发，
**不随本源码仓库**分发。仓库里只有从 `espressif/esp-sr` 组件里取模型的
生成脚本 `tools/_prep_voice_models.py`。

> ⚠️ 使用者的使用范围以 Espressif 的授权为准（"for use on all ESPRESSIF
> SYSTEMS products"）。本固件面向 ESP32-S3，符合该条。
>
> The wake word and command vocabulary are the ones bundled with ESP-SR;
> this project does not train or redistribute modified models.
> 唤醒词取自 ESP-SR 随组件附带的词表；本项目不训练、不改造、不再分发修改后的模型。

---

## 3. Station data / 电台数据

**This repository contains no station list.** The published firmware ships
with **0 built-in stations** (`main/net_stations_pub.c`).

**本仓库不含任何电台清单。** 发布版固件内置 **0 条台单**
（`main/net_stations_pub.c`）。

- The author's own station list (`main/net_stations.c`, 1254 entries) is a
  *local* file that is deliberately **excluded from this repository** and
  from every release image — see `.gitignore` ②/②b/②c.
- Users supply their own `stations.tsv` on the TF card. Those addresses
  are the user's own choice and are neither bundled nor endorsed here.
- 作者自用的台单（`main/net_stations.c`，1254 条）是**本地文件**，已被
  刻意排除在本仓库与每个发布镜像之外 —— 见 `.gitignore` 的 ②/②b/②c。
- 使用者的台单由使用者自己放进 TF 卡（`stations.tsv`）。那些地址是使用者
  自己的选择，本项目既不内置也不为其背书。

The firmware only performs outbound **GET** requests to the address you
supply. It does not proxy, relay, record or re-publish any audio content.

固件只对**你自己提供的地址**发出站 GET 请求，不代理、不转发、不录制、
不再发布任何音频内容。

---

## 4. Build dependencies / 编译依赖

Managed by the ESP-IDF Component Manager — see `main/idf_component.yml`
and `dependencies.lock`. All are fetched automatically at build time and
are **not** vendored in this repository.

由 ESP-IDF 组件管理器管理 —— 见 `main/idf_component.yml` 与
`dependencies.lock`。它们在编译时自动拉取，**不**随本仓库分发。

| Component | Licence |
|---|---|
| [ESP-IDF](https://github.com/espressif/esp-idf) 6.x | Apache-2.0 |
| [LVGL](https://github.com/lvgl/lvgl) 9 | MIT |
| `espressif/esp_lvgl_port` | Apache-2.0 |
| `espressif/esp_lcd_ili9341` | Apache-2.0 |
| `espressif/esp_lcd_touch` / `esp_lcd_touch_ft5x06` | Apache-2.0 |
| `espressif/esp_codec_dev` / `esp_audio_codec` | Apache-2.0 |
| `espressif/esp-sr` | ESPRESSIF MIT (see §2) |
| `espressif/dl_fft`, `espressif/esp-dl`, `espressif/esp_new_jpeg` | Apache-2.0 / ESPRESSIF MIT |

`cJSON` is bundled inside ESP-IDF (MIT).

---

© 2026 永远的兰兰 · Lanlan Eternal
