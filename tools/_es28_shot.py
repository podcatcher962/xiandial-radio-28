# -*- coding: utf-8 -*-
"""es28（2.8″ 拾声）· 屏幕截图（走串口的 lq_shot 命令）。

用法：
    python _es28_shot.py [COM口] [输出.png] [启动等待秒] [忽略] [忽略] [swap]

第 6 个参数写 swap ⇒ 把帧内每个 16bit 像素的高低字节交换回来。
为什么需要它（10-10 查了很久才想通）：
    2.8 的 LVGL 缓冲由 lvgl_port 托管，而它带 flags.swap_bytes=1
    （屏要的是"高字节在前"，LVGL 内部是"低字节在前"）。
    lvgl_port 在 flush 时对缓冲做了字节交换，
    于是我们直接读同一个缓冲，拿到的是**已经交换过的**字节。
    后果：颜色整体错位 —— 深蓝黑 C_BG(0x0B0E12) 的 RGB565 小端是 62 08，
    换过来按小端读成 0x6208 ⇒ RGB888 变成 (98,65,66) 的暗红褐色，
    截图看起来"到处是怪色块"，极易误判成花屏 / UI 画错。
    ★ 注意：这只是【截图侧】的问题，屏上颜色本身是对的（屏正好要这个字节序）。

协议（设备侧 es28-suite/main/es28_main.c 的 xs_shot_send）：
    发  "lq_shot\r\n"
    ←   "SHOT <w> <h> <nbytes> <crc32>\r\n"
    ←   nbytes 个原始字节（LVGL 行主序 RGB565，小端）
    ←   "\r\nSHOT_END\r\n"
★ CRC 必须验：控制台日志与这帧二进制走同一个 USB-JTAG，
  中途插进一行日志就会整体错位 —— 表现是颜色全乱，但字节数一个不差。
"""
import sys
import time
import zlib

import numpy as np
from PIL import Image
import serial


def drain(ser, sec):
    t0 = time.time()
    while time.time() - t0 < sec:
        ser.read(16384)
        time.sleep(0.05)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM4'   # 2.8 板在 COM4
    # ★ 默认输出到【当前目录】，不要写死开发机上的绝对路径 ——
    #   文件名是发布物的一部分，本机路径属于发布禁区（发布前扫描会拦下）。
    out = sys.argv[2] if len(sys.argv) > 2 else 'shot.png'
    after = float(sys.argv[3]) if len(sys.argv) > 3 else 16.0
    page = sys.argv[4] if len(sys.argv) > 4 else None
    pick = sys.argv[5] if len(sys.argv) > 5 else None

    ser = serial.Serial(port, 115200, timeout=0.3)
    print('已打开 %s —— 等板子起来（%.0f 秒）…' % (port, after))
    drain(ser, after)
    # ★★ 必须【彻底静默】再发命令：控制台日志与二进制帧走同一条 USB-JTAG，
    #   中间插进任何一行日志都会把帧流整体错位（字节数照样"看起来"对）。
    #   这里读空 2 秒确认安静；日志止不住就退避重试。
    for attempt in range(6):
        drain(ser, 2.0)
        pending = ser.in_waiting
        print('  静默确认 %d/6：残留 %d 字节' % (attempt + 1, pending))
        if pending == 0:
            break
        time.sleep(1.0)
    ser.reset_input_buffer()

    if page is not None:
        # ⚠️ 2.8 固件【没有】lq_demo 跳页命令（那是 3.5 竖版 UI 的调试口），
        #    传了也没用 —— 只提示一下，不发送，免得污染文件流。
        print('（忽略 page/pick：2.8 固件无 lq_demo 命令）')

    print('发 lq_shot …')
    ser.write(b'lq_shot\r\n')

    buf = bytearray()
    hdr = None
    payload = b''
    t0 = time.time()
    while time.time() - t0 < 20:
        d = ser.read(8192)
        if d:
            buf += d
        i = buf.find(b'SHOT ')
        if i >= 0:
            j = buf.find(b'\r\n', i)
            if j > i:
                hdr = buf[i:j].decode('ascii', 'replace')
                payload = bytes(buf[j + 2:])
                break
        if len(buf) > 200000:
            del buf[:-200000]
    if not hdr:
        print('❌ 没等到 SHOT 头。最后收到的内容：')
        print(bytes(buf[-400:]))
        ser.close()
        return 2

    p = hdr.split()
    w, h, nb = int(p[1]), int(p[2]), int(p[3])
    want = int(p[4], 16)
    print('头：%s' % hdr)

    data = bytearray(payload)
    t0 = time.time()
    last = 0
    while len(data) < nb and time.time() - t0 < 40:
        d = ser.read(65536)
        if d:
            data += d
            if len(data) - last >= 32768:
                last = len(data)
                print('    …%d/%d 字节（%.0f%%）'
                      % (len(data), nb, 100.0 * len(data) / nb), flush=True)
    if len(data) < nb:
        print('❌ 只收到 %d/%d 字节' % (len(data), nb))
        ser.close()
        return 3
    data = bytes(data[:nb])

    got = zlib.crc32(data) & 0xFFFFFFFF
    print('CRC %08X / %08X  %s' % (got, want, '✅ 一致' if got == want else '❌ 不一致（混进日志了）'))

    a = np.frombuffer(data, dtype='<u2')
    if len(sys.argv) > 6 and sys.argv[6] == 'swap':
        # ★ 还原 lvgl_port 在 flush 时做过的 swap_bytes（见文件头说明）
        a = a.byteswap()
        print('（已把帧内 16bit 字节交换回来）')
    a = a.reshape(h, w)
    r = (((a >> 11) & 0x1F).astype(np.uint16) * 255 // 31)
    g = (((a >> 5) & 0x3F).astype(np.uint16) * 255 // 63)
    b = ((a & 0x1F).astype(np.uint16) * 255 // 31)
    img = np.dstack([r, g, b]).astype(np.uint8)
    Image.fromarray(img).save(out)
    print('已保存', out, img.shape)
    ser.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
