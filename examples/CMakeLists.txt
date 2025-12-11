cmake_minimum_required(VERSION 3.10)
project(DiskANN_Examples)

# 1. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 自动寻找库文件 (可选，但为了保险起见，我们手动指定)
# 假设 DiskANN 的结构是:
#   ../include/   (头文件)
#   ../build/src/ (库文件 libdiskann.so)

# 定义路径变量
set(DISKANN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(DISKANN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/src")

# 3. 添加头文件路径 (-I)
include_directories(${DISKANN_INCLUDE_DIR})

# 4. 添加库文件路径 (-L)
link_directories(${DISKANN_LIB_DIR})

# 5. 设置编译选项 (SIMD 支持等)
add_compile_options(-march=native -O3 -Wall)

# 6. 查找依赖库 (OpenMP, Linux AIO)
find_package(OpenMP REQUIRED)
# Linux AIO 通常没有标准的 cmake module，直接链接 "aio" 即可

# --- 定义可执行文件 ---

# 示例 1: Hello Smoke Test
add_executable(hello_hpdic hello_hpdic.cpp)
target_link_libraries(hello_hpdic 
    diskann 
    aio 
    OpenMP::OpenMP_CXX 
    pthread
)

# 示例 2: Memory Index (Vamana)
add_executable(hello_diskann_index hello_diskann_index.cpp)
target_link_libraries(hello_diskann_index 
    diskann 
    aio 
    OpenMP::OpenMP_CXX 
    pthread
)

# 示例 3: Index Serialization (Save/Load)
add_executable(index_serialization index_serialization.cpp)
target_link_libraries(index_serialization 
    diskann 
    aio 
    OpenMP::OpenMP_CXX 
    pthread
)

# 示例 4: SSD Index (FlashIndex)
add_executable(index_ssd index_ssd.cpp)
target_link_libraries(index_ssd 
    diskann 
    aio 
    OpenMP::OpenMP_CXX 
    pthread
)

# 7. 关键设置：RPATH
# 这样编译出来的程序运行时会自动去 ../build/src 找 .so 文件
# 相当于省去了 LD_LIBRARY_PATH
set_target_properties(hello_hpdic PROPERTIES INSTALL_RPATH "${DISKANN_LIB_DIR}")
set_target_properties(hello_diskann_index PROPERTIES INSTALL_RPATH "${DISKANN_LIB_DIR}")
set_target_properties(index_serialization PROPERTIES INSTALL_RPATH "${DISKANN_LIB_DIR}")
set_target_properties(index_ssd PROPERTIES INSTALL_RPATH "${DISKANN_LIB_DIR}")

# 确保在构建树中也能使用 RPATH
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)