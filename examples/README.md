```bash
# 1. 创建并进入构建目录
mkdir build_examples
cd build_examples

# 2. 生成 Makefile (这一步只需做一次，除非你改了 CMakeLists.txt)
cmake ..

# 3. 编译所有文件
make -j

# 直接运行，无需设置 LD_LIBRARY_PATH (因为我们配置了 RPATH)
./hello_hpdic
./index_ssd
# ... 等等
```