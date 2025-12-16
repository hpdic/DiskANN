#!/bin/bash

# ==========================================
# 配置区域 (Configuration)
# ==========================================

# DiskANN 安装路径 (请确认路径是否正确)
DISKANN_HOME="/home/cc/DiskANN"
BUILDER_BIN="${DISKANN_HOME}/build/apps/build_disk_index"

# 脚本与数据路径
GEN_DATA_SCRIPT="gen_data.py"
DATA_DIR="${DISKANN_HOME}/hpdic_data"
RAW_DATA="${DATA_DIR}/ingest_raw.bin"
INDEX_PREFIX="${DATA_DIR}/ingest_index"

# ==========================================
# 步骤 1: 检查并准备数据
# ==========================================

if [ -f "$RAW_DATA" ]; then
    echo "✅ 检测到数据文件: $RAW_DATA"
    echo "   跳过生成步骤，直接构建索引..."
else
    echo "⚠️  未检测到数据文件: $RAW_DATA"
    echo "   正在调用 Python 脚本生成数据..."
    
    # 检查 Python 脚本是否存在
    if [ ! -f "$GEN_DATA_SCRIPT" ]; then
        echo "❌ 错误: 找不到生成脚本 $GEN_DATA_SCRIPT"
        echo "请将之前的 Python 代码保存为 $GEN_DATA_SCRIPT"
        exit 1
    fi

    # 运行 Python 生成数据
    python3 "$GEN_DATA_SCRIPT"

    # 检查生成是否成功
    if [ $? -ne 0 ]; then
        echo "❌ 错误: Python 数据生成失败。"
        exit 1
    fi
    
    echo "✅ 数据生成完毕！准备构建索引..."
fi

# ==========================================
# 步骤 2: 检查构建工具
# ==========================================

if [ ! -f "$BUILDER_BIN" ]; then
    echo "❌ 错误: 找不到构建工具: $BUILDER_BIN"
    echo "请检查 DISKANN_HOME 路径或是否已编译 DiskANN。"
    exit 1
fi

# ==========================================
# 步骤 3: 运行 DiskANN 构建索引
# ==========================================

# 索引参数
R=32
L=50
B=0.1   # 0.1 GB
M=0.1   # 0.1 GB
THREADS=4

echo "----------------------------------------------------------------"
echo "🛠️  开始构建 DiskANN 索引..."
echo "📂 输入数据: $RAW_DATA"
echo "💾 输出前缀: $INDEX_PREFIX"
echo "⚙️  参数: R=$R, L=$L, Threads=$THREADS"
echo "----------------------------------------------------------------"

"$BUILDER_BIN" \
    --data_type float \
    --dist_fn l2 \
    --data_path "$RAW_DATA" \
    --index_path_prefix "$INDEX_PREFIX" \
    -R "$R" \
    -L "$L" \
    -B "$B" \
    -M "$M" \
    -T "$THREADS"

# 检查最终结果
if [ $? -eq 0 ]; then
    echo "✅ 索引构建成功！"
    echo "生成文件如下："
    ls -lh "${INDEX_PREFIX}"*
else
    echo "❌ 构建失败，请检查上方错误日志。"
    exit 1
fi