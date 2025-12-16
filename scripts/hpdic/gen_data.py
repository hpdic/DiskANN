import numpy as np
import struct
import os

# ================= 配置区域 =================
OUTPUT_DIR = "./hpdic_data"
FILENAME = "ingest_raw.bin"
FULL_PATH = os.path.join(OUTPUT_DIR, FILENAME)

NUM_POINTS = 10000  # N
DIM = 128           # D
# ===========================================

def generate_diskann_binary():
    # 1. 创建目录
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    print(f"🚀 正在生成数据: {NUM_POINTS} 向量, 维度 {DIM}...")

    # 2. 生成随机数据 (float32, 范围 0.0 - 1.0)
    # DiskANN 要求必须是 float32 (4字节)
    data = np.random.rand(NUM_POINTS, DIM).astype(np.float32)

    # 3. 写入二进制文件
    # 格式: [int32: N] [int32: D] [float array...]
    with open(FULL_PATH, "wb") as f:
        # 写入头部 (Header)
        f.write(struct.pack('<i', NUM_POINTS)) # <i 表示小端序 int32
        f.write(struct.pack('<i', DIM))
        
        # 写入向量数据
        f.write(data.tobytes())

    print(f"✅ 成功! 文件已保存至: {FULL_PATH}")
    print(f"📊 文件大小: {os.path.getsize(FULL_PATH) / 1024 / 1024:.2f} MB")

if __name__ == "__main__":
    # 检查 numpy 是否安装
    try:
        import numpy
        generate_diskann_binary()
    except ImportError:
        print("❌ 错误: 需要 numpy 库。请运行 'pip install numpy' 安装。")