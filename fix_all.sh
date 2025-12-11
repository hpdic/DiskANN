#!/bin/bash

# 替换所有源文件中的 mkl.h
find . -name "*.cpp" -exec sed -i 's/#include <mkl.h>/#include <cblas.h>\n#include <lapacke.h>/g' {} \;
find . -name "*.cpp" -exec sed -i 's/#include "mkl.h"/#include <cblas.h>\n#include <lapacke.h>/g' {} \;

# 替换所有MKL函数和类型
find . -name "*.cpp" -exec sed -i 's/mkl_set_num_threads/openblas_set_num_threads/g' {} \;
find . -name "*.cpp" -exec sed -i 's/mkl_get_max_threads/openblas_get_num_threads/g' {} \;
find . -name "*.cpp" -exec sed -i 's/(MKL_INT)/(int)/g' {} \;
find . -name "*.cpp" -exec sed -i 's/MKL_INT/int/g' {} \;

# 修改CMakeLists.txt - 在第194行添加lapacke
sed -i '194 s/gomp/gomp lapacke/' CMakeLists.txt

echo "修复完成！"
