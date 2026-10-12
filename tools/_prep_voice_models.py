#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""_prep_voice_models.py —— 从 esp-sr 组件取出本工程要用的语音模型，打成 srmodels.bin。

为什么需要这个脚本（而不是手工把模型拷出来）：
  ① 模型文件住在【托管组件】里（`es28-suite/managed_components/espressif__esp-sr/model/`）。
     组件一重拉（升版本、清 build）就会变；手工拷的副本会与组件**脱节**，
     而且脱节是静默的 —— 这正是本项目铁律 40「手工改生成物 = 下次生成被静默撤销」
     的同族问题。所以规则写在脚本里，随时可重跑、结果幂等。
  ② 组件里躺着 **95 个唤醒词模型 + 13 个 MultiNet**（几百 MB）。
     本工程只要两个：`wn9_himiaomiao_tts`（「Hi，喵喵」）+ `mn7_cn`（通用中文命令词）。
     多打一个进去都是白烧 flash（model 分区只有 3 MB）。
  ③ 打包格式是 esp-sr 自己定的（`model/pack_model.py`）—— 而且
     `esp_srmodel_init()` 读的是 **mmap + packed 单文件**，不是 SPIFFS 文件系统
     （见 model_path.c:510-531：找到分区后直接调 srmodel_mmap_init）。
     ⇒ 必须用它自己的工具打包，塞一个目录进去是不会被认的。

产物：
    es28-suite/_voice_models/<模型名>/...    ← 从组件拷来的模型文件
    es28-suite/_voice_models/srmodels.bin    ← 打包结果

烧录：srmodels.bin 写到 **model 分区**（partitions.csv 里 0x710000，3 MB）。
      `_flash_es28.py voice` 负责这件事。

用法：
    python _prep_voice_models.py           # 拷 + 打包 + 自检
    python _prep_voice_models.py --check   # 只自检现有产物，不重做
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# ★ 10-12：本脚本有【两个摆放位置】，同一份文件不复制逻辑：
#     ① 作者工作区   ESP32-Platform/_prep_voice_models.py    ⇒ 工程在 ./es28-suite
#     ② 发布仓库内   es28-suite/tools/_prep_voice_models.py ⇒ 工程就是上一级
#   ⚠️ 判据用「上一级有没有 main/」而不是照抄相对深度 —— 以后再挪目录不用改代码，
#      也不会出现"仓库里那份与工作区那份悄悄分叉"（那正是铁律 40 的同族坑）。
_d = os.path.join(HERE, "es28-suite")
PROJ = _d if os.path.isdir(os.path.join(_d, "main")) else os.path.dirname(HERE)
COMP_DIR = os.path.join(PROJ, "managed_components", "espressif__esp-sr")
SRC_MODEL = os.path.join(COMP_DIR, "model")
PACKER = os.path.join(SRC_MODEL, "pack_model.py")
OUT_DIR = os.path.join(PROJ, "_voice_models")
OUT_BIN = os.path.join(OUT_DIR, "srmodels.bin")
# ★★★ 10-11 晚：第二个暂存目录 —— 给 **SPIFFS 镜像** 用（`spiffs_create_partition_image`）。
#   为什么要跟 OUT_DIR 分开：
#     ① OUT_DIR 里会生成 `srmodels.bin`（2.8 MB），如果 SPIFFS 也从它建镜像，
#        那个 bin 会被当成一个文件塞进去 ⇒ 直接撑爆 3 MB 分区。
#     ② 两者的**命名约定不同**（见下面 stage_spiffs 的说明）。
SPIFFS_DIR = os.path.join(PROJ, "_voice_spiffs")

# 要打包的模型 —— 必须与 sdkconfig.defaults 里的选择一一对应：
#   唤醒词 CONFIG_SR_WN_WN9_HIMIAOMIAO_TTS → wakenet_model/wn9_himiaomiao_tts
#   命令词 CONFIG_SR_MN_CN_MULTINET7_QUANT → multinet_model/mn7_cn
#
# ★★★ 10-11 晚 补 `fst`（**这一条不加，MultiNet 会直接崩**）：
#   真机实测踩到的崩溃（详见 app_voice.c 里那段长注释）：
#       no (null)/fst/commands_cn.txt or no (null)/mn7_cn/vocab, cannot build fst.
#       Guru Meditation Error: Core 0 panic'ed (LoadProhibited)
#       → model_init at multinet7_quantized.c:2049   ← 预编译库，我们改不了
#   根因（读源码定的案）：
#     · MultiNet7 的搜索图（FST）是**运行时**才建的，它要**真实文件路径**去读
#       `fst/commands_cn.txt`（音素语料）或 `mn7_cn/vocab`（音素表）。
#     · 而 `esp_srmodel_init()` 只走 `srmodel_mmap_init()`（packed 位图模式），
#       并在 `model_path.c:173` 调 `set_model_base_path(NULL)`
#       ⇒ `get_model_base_path()` 返回 NULL ⇒ 路径拼不出来 ⇒ 打印 `(null)` 后空指针崩。
#     · `srmodel_spiffs_init(part)`（**正解**，10-11 晚定案）：
#       它把 model 分区挂到默认基路径 `/srmodel`（`model_path.c:20` 的初值，且
#       **不会**像 mmap 那样 `set_model_base_path(NULL)`）⇒ 路径能拼；
#       同时 SPIFFS 在位 ⇒ `read_spiffs_params`（libhufzip 要的）拿得到非零参数。
#       两条命门一次满足 ⇒ 这就是 MultiNet7 唯一的家。
#       ⚠️ 它扫模型名的那段（`read_models_form_spiffs`）有固有的 off-by-one，
#          所以平铺份要用双下划线命名 —— 见下面 `stage_spiffs()` 的说明。
#   ⇒ 所以：`fst` 也要打进镜像（`fst/commands_cn.txt` 是 MultiNet 运行时建图的语料）。
#     它只有 2 个文本文件、约 10 KB。
WANT = [
    ("wakenet_model",  "wn9_himiaomiao_tts", "CONFIG_SR_WN_WN9_HIMIAOMIAO_TTS"),
    ("multinet_model", "mn7_cn",             "CONFIG_SR_MN_CN_MULTINET7_QUANT"),
    ("multinet_model", "fst",                "（MultiNet 运行时 FST 语料，必须带）"),
]

# `fst` 是【散文件目录】不是模型：它没有 `_MODEL_INFO_`，也不是"模型"语义。
# 自检时对它豁免 `_MODEL_INFO_` 检查（但仍要确认两个语料文件都在）。
NO_INFO_DIRS = {"fst"}

# 与 partitions.csv 的 `model` 行保持一致（改一处要改两处，故在此写死并互相校验）
# ⚠️ 10-11 晚：3 MB → **6 MB**。装不下不是"模型变大"，是 SPIFFS 自身元数据开销
#    （2.97 MB 内容 + 每文件页头/索引页 > 3 MB ⇒ spiffsgen 报 SpiffsFullError）。
PART_OFFSET = 0x710000
PART_SIZE = 0x600000          # 6 MB


def _die(msg):
    print("[语音模型] ✖ " + msg)
    sys.exit(1)


def check_env():
    if not os.path.isdir(SRC_MODEL):
        _die("找不到 esp-sr 组件：%s\n"
             "     ⇒ 先跑 `python _build_es28.py reconf` 把组件拉下来。" % SRC_MODEL)
    if not os.path.isfile(PACKER):
        _die("找不到打包工具：%s" % PACKER)


def parse_packed(path):
    """按 pack_model.py 的格式解出「模型名 → 文件列表」，用于自检。

    格式（见 model/pack_model.py 的注释）：
        uint32            model_num
        每个模型：
          char[32]        model_name
          uint32          file_num
          每个文件：
            char[32]      file_name
            uint32        start（相对文件头的偏移）
            uint32        len
    全部用**小端**（pack_model.py 用 struct.pack('I')，本机 x86 是小端）。
    """
    d = open(path, "rb").read()
    off = 0
    (n_models,) = struct.unpack_from("<I", d, off)
    off += 4
    out = {}
    for _ in range(n_models):
        raw = d[off:off + 32]
        name = raw.split(b"\0", 1)[0].decode("utf-8", "replace")
        off += 32
        (n_files,) = struct.unpack_from("<I", d, off)
        off += 4
        files = []
        for _ in range(n_files):
            fraw = d[off:off + 32]
            fname = fraw.split(b"\0", 1)[0].decode("utf-8", "replace")
            off += 32
            start, flen = struct.unpack_from("<II", d, off)
            off += 8
            files.append((fname, start, flen))
        out[name] = files
    return out


def verify(verbose=True):
    if not os.path.isfile(OUT_BIN):
        _die("产物不存在：%s" % OUT_BIN)
    size = os.path.getsize(OUT_BIN)
    if size > PART_SIZE:
        _die("产物 %d B 超过 model 分区 %d B" % (size, PART_SIZE))
    try:
        models = parse_packed(OUT_BIN)
    except Exception as e:
        _die("解包失败（格式不对？）：%s" % e)

    want = sorted(m[1] for m in WANT)
    got = sorted(models.keys())
    if verbose:
        print("[语音模型] 产物 %s" % os.path.relpath(OUT_BIN, HERE))
        print("           大小 %.2f MB / 分区 %.2f MB（占用 %.0f%%）"
              % (size / 1048576, PART_SIZE / 1048576, size * 100.0 / PART_SIZE))
        for name in got:
            tot = sum(f[2] for f in models[name])
            print("           · %-24s %d 个文件 %.2f MB"
                  % (name, len(models[name]), tot / 1048576))
    if got != want:
        _die("产物里的模型与预期不符\n     期望 %s\n     实际 %s\n"
             "     ⇒ 检查 sdkconfig.defaults 的模型选项与脚本里的 WANT 是否一致"
             % (want, got))
    # 每个模型都必须带 _MODEL_INFO_（esp-sr 靠它认模型元信息，缺了加载会静默失败）
    # ★ 例外：`fst` 是散文件目录，本来就没有 _MODEL_INFO_（见 NO_INFO_DIRS）。
    for name in got:
        names = [f[0] for f in models[name]]
        if name in NO_INFO_DIRS:
            if not any("commands_cn" in n for n in names):
                _die("%s 里没有 commands_cn.txt —— MultiNet 建 FST 会崩" % name)
            continue
        if "_MODEL_INFO_" not in names:
            _die("%s 里缺 _MODEL_INFO_（esp-sr 加载会失败）" % name)
    print("[语音模型] ✅ 自检通过：%d 个模型，%s"
          % (len(got), "、".join(got)))
    return size


def stage():
    """把需要的模型从组件拷到 OUT_DIR（先清掉旧的模型目录，保证幂等）。"""
    os.makedirs(OUT_DIR, exist_ok=True)
    for f in os.listdir(OUT_DIR):
        p = os.path.join(OUT_DIR, f)
        if os.path.isdir(p):
            shutil.rmtree(p)
    for sub, name, _cfg in WANT:
        src = os.path.join(SRC_MODEL, sub, name)
        if not os.path.isdir(src):
            _die("组件里没有这个模型：%s\n"
                 "     ⇒ 组件版本变了？或 sdkconfig 的选择与组件内容不匹配。" % src)
        dst = os.path.join(OUT_DIR, name)
        shutil.copytree(src, dst)
        n = sum(len(fs) for _r, _d, fs in os.walk(dst))
        print("[语音模型] 拷入 %-24s （%d 个文件）" % (name, n))


def pack():
    if os.path.exists(OUT_BIN):
        os.remove(OUT_BIN)
    cmd = [sys.executable, PACKER, "-m", OUT_DIR, "-o", OUT_BIN]
    print("[语音模型] 打包：%s" % " ".join(os.path.basename(c) if i else c
                                          for i, c in enumerate(cmd)))
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        _die("打包失败：\n%s\n%s" % (r.stdout, r.stderr))
    if not os.path.isfile(OUT_BIN):
        # ⚠️ pack_model.py 把 out_file 拼在 model_path 下 ⇒ -o 传绝对路径才落对地方。
        _die("打包命令返回 0 但产物不在 %s（pack_model.py 的 -o 拼路径行为变了？）"
             % OUT_BIN)


def stage_spiffs():
    """给 **SPIFFS 镜像** 准备目录树（`es28-suite/main/CMakeLists.txt` 会拿它建镜像）。

    ★★★ 镜像里放什么（10-11 晚真机实测后定案，别再改）：
      只放【目录树】：
          mn7_cn/{_MODEL_INFO_, vocab, mn7_data, mn7_index}
          wn9_himiaomiao_tts/{_MODEL_INFO_, wn9_data, wn9_index}
          fst/{commands_cn.txt, commands_en.txt}
      理由（两条命门，缺一不可）：
        ① 模型发现：`read_models_form_spiffs()` 靠 `len-13` 从
           `<...>/_MODEL_INFO_` 的名字里剪出模型名。**SPIFFS 是扁平命名空间**
           （没有真目录，`/mn7_cn/` 只是名字前缀），所以目录内那份
           `_MODEL_INFO_` 本来就会被 readdir 扫到 ⇒ 模型名自然得到，
           **不需要**另外放根目录平铺份。
           （我一度以为需要，真机打出 4 项才发现是多余的，已删 —— 见函数体注释。）
        ② 数据读取：各算法库（libhufzip / libmultinet）按**真实路径**找数据：
           `<base>/mn7_cn/vocab`、`<base>/fst/commands_cn.txt`、
           `<base>/wn9_.../wn9_data` ⇒ 必须**保留目录结构**。
    """
    if not os.path.isdir(OUT_DIR):
        _die("还没有 OUT_DIR，先跑一次不带参数的。")
    if os.path.isdir(SPIFFS_DIR):
        shutil.rmtree(SPIFFS_DIR)
    os.makedirs(SPIFFS_DIR)

    for sub, name, _cfg in WANT:
        src = os.path.join(SRC_MODEL, sub, name)
        dst = os.path.join(SPIFFS_DIR, name)
        shutil.copytree(src, dst)
        n = sum(len(fs) for _r, _d, fs in os.walk(dst))
        print("[语音模型] spiffs 目录树：%-24s （%d 个文件）" % (name, n))

        # ★★★ 10-11 晚（真机实测后【删掉】的一段）：原来这里还会额外在 SPIFFS
        #   根目录放一份平铺的 `<模型名>_MODEL_INFO_`。当时的理由是读 model_path.c
        #   的 `read_models_form_spiffs()` 得出"它跳过所有目录、只看扁平文件"。
        #   **真机证明那个理解是错的** —— 挂载后模型列表打出 **4 项**：
        #       [0] mn7_cn  [1] wn9_himiaomiao_tts  [2] mn7_cn  [3] wn9_himiaomiao_tts
        #   根因：**SPIFFS 是扁平命名空间**（没有真目录，`/mn7_cn/` 只是名字前缀），
        #   于是 readdir 会**同时**扫到两条：
        #       `/mn7_cn/_MODEL_INFO_`   → len-13 剪出 "mn7_cn"    ✅
        #       `/mn7_cn__MODEL_INFO_`   → len-13 剪出 "mn7_cn"    ✅（多余那份）
        #   ⇒ 目录里那份本来就会被当成一个模型条目，平铺份纯属多余。
        #     而重复的副作用不小：`afe_config_init` 会对两个 wn9 条目各设一次 WakeNet
        #     （日志里 `Set WakeNet Model` ＋ `Set Second WakeNet Model`，
        #      AFE 管道打印 `WakeNet(a,a)`）⇒ **白加载一份模型实例**、白占 RAM。
        #   ⚠️ 这也顺带说明：`fst/` 从来不需要 `_MODEL_INFO_`，它靠
        #      `<base>/fst/commands_cn.txt` 直接读（真机 `Build fst from commands.` 成功）。

    tot = sum(os.path.getsize(os.path.join(r, f))
              for r, _d, fs in os.walk(SPIFFS_DIR) for f in fs)
    print("[语音模型] spiffs 暂存 %s（%.2f MB / 分区上限 %.2f MB）"
          % (os.path.relpath(SPIFFS_DIR, HERE), tot / 1048576, PART_SIZE / 1048576))
    if tot > PART_SIZE * 0.95:
        _die("spiffs 内容 %.2f MB 太接近分区上限 %.2f MB，SPIFFS 自身还要元数据开销"
             % (tot / 1048576, PART_SIZE / 1048576))
    return 0


def export(dst_dir):
    """把 OUT_DIR 里的模型目录树整体拷到 dst_dir —— 给"手工往 SD 卡拷"兜底用。

    正常路径不靠这个：固件会自己从 model 分区把模型导出到 SD 卡
    （见 app_voice.c 的 voice_export_models_to_sd），不用人插手。
    但万一那条路出问题（卡格式、权限、PSRAM 不够），这个能顶上。
    """
    if not os.path.isdir(OUT_DIR):
        _die("还没有产物，先跑一次不带参数的。")
    os.makedirs(dst_dir, exist_ok=True)
    n = 0
    for name in sorted(os.listdir(OUT_DIR)):
        src = os.path.join(OUT_DIR, name)
        if not os.path.isdir(src):
            continue
        d = os.path.join(dst_dir, name)
        shutil.copytree(src, d, dirs_exist_ok=True)
        n += 1
        print("[语音模型] 导出 %-24s -> %s" % (name, d))
    print("[语音模型] ✅ 已导出 %d 个目录到 %s" % (n, dst_dir))
    print("           把这几个目录放进 SD 卡的 /sdcard/srmodel/ 下即可。")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只自检，不重做")
    ap.add_argument("--export", metavar="DIR",
                    help="把模型目录树导出到 DIR（给手工拷 SD 卡兜底）")
    a = ap.parse_args()

    if a.check:
        verify()
        return 0
    if a.export:
        return export(a.export)

    check_env()
    print("[语音模型] 组件: %s" % os.path.relpath(COMP_DIR, HERE))
    stage()
    pack()
    stage_spiffs()
    verify()
    print("[语音模型] 烧录提示：")
    print("           · SPIFFS 镜像 = es28-suite/build/model.bin ⇒ 写到分区 model @ 0x%06X"
          % PART_OFFSET)
    print("           · 或 packed 包 = _voice_models/srmodels.bin（旧路，作对照）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
