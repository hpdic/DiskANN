/*
 * ======================================================================================
 * File: agents/agent_ingest.cpp
 * Project: AdaDisk - Distributed Agentic System for Adaptive RAG
 *
 * Description:
 * This agent is responsible for the data ingestion phase of the AdaDisk system.
 * It generates raw vector data and invokes the DiskANN CLI tool to build the
 * initial SSD-based index. It acts as the "Producer" in the producer-consumer model.
 *
 * Author: Dongfang Zhao <dzhao@uw.edu>
 * Date:   December 12, 2025
 *
 * Copyright (c) 2025 Dongfang Zhao. All rights reserved.
 * ======================================================================================
 */

#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <filesystem>
#include <cstdlib> 

namespace fs = std::filesystem;

// 1. Data Generation Utility
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
    // === Path Configuration (Ingest Side) ===
    const std::string DIR = "./hpdic_data";
    if (!fs::exists(DIR)) fs::create_directory(DIR);

    // [Key Change] Use 'ingest_' prefix to explicitly mark this as ingestion data
    const std::string DATA_FILE = DIR + "/ingest_raw.bin";
    const std::string INDEX_PREFIX = DIR + "/ingest_index"; 
    
    // Point to the DiskANN CLI tool
    const std::string BUILDER_BIN = "/home/cc/DiskANN/build/apps/build_disk_index";

    const size_t DIM = 128;
    const size_t NUM_POINTS = 10000; 
    const size_t NUM_THREADS = 4;

    // ---------------------------------------------------------
    // Step 1: Generate Raw Data (Ingest Raw Data)
    // ---------------------------------------------------------
    std::cout << "[Agent Ingest] Generating raw data: " << DATA_FILE << "..." << std::endl;
    generate_data<float>(DATA_FILE, NUM_POINTS, DIM);

    // ---------------------------------------------------------
    // Step 2: Build Index
    // ---------------------------------------------------------
    std::cout << "[Agent Ingest] Building DiskANN Index..." << std::endl;
    
    // Call build_disk_index, input: ingest_raw.bin, output: ingest_index_xxx
    std::string cmd = BUILDER_BIN + " "
                      "--data_type float --dist_fn l2 "
                      "--data_path " + DATA_FILE + " "
                      "--index_path_prefix " + INDEX_PREFIX + " "
                      "-R 32 -L 50 -B 0.1 -M 0.1 -T " + std::to_string(NUM_THREADS);

    std::cout << "[Command] " << cmd << std::endl;
    
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[Agent Ingest] Error: Build failed! Check builder path.\n";
        return 1;
    }

    std::cout << "[Agent Ingest] Success! Created index: " << INDEX_PREFIX << std::endl;
    return 0;
}