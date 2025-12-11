#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <memory>
#include <filesystem>
#include <cstdlib>

#include <pq_flash_index.h>
#include <linux_aligned_file_reader.h>

using namespace diskann;
namespace fs = std::filesystem;

// 1. 造数据
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
    const std::string DIR = "./hpdic_data";
    if (!fs::exists(DIR)) fs::create_directory(DIR);

    const std::string DATA_FILE = DIR + "/ssd_raw.bin";
    const std::string INDEX_PREFIX = DIR + "/ssd_index"; 
    
    const size_t DIM = 128;
    const size_t NUM_POINTS = 10000; 
    const size_t NUM_THREADS = 4;

    // ---------------------------------------------------------
    // Step 1: 准备数据
    // ---------------------------------------------------------
    std::cout << "[Step 1] Generating raw data..." << std::endl;
    generate_data<float>(DATA_FILE, NUM_POINTS, DIM);

    // ---------------------------------------------------------
    // Step 2: 构建 SSD 索引 (调用 CLI)
    // ---------------------------------------------------------
    std::cout << "[Step 2] Building SSD Index via CLI..." << std::endl;
    
    // 注意：如果你的机器核心数较多，可以适当调大 -T
    // -B 和 -M 设置为 0.1G 以适应小内存测试环境
    std::string cmd = "../build/apps/build_disk_index "
                      "--data_type float --dist_fn l2 "
                      "--data_path " + DATA_FILE + " "
                      "--index_path_prefix " + INDEX_PREFIX + " "
                      "-R 32 -L 50 -B 0.1 -M 0.1 -T " + std::to_string(NUM_THREADS);

    std::cout << "Running: " << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Error: Build failed! Please check if '../build/apps/build_disk_index' exists.\n";
        return 1;
    }

    // ---------------------------------------------------------
    // Step 3: 加载并搜索 (C++ Core)
    // ---------------------------------------------------------
    std::cout << "\n[Step 3] Loading SSD Index (PQFlashIndex)..." << std::endl;

    // FIX 1: 显式使用基类指针类型 AlignedFileReader
    // 这样才能通过 std::shared_ptr<AlignedFileReader>& 的参数检查
    std::shared_ptr<AlignedFileReader> reader = std::make_shared<LinuxAlignedFileReader>();

    auto index = std::make_unique<PQFlashIndex<float>>(
        reader, 
        diskann::Metric::L2
    );

    // 加载
    int load_ret = index->load(NUM_THREADS, INDEX_PREFIX.c_str());
    if (load_ret != 0) {
        std::cerr << "Error: Load failed.\n";
        return 1;
    }
    std::cout << "Index loaded successfully via Linux AIO.\n";

    // 搜索
    std::cout << "[Step 4] Searching..." << std::endl;
    
    std::vector<float> query(DIM, 0.5f);
    std::vector<uint64_t> ids(5);
    std::vector<float> dists(5);

    // FIX 2: 补全参数 (共 8 个)
    // 参数: query, K, L, ids, dists, beam_width, use_reorder, stats
    // beam_width: 4 (SSD 读取并发度，通常设为 4-8)
    // use_reorder_data: false (是否使用全精度向量重排序，需构建时支持)
    // stats: nullptr (不需要详细统计)
    index->cached_beam_search(
        query.data(), 
        5,              // K
        20,             // L_search
        ids.data(), 
        dists.data(), 
        4,              // Beam Width (关键参数)
        false,          // Use Reorder Data
        nullptr         // Query Stats
    );

    std::cout << "Top-1 ID: " << ids[0] << " Dist: " << dists[0] << "\n";

    return 0;
}