#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""_es28_send.py —— 往 es28 串口发命令并回读日志。
用法： python _es28_send.py "命令1" ["命令2" ...] [--wait 秒]
例：   python _es28_send.py "vol 100" "snd on" --wait 6

★★★ 2026-10-11 修一个我自己造的坑（兰兰报「重启了好几次」）：
  原来这里写的是 `serial.Serial("COM4", 115200, timeout=0.2)`。
  pyserial 的 open() 会把 DTR/RTS 拉起来，而 ESP32-S3 的 **USB-Serial-JTAG
  把这两根线的跳变解释为「主机请求复位」** ⇒ 我每跑一次这个脚本，
  板子就真重启一次。兰兰看到的「重启了好几次」有一半是这样来的
  （日志里是 rst:0x15 USB_UART_CHIP_RESET —— 主机侧发的，不是固件崩）。
  正确姿势：**先设 dtr/rts=False 再 open()**，构造时不碰线，
  _reconfigure_port 也会走 DTR_CONTROL_DISABLE / RTS_CONTROL_DISABLE。
  ⇒ 以后凡是想「只看/只发一条命令、不要打扰正在放的音乐」，都用这个。
  要复位请显式用 _es28_bootlog.py（那个脚本是故意拉 RTS 的）。
"""
import sys
import time

import serial


def open_no_reset(port="COM4", baud=115200, timeout=0.2):
    """开串口，但【不拉 DTR/RTS】⇒ 板子不重启。"""
    s = serial.Serial()
    s.port = port
    s.baudrate = baud
    s.timeout = timeout
    try:
        s.dtr = False          # 必须在 open() 之前 —— 这时只是记到 _dtr_state
        s.rts = False
    except Exception as e:                                     # noqa: BLE001
        print("[warn] 预置 DTR/RTS 失败：%r（可能仍会复位）" % (e,), flush=True)
    s.open()
    return s


argv = sys.argv[1:]
wait = 6.0
if "--wait" in argv:
    i = argv.index("--wait")
    wait = float(argv[i + 1])
    del argv[i:i + 2]
cmds = argv

s = open_no_reset()
log = bytearray()
for c in cmds:
    print(">> %s" % c, flush=True)
    s.write((c + "\r").encode())
    t0 = time.time()
    while time.time() - t0 < 1.2:
        d = s.read(4096)
        if d:
            log += d
t0 = time.time()
while time.time() - t0 < wait:
    d = s.read(8192)
    if d:
        log += d
s.close()
txt = log.decode("utf-8", "replace")
sys.stdout.write(txt)

# ★ 自检：日志里出现 ROM 启动横幅 ⇒ 说明这次 open 又把板子复了位
if ("rst:0x" in txt) or ("ESP-ROM:esp32s3" in txt):
    print("\n[★ 警告] 这次连接把板子复位了（日志里有 rst:0x/ROM 横幅）"
          "—— 无复位打开没生效，别把这次的重启算到固件头上。", flush=True)
