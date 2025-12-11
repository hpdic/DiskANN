#!/bin/bash

# 替换源文件中的 mkl.h
sed -i 's/#include "mkl.h"/#include <cblas.h>/g' src/pq.cpp
sed -i 's/#include <mkl.h>/#include <cblas.h>/g' src/math_utils.cpp
sed -i 's/#include "mkl.h"/#include <cblas.h>/g' src/disk_utils.cpp

# 修改 CMakeLists.txt
# 注释掉 MKL_ILP64 定义（第207行）
sed -i '207s/^/# /' CMakeLists.txt

# 替换链接库部分（195-198行）
sed -i '195,198d' CMakeLists.txt
sed -i '194a\                openblas\n                gomp' CMakeLists.txt

echo "修复完成！"
