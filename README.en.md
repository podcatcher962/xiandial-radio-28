# XianDial · 拾声 (2.8″ hardware build)

**An ESP32-S3 desktop internet radio: 13 categories / 42 regions of live network radio, plus local audio from a TF card and offline voice control.**

> Flashing guide: [FLASHING.md](FLASHING.md) ｜ 中文: [README.md](README.md)
>
> Current firmware: **v2.8.0** (first release for the 2.8″ platform)
>
> The name means "picking up sound". The screen is a 2.8″ IPS landscape panel
> (320×240), meant to sit on a desk. It shares its display / touch / font /
> audio / power service layer with the 3.5″ four-in-one project
> **XianDial Suite**; only the UI layer and board drivers differ.

---

## Features

| What | Detail |
|---|---|
| **Live network radio** | Filter by 13 categories / 42 regions; plain HTTP, direct HTTPS, and **HLS** (`.m3u8`) sources |
| **Local audio** | MP3 from the TF card, **with resume** (picks up where you stopped) |
| **Favourites / history** | Save stations and see recently played; delete from the UI |
| **Play modes** | Repeat-all / single / shuffle as three independent keys, plus playback speed |
| **Gestures** | Swipe to change station, swipe up/down for volume |
| **Voice** | Wake word "**Hi, Miaomiao**" plus 36 command words (category playback, next/prev, volume, play control, power off) |
| **Wi-Fi setup** | Join the device's hotspot → open `192.168.4.1` → enter your Wi-Fi password |
| **Other** | Live spectrum, sleep timer with fade-out, volume / brightness keys (step 10), battery gauge, deep-sleep power off |

### What voice can and cannot do

**Can do** (speak within 6 seconds of the wake word):

| Category | Utterance |
|---|---|
| Category playback | news ｜ traffic ｜ music ｜ arts ｜ storytelling ｜ opera ｜ oldies ｜ education ｜ religious ｜ overseas |
| Change station | next station ｜ previous station |
| Volume | louder ｜ quieter ｜ mute (say again to unmute) |
| Playback | play ｜ pause ｜ stop ｜ play again |
| Other | favourite this ｜ unfavourite ｜ power off |

**Cannot do:** say a specific station name. ESP-SR's MultiNet is
**command-word recognition, not general speech recognition** — feeding it
thousands of station names destroys its accuracy. To hear a particular station,
say the **category** it belongs to.

**Voice and playback are mutually exclusive.** On this board the microphone
(ES8311 ADC) and the speaker (DAC) share one I2S bus with common BCLK/WS, so
**16 kHz capture and 44.1 kHz playback cannot run at the same time**. The
firmware therefore only listens when idle: it opens the mic when nothing is
playing, yields as soon as playback starts, and resumes afterwards.

---

## ★★ Read this first: two builds (station list)

This repository ships the **published build**. The station list is the only
difference between the two builds:

| | **Published build** (default here, when `main/net_stations_pub.c` exists) | **Personal build** |
|---|---|---|
| Built-in stations | **0** | **1254** (incl. Hong Kong / Macao / Taiwan, China and overseas) |
| Station font | `fonts/xs_font_pub_st16.c` (GB2312 ∪ cp950 = 16124 glyphs, 2 bpp) | `fonts/xs_font_st16.c` (subset baked from the local list, 776 glyphs, 4 bpp) |
| SD-page large charset | `fonts/xs_font_pub_cjk12.c` (GB2312 levels 1–2) | `fonts/xs_font_cjk12.c` (**plus the 750 station-list characters**) |
| Favourite seeding | none (`XS_FAV_PUBLISH=1`) | seeds 10 locally-used stations |

**Why the published build scrubs the list:** the station list is not content,
it is an **index**. Shipping it with the firmware means broadcasting on someone
else's behalf, and some addresses only exist because they were verified on the
author's own machine.
⇒ The published build ships an empty list; **you push your own stations to your
own device** (next section).

> ★★ **Omitting the station-list file is not enough** — the same list reaches
> the binary in **three forms**:
> ① `main/net_stations.c` (the list itself);
> ② `fonts/xs_font_st16.c` (**glyphs baked from the local list's characters**);
> ③ `fonts/xs_font_cjk12.c` (which absorbed the same 750 characters).
> Skipping any one of them leaves the list in the firmware **as glyphs**.
> All three, plus ④ the favourite seed list, hang off the **same
> `if(EXISTS net_stations_pub.c)` branch** in `main/CMakeLists.txt` and the
> same `.gitignore` rules.
>
> ★ There is only **one** way to tell which build a given firmware is: count the
> station names inside `build/es28-suite.bin`. When both station-list files are
> present, CMake silently takes the published branch — source inspection cannot
> reveal this.

### Building the personal build (bring your own list)

⚠️ This repository does **not** contain `main/net_stations.c` or the personal
fonts (`fonts/xs_font_st16.c`, `fonts/xs_font_cjk12.c`) — those live in the
author's workspace and are excluded by `.gitignore` (see above).
⇒ **A fresh clone can only build the published build.** To build the personal
build you must supply those files yourself.

Once they are in place, just move the switch file aside (no code changes):

```bash
mv  main/net_stations_pub.c  main/net_stations_pub.c.HOLD
touch main/CMakeLists.txt    # EXISTS is evaluated at configure time - don't skip this
idf.py build
mv  main/net_stations_pub.c.HOLD  main/net_stations_pub.c
```

> ⚠️ **Don't forget the last line**: without `net_stations_pub.c` the tree is a
> personal build; if you forget to restore it, the next build is a personal one.
> In the author's workspace this is handled by `_build_es28_self.py`
> (`try/finally`, restores either way); that script is not in the repository.

---

## Adding your own stations: three steps

### ① Prepare a TSV

Four columns, **TAB**-separated, UTF-8 without BOM, LF line endings:

```
station name	category	region	URL
Example Radio A	music	Shanghai	http://example.com/live/64k.mp3
Example Radio B	news	national	http://example.com/live/playlist.m3u8
```

- **Category** must be one of the 13 fixed Chinese names:
  `新闻综合 交通台 音乐 文艺 说书 戏曲 怀旧老歌 网络台 教育台 电视伴音 综合 宗教 境外新闻`
- **Region** must be one of the 42 fixed Chinese names (31 mainland provinces plus
  Taiwan, China / Hong Kong, China / Macao, China and several overseas buckets),
  or `全国`
- **URL** may be plain HTTP, direct HTTPS, or **HLS** (`.m3u8`)

> ★ **Don't want to hand-write it? Use `XianForge.html`** (a release asset, or
> [`tools/XianForge.html`](tools/XianForge.html)). Double-click to open it in a
> browser and drop in your M3U / TXT / pasted playlist — it produces
> `stations.tsv`. **It runs fully offline and uploads nothing.**

### ② Put it on the TF card (`/sdcard/stations.tsv`, card root)

Copy it with a card reader, or push it over serial.

### ③ Station names will not render as boxes

The published station font is **GB2312 ∪ cp950** (common characters across the
Chinese-speaking world), which covers the vast majority of station names. If
yours contain rarer characters, the font must be rebaked — that needs the
author's font generator and the upstream TTF (LXGW WenKai, OFL-1.1), neither of
which is in this repository (the TTF is 20+ MB, and OFL does not require
shipping it).

> Why a separate font for station names, and why not just use a full character
> set: see the Fonts section below.

---

## Hardware

The author's own unit. **This firmware has only been verified on exactly this
hardware** — a different panel, touch controller, or audio path requires code
changes.

| Item | Spec |
|---|---|
| Board | **LCDWiki ES3C28P** (ESP32-S3) |
| MCU | ESP32-S3 (512 KB SRAM / **8 MB PSRAM** / 16 MB flash, i.e. N16R8) |
| Display | **2.8″ IPS TFT**, **320×240**, ILI9341V, **4-line SPI** (40 MHz) |
| Touch | FT6336G capacitive (I²C, address `0x38`) |
| Audio | ES8311 codec → FM8002E amp, mono speaker (`PA_EN` = IO1, **active low**) |
| Storage | on-board TF socket (4-bit SDIO) |
| Power | 3.7 V Li-ion + LDO; battery ADC sense on **IO9**; deep-sleep power off |
| Stack | ESP-IDF 6.1 + LVGL 9 |
| Partitions | app 7 MB + **6 MB model partition**, **no OTA** — upgrades need reflashing |

**⚠️ On this board the battery ADC is on IO9, not IO8.** The same IO8 is the
battery pin on the 3.5″ board (XianDial Suite) but is **I2S data out (DOUT)**
here. Early firmware copied 3.5's `ADC_CHANNEL_7` (= IO8) and configured the
audio output pin as an analog input, leaving **the whole device silent**. The
2.8 `app_pins.h` now sets its own `BAT_ADC_UNIT/CHAN`, and `app_pwr_init()`
performs a **runtime self-check** with `adc_oneshot_io_to_channel()`, printing
`★★★ ADC misconfigured!` if it is wrong.

> ★ **Do not copy pin assignments across boards** — IO38/39, 41, 42, 47/48 mean
> completely different things on the two boards.
>
> ★ **Orientation**: the panel's `LCD_MIRROR_X` and the touch's `TP_MIRROR_X`
> are **different values** on this board (each determined by its own ribbon and
> bonding). When the orientation is wrong, **change both by the same amount** —
> flipping only one makes taps land on the opposite side.

---

## Fonts

The UI uses **LXGW WenKai** (SIL OFL 1.1 — permits embedding in firmware and
subsetting).

| Size | Used for | Charset |
|---|---|---|
| 14 px | Body text, list rows, status line | **2118 characters** (scanned from UI source) |
| 18 px | Card titles, buttons | same |
| 22 px | Large headings | same |
| 14 px (`xs_font_cjk12`) | **SD-page filenames**, any user text | GB2312 levels 1–2 (+ fallback target) |
| 16 px (`xs_font_st16`) | **Station / category / province names** | Published: GB2312 ∪ cp950; Personal: from the local list |

**The main fonts carry a fallback chain**: `xs_font_14/18/22` all declare
`.fallback → xs_font_cjk12`, so the main fonts can stay "small and exact"
(identical metrics to the previous release = zero layout drift) while missing
characters are taken from the fallback instead of rendering as boxes.

> ★ Why not merge CJK into the main fonts: measured, it pushes the 14 px
> `line_height` from **15 to 16–17**, and the UI is laid out pixel-by-pixel in
> 240 px height — 2 px of extra line height moves the whole layout.
>
> ★ **"The main font lacks this character" ≠ "a box appears on screen"** — check
> the fallback chain first.

Licensing and the reserved-font-name note: see
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

---

## Build and flash

See **[FLASHING.md](FLASHING.md)**. Summary:

| File | Flash at |
|---|---|
| `bootloader.bin` | `0x0` |
| `partition-table.bin` | `0x8000` |
| `es28-suite.bin` (application) | `0x10000` |
| `model.bin` (voice models, SPIFFS) | `0x710000` |

> ⚠️ **Do not flash `es28-suite.bin` alone at `0x0`** — that writes the
> application into the bootloader region and the board will not boot.
> ⚠️ `model.bin` is only **6 MB**, and flashing it **does not erase your Wi-Fi
> credentials or settings** (NVS is at `0x9000`, a fixed address). Booting
> without it is fine — only **voice** is unavailable (a missing model is a soft
> failure; the UI works normally).

---

## Privacy (a commitment, not marketing)

### What the device does on the network

**The only addresses it connects out to are the station URLs you enter** (the
published build's built-in list is empty). There are exactly two other fixed
outbound uses: the setup page (the device's own hotspot `192.168.4.1`, which
never leaves the LAN) and SNTP time sync.

### What is stored on the device

- **NVS** (`xs_cfg` namespace): volume, backlight, mute, play mode, and the
  Wi-Fi credentials from setup
- **TF card**: the station list and audio you put there
- No accounts, no telemetry, no usage data is uploaded

### What is not in this repository

- **Station lists** (`main/net_stations.c` and the personal fonts
  `fonts/xs_font_st*.c`, `fonts/xs_font_cjk12.c`) — see "two builds" above
- **Voice models** (`_voice_models/`, `_voice_spiffs/`, ~5.8 MB) — Espressif's
  model data, distributed with the **firmware image**, not with the source repo
- Any credentials, tokens, personal paths, or device IDs

---

## Known limitations

- **No OTA**: upgrading requires reflashing (no OTA slots in the partition table)
- **TLS-dependent stations do not play**: the largest contiguous block of
  internal RAM is ~31 KB, while a TLS handshake needs 16–40 KB of **contiguous**
  memory — a hard hardware limit, not a bug
- **Voice cannot speak station names** (see above), and **voice is unavailable
  while playing** (mic vs. speaker sample-rate conflict)
- Audio quality and availability depend on **the source you enter**; this
  project guarantees no particular address stays reachable

---

## Disclaimer

- This is a **personal / educational** open-source hardware project.
- All station addresses are **supplied by the user**. This project does not
  bundle, host, relay, or store any audio content. If you are a rights holder
  and want an address removed, open an issue and I will remove it.
- The author accepts no liability for any direct or indirect loss arising from
  the use of this firmware.

---

## Licence and credits

**MIT License** — see [LICENSE](LICENSE).
Third-party components, fonts, and **voice models** are covered in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

- UI and station font: **LXGW WenKai (霞鹜文楷)** — SIL OFL 1.1
- Supplementary font: **Source Han Sans SC (思源黑体)** — SIL OFL 1.1
- Voice: **ESP-SR** (WakeNet / MultiNet) — ESPRESSIF MIT License
- Frameworks: **ESP-IDF**, **LVGL**

© 2026 Lanlan Eternal · 永远的兰兰
