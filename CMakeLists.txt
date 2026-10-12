# es28 Suite —— ESP32-S3 / LCDWiki ES3C28P（2.8″ 240x320）
# ★ 独立工程，与拾声（xiandial-hw/lingqian-radio，3.5″）互不相干：
#   屏协议 / 分辨率 / 引脚面全不同，合并只会两边都变差（见 多产品共存方案.md 第八节）。
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(es28-suite)
