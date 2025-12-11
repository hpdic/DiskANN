#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <memory>
#include <cstring>
#include <thread>
#include <chrono>
#include <filesystem> // C++17 文件系统库

// 核心头文件
#include <index.h> 
#include <parameters.h> 
#include <types.h>

using namespace diskann;
namespace fs = std::filesystem;

// -------------------------
// 辅助函数：生成随机数据
// -------------------------
template<typename T>
void generate_data(const std::string& filename, size_t n, size_t d) {
    std::ofstream out(filename, std::ios::binary);
    int32_t n_pts = (int32_t)n;
    int32_t dim = (int32_t)d;
    out.write((char*)&n_pts, sizeof(int32_t));
    out.write((char*)&dim, sizeof(int32_t));
    std::vector<T> vec(n * d);
    for(auto& x : vec) x = (T)rand() / RAND_MAX;
    out.write((char*)vec.data(), vec.size() * sizeof(T));
    out.close();
}

int main() {
    // === 配置 ===
    const std::string DIR_NAME = "./hpdic_data";
    
    // 0. 自动创建目录
    if (!fs::exists(DIR_NAME)) {
        fs::create_directory(DIR_NAME);
        std::cout << "[Info] Created directory: " << DIR_NAME << std::endl;
    }

    const size_t DIM = 128;
    const size_t NUM_POINTS = 2000; 
    
    // 修改路径：指向 hpdic_data 目录
    const std::string DATA_FILE = DIR_NAME + "/data_serial.bin";
    const std::string INDEX_PREFIX = DIR_NAME + "/saved_index"; 
    
    const uint32_t NUM_THREADS = 4;

    // 1. 生成数据
    std::cout << "[Step 1] Generating raw data in " << DATA_FILE << "..." << std::endl;
    generate_data<float>(DATA_FILE, NUM_POINTS, DIM);

    // === PART A: 构建并保存 (Build & Save) ===
    {
        std::cout << "\n[Step 2] Building Index..." << std::endl;
        
        // 准备参数 (严格匹配签名)
        auto write_params = std::make_shared<IndexWriteParameters>(
            50,   // L_build
            32,   // R
            true, // saturate_graph
            750,  // C
            1.2f, // alpha
            NUM_THREADS,
            0     // filter_list_size
        );

        auto search_params = std::make_shared<IndexSearchParams>(20, NUM_THREADS);

        // 初始化 Index 对象
        auto build_index = std::make_unique<Index<float>>(
            diskann::Metric::L2, DIM, NUM_POINTS, 
            write_params, search_params, 
            0, false, false, false, false, 0, false, false
        );

        // 构建
        build_index->build(DATA_FILE.c_str(), NUM_POINTS);
        
        std::cout << "[Step 3] Saving index to " << INDEX_PREFIX << "..." << std::endl;
        build_index->save(INDEX_PREFIX.c_str());
        
        std::cout << "Index saved. Destroying memory object.\n";
    }

    // 模拟重启
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "\n--- (Simulating Restart) ---\n\n";

    // === PART B: 加载并搜索 (Load & Search) ===
    {
        std::cout << "[Step 4] Loading index from " << INDEX_PREFIX << "..." << std::endl;

        auto write_params = std::make_shared<IndexWriteParameters>(
            50, 32, true, 750, 1.2f, NUM_THREADS, 0
        );
        auto search_params = std::make_shared<IndexSearchParams>(20, NUM_THREADS);

        auto load_index = std::make_unique<Index<float>>(
            diskann::Metric::L2, DIM, NUM_POINTS, 
            write_params, search_params, 
            0, false, false, false, false, 0, false, false
        );

        // 加载索引
        load_index->load(INDEX_PREFIX.c_str(), NUM_POINTS, NUM_POINTS);
        std::cout << "Index loaded successfully!" << std::endl;

        // 搜索
        std::cout << "[Step 5] Performing search..." << std::endl;
        std::vector<float> query(DIM, 0.5f);
        std::vector<uint32_t> ids(5);
        std::vector<float> dists(5);

        load_index->search(query.data(), 5, 20, ids.data(), dists.data());

        std::cout << "Top-1 ID: " << ids[0] << " Dist: " << dists[0] << "\n";
    }

    return 0;
}