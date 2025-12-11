#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <memory>
#include <cstring>

// 头文件路径保持现状
#include <index.h> 
#include <parameters.h> 
#include <types.h>

using namespace diskann;

// 生成数据辅助函数
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
    const size_t DIM = 128;
    const size_t NUM_POINTS = 1000;
    const std::string DATA_FILE = "./hpdic_data/data.bin";

    // 1. 生成数据
    generate_data<float>(DATA_FILE, NUM_POINTS, DIM);

    // 2. 准备参数对象 (Write Params & Search Params)
    
    uint32_t L_build = 50;
    uint32_t R = 32;
    uint32_t C = 750;
    float alpha = 1.2f;
    uint32_t num_threads = 4;
    
    // FIX: 根据报错信息严格匹配 7 个参数的顺序和类型
    // (uint32 L, uint32 R, bool saturate, uint32 C, float alpha, uint32 threads, uint32 filter_L)
    auto write_params = std::make_shared<IndexWriteParameters>(
        L_build,      // 1. Search List Size
        R,            // 2. Max Degree
        true,         // 3. Saturate Graph (BOOL!)
        C,            // 4. Max Occlusion Size
        alpha,        // 5. Alpha
        num_threads,  // 6. Num Threads
        0             // 7. Filter List Size (通常默认 0)
    );

    // 搜索参数: L_search, threads
    // 如果这里还报错，可能也需要检查参数个数，但通常是 2 个
    uint32_t L_search = 20;
    auto search_params = std::make_shared<IndexSearchParams>(
        L_search, num_threads
    );

    // 3. 初始化 Index
    std::cout << "Initializing Index...\n";
    std::unique_ptr<Index<float>> index = std::make_unique<Index<float>>(
        diskann::Metric::L2,         
        DIM,                         
        NUM_POINTS,                  
        write_params,                
        search_params,               
        0,                           
        false,                       
        false,                       
        false,                       
        false,                       
        0,                           
        false,                       
        false                        
    );

    // 4. 构建索引
    std::cout << "Building index...\n";
    index->build(DATA_FILE.c_str(), NUM_POINTS);

    // 5. 搜索
    std::vector<float> query(DIM, 0.5f);
    std::vector<uint32_t> ids(5);
    std::vector<float> dists(5);

    std::cout << "Searching...\n";
    index->search(query.data(), 5, 20, ids.data(), dists.data());

    std::cout << "Top-1 ID: " << ids[0] << " Dist: " << dists[0] << "\n";

    return 0;
}