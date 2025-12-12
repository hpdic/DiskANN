/*
 * ======================================================================================
 * File: agents/agent_query.cpp
 * Project: AdaDisk - Distributed Agentic System for Adaptive RAG
 *
 * Description:
 * This agent is responsible for the query serving phase of the AdaDisk system.
 * It manages its own query-specific dataset and index to simulate a realistic
 * consumer workload. It checks for the existence of data and indices, building
 * them if necessary (Idempotency), and then executes high-throughput vector searches.
 * It acts as the "Consumer" in the producer-consumer model.
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
#include <memory>
#include <filesystem>
#include <cstdlib>

// Only include headers required for searching
#include <pq_flash_index.h>
#include <linux_aligned_file_reader.h>

using namespace diskann;
namespace fs = std::filesystem;

// 1. Data Generation Function
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
    // === Configuration ===
    const std::string DIR = "./hpdic_data";
    if (!fs::exists(DIR)) fs::create_directory(DIR);

    // [Naming] Use 'query_' prefix, completely separate from ingest
    const std::string DATA_FILE = DIR + "/query_raw.bin";
    const std::string INDEX_PREFIX = DIR + "/query_index"; 
    
    // DiskANN generates a _disk.index file after build; use it to check index existence
    const std::string INDEX_CHECK_FILE = INDEX_PREFIX + "_disk.index";
    
    // Point to the build tool CLI
    const std::string BUILDER_BIN = "/home/cc/DiskANN/build/apps/build_disk_index";

    const size_t DIM = 128;
    const size_t NUM_POINTS = 10000; 
    const size_t NUM_THREADS = 4;

    // ---------------------------------------------------------
    // Step 1: Check Data (Data Check)
    // ---------------------------------------------------------
    if (fs::exists(DATA_FILE)) {
        std::cout << "[Agent Query] Data file exists (" << DATA_FILE << "). Skipping generation.\n";
    } else {
        std::cout << "[Agent Query] Data file missing. Generating " << NUM_POINTS << " vectors...\n";
        generate_data<float>(DATA_FILE, NUM_POINTS, DIM);
        std::cout << "[Agent Query] Data generated.\n";
    }

    // ---------------------------------------------------------
    // Step 2: Check Index (Index Check)
    // ---------------------------------------------------------
    if (fs::exists(INDEX_CHECK_FILE)) {
        std::cout << "[Agent Query] Index exists (" << INDEX_CHECK_FILE << "). Skipping build.\n";
    } else {
        std::cout << "[Agent Query] Index missing. Building via CLI...\n";
        
        // Invoke CLI build
        std::string cmd = BUILDER_BIN + " "
                          "--data_type float --dist_fn l2 "
                          "--data_path " + DATA_FILE + " "
                          "--index_path_prefix " + INDEX_PREFIX + " "
                          "-R 32 -L 50 -B 0.1 -M 0.1 -T " + std::to_string(NUM_THREADS);
        
        std::cout << "Running: " << cmd << std::endl;
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            std::cerr << "Error: Build failed! Check path: " << BUILDER_BIN << "\n";
            return 1;
        }
        std::cout << "[Agent Query] Index built successfully.\n";
    }

    // ---------------------------------------------------------
    // Step 3: Load and Search (Load & Search)
    // ---------------------------------------------------------
    std::cout << "[Agent Query] Loading Index (PQFlashIndex)..." << std::endl;

    std::shared_ptr<AlignedFileReader> reader = std::make_shared<LinuxAlignedFileReader>();

    auto index = std::make_unique<PQFlashIndex<float>>(
        reader, 
        diskann::Metric::L2
    );

    int load_ret = index->load(NUM_THREADS, INDEX_PREFIX.c_str());
    if (load_ret != 0) {
        std::cerr << "Error: Load failed.\n";
        return 1;
    }
    std::cout << "Index loaded. Ready to search.\n";

    // ---------------------------------------------------------
    // Step 4: Search Loop
    // ---------------------------------------------------------
    std::cout << "[Agent Query] Searching..." << std::endl;
    
    // Simulate query
    std::vector<float> query(DIM);
    for(auto& x : query) x = (float)rand() / RAND_MAX;

    std::vector<uint64_t> ids(5);
    std::vector<float> dists(5);

    index->cached_beam_search(
        query.data(), 
        5,              // K
        20,             // L_search
        ids.data(), 
        dists.data(), 
        4,              // Beam Width
        false,          // Use Reorder Data
        nullptr         // Query Stats
    );

    std::cout << "Top-1 ID: " << ids[0] << " Dist: " << dists[0] << "\n";

    return 0;
}