#!/bin/bash
set -x

# 如果 build 目录不存在则创建
if [ ! -d build ]; then
    mkdir build
fi

# 清空 build 目录（保留目录本身）
rm -rf build/*

# 进入 build 目录编译
cd build && cmake .. && make