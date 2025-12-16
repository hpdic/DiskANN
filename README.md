# [HPDIC MOD] AdaDisk: A Distributed Agentic System for Adaptive Ingestion-Query Scheduling of DiskANN RAG in LLM Serving

## Motivation: Resolving the Freshness-Latency Dilemma in Production RAG and LLM Serving
In production Retrieval-Augmented Generation (RAG) environments, the system faces a fundamental conflict: the need for **Knowledge Freshness** (continuous data ingestion) versus the demand for **Serving Low-Latency** (real-time query retrieval).

Standard monolithic vector search pipelines (e.g., vanilla DiskANN) couple these workloads tightly. High-throughput indexing jobs often monopolize I/O bandwidth and CPU resources, causing severe **Resource Contention**. This leads to:
1.  **TTFT Spikes:** The Time-to-First-Token for LLM generation degrades significantly as the database locks up during updates.
2.  **Stale Knowledge:** To avoid latency penalties, operators often pause updates during peak hours, serving outdated information to users.

## The AdaDisk Solution: Distributed Agentic Orchestration
**AdaDisk** reimagines the vector storage layer as a **Distributed Agentic System**, decoupling the RAG lifecycle into autonomous, asynchronous workflows:

* **Decoupled Architecture:** 
    * **Ingest Agents (Producers):** Handle data validation and index construction in the background/near-line, ensuring data integrity without blocking the read path.
    * **Query Agents (Consumers):** Dedicated to serving real-time inference requests with strict latency SLAs, isolating them from write-heavy operations.
    
* **LLM-Driven Control Plane:**
    Unlike rigid cron jobs or static scripts, AdaDisk employs lightweight **Small Language Models (SLMs)** as the system's control plane. These agents autonomously validate inputs, verify system readiness, and make adaptive scheduling decisions (e.g., back-pressure handling), ensuring the database remains robust under fluctuating RAG workloads.

## Compilation
The original DiskANN didn't work for AMD CPUs due to the use of some Intel-specific optimizations.
We have modified the build scripts to allow compilation on AMD CPUs by replacing Intel MKL with OpenBLAS.
To build the modified DiskANN library, follow these steps:
```bash
bash build_shared.sh
bash patch_and_build.sh
# After modifying diskANN app, e.g., app/build_disk_index.cpp:
cd build
make build_disk_index -j
# After modifying diskANN kernel, e.g., src/pq_flash_index.cpp:
cd build
make -j
```

## Quick start
```bash
bash scripts/run_build_disk_index.sh
```

## Agentic execution
This is what's happening in the multi-agent setup of LLM serving, where multiple agents are running concurrently to handle different tasks such as data ingestion and query processing. 
- The HPDIC MOD introduces a new agent called `agent_AdaDisk.py` that dynamically adjusts the search parameters based on the runtime variance of the search queries and possible concurrent ingestion.
- The following examples were tested with a tiny LLM *llama3.2:1b* and a production LLM *llama4:maverick*; example outputs can be found in `agents/results/`.
```bash
# You should read DiskANN/agents/INSTALL.md for instructions on how to install the agents. The following illustrates a simple example of how to run two workers, one for data ingestion and one for query processing, in a distributed agentic manner:
cd agents
mkdir build
cd build
cmake ..
make -j
# Make sure each agent is working correctly by itself.
./agent_ingest
./agent_query
cd ..
# Run multiple agents:
python agent_AdaDisk.py # --model llama4:maverick
```

## VS Code
```bash
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
# Then update .vscode/c_cpp_properties.json to include the following:
#             "compileCommands": "${workspaceFolder}/build/compile_commands.json",
```

## Run singular DiskANN programs with CPP
If you prefer *cmake*, do the regular things (e.g., `mkdir build; cd build; cmake ..; make -j;`); otherwise you can test it by directly compiling the examples:
```bash
cd examples
g++ -std=c++17 -march=native hello_hpdic.cpp \
    -I../include -L../build/src -ldiskann -laio -o hello_diskann.bin
./hello_diskann.bin 
# compiler+linker smoke test OK
g++ -std=c++17 -march=native hello_diskann_index.cpp \
    -I../include \
    -L../build/src \
    -Wl,-rpath=../build/src \
    -ldiskann -laio -lgomp -lpthread \
    -o hello_diskann_index.bin
./hello_diskann_index.bin
# Initializing Index...
# L2: Using AVX2 distance computation DistanceL2Float
# Building index...
# Using only first 1000 from file.. 
# Starting index build with 1000 points... 
# 0% of index build completed.Starting final cleanup..done. Link time: 1.36068s
# Index built with degree: max:32  avg:32  min:32  count(deg<2):0
# Searching...
# Top-1 ID: 64 Dist: 8.40254
g++ -std=c++17 -march=native index_serialization.cpp \
    -I../include \
    -L../build/src \
    -Wl,-rpath=../build/src \
    -ldiskann -laio -lgomp -lpthread \
    -o index_serialization.bin
./index_serialization.bin
# [Info] Created directory: ./hpdic_data
# [Step 1] Generating raw data in ./hpdic_data/data_serial.bin...

# [Step 2] Building Index...
# L2: Using AVX2 distance computation DistanceL2Float
# Using only first 2000 from file.. 
# Starting index build with 2000 points... 
# 0% of index build completed.Starting final cleanup..done. Link time: 2.95362s
# Index built with degree: max:32  avg:32  min:32  count(deg<2):0
# [Step 3] Saving index to ./hpdic_data/saved_index...
# Not saving tags as they are not enabled.
# Time taken for save: 0.00134s.
# Index saved. Destroying memory object.

# --- (Simulating Restart) ---

# [Step 4] Loading index from ./hpdic_data/saved_index...
# L2: Using AVX2 distance computation DistanceL2Float
# From graph header, expected_file_size: 264024, _max_observed_degree: 32, _start: 1230, file_frozen_pts: 0
# Loading vamana graph ./hpdic_data/saved_index...done. Index has 2000 nodes and 64000 out-edges, _start is set to 1230
# Num frozen points:0 _nd: 2000 _start: 1230 size(_location_to_tag): 0 size(_tag_to_location):0 Max points: 2000
# Index loaded successfully!
# [Step 5] Performing search...
# Top-1 ID: 1230 Dist: 8.12346
g++ -std=c++17 -march=native index_ssd.cpp \
    -I../include \
    -L../build/src \
    -Wl,-rpath=../build/src \
    -ldiskann -laio -lgomp -lpthread \
    -o index_ssd.bin
./index_ssd.bin 
# [Step 1] Generating raw data...
# [Step 2] Building SSD Index via CLI...
# Running: ../build/apps/build_disk_index --data_type float --dist_fn l2 --data_path ./hpdic_data/ssd_raw.bin --index_path_prefix ./hpdic_data/ssd_index -R 32 -L 50 -B 0.1 -M 0.1 -T 4
# Starting index build: R=32 L=50 Query RAM budget: 1.07374e+08 Indexing ram budget: 0.1 T: 4
# Compressing 128-dimensional data into 128 bytes per vector.
# Opened: ./hpdic_data/ssd_raw.bin, size: 5120008, cache_size: 5120008
# Training data with 10000 samples loaded.
# Processing chunk 0 with dimensions [0, 1)
# Processing chunk 1 with dimensions [1, 2)
# Processing chunk 2 with dimensions [2, 3)
# Processing chunk 3 with dimensions [3, 4)
# Processing chunk 4 with dimensions [4, 5)
# Processing chunk 5 with dimensions [5, 6)
# Processing chunk 6 with dimensions [6, 7)
# Processing chunk 7 with dimensions [7, 8)
# Processing chunk 8 with dimensions [8, 9)
# Processing chunk 9 with dimensions [9, 10)
# Processing chunk 10 with dimensions [10, 11)
# Processing chunk 11 with dimensions [11, 12)
# Processing chunk 12 with dimensions [12, 13)
# Processing chunk 13 with dimensions [13, 14)
# Processing chunk 14 with dimensions [14, 15)
# Processing chunk 15 with dimensions [15, 16)
# Processing chunk 16 with dimensions [16, 17)
# Processing chunk 17 with dimensions [17, 18)
# Processing chunk 18 with dimensions [18, 19)
# Processing chunk 19 with dimensions [19, 20)
# Processing chunk 20 with dimensions [20, 21)
# Processing chunk 21 with dimensions [21, 22)
# Processing chunk 22 with dimensions [22, 23)
# Processing chunk 23 with dimensions [23, 24)
# Processing chunk 24 with dimensions [24, 25)
# Processing chunk 25 with dimensions [25, 26)
# Processing chunk 26 with dimensions [26, 27)
# Processing chunk 27 with dimensions [27, 28)
# Processing chunk 28 with dimensions [28, 29)
# Processing chunk 29 with dimensions [29, 30)
# Processing chunk 30 with dimensions [30, 31)
# Processing chunk 31 with dimensions [31, 32)
# Processing chunk 32 with dimensions [32, 33)
# Processing chunk 33 with dimensions [33, 34)
# Processing chunk 34 with dimensions [34, 35)
# Processing chunk 35 with dimensions [35, 36)
# Processing chunk 36 with dimensions [36, 37)
# Processing chunk 37 with dimensions [37, 38)
# Processing chunk 38 with dimensions [38, 39)
# Processing chunk 39 with dimensions [39, 40)
# Processing chunk 40 with dimensions [40, 41)
# Processing chunk 41 with dimensions [41, 42)
# Processing chunk 42 with dimensions [42, 43)
# Processing chunk 43 with dimensions [43, 44)
# Processing chunk 44 with dimensions [44, 45)
# Processing chunk 45 with dimensions [45, 46)
# Processing chunk 46 with dimensions [46, 47)
# Processing chunk 47 with dimensions [47, 48)
# Processing chunk 48 with dimensions [48, 49)
# Processing chunk 49 with dimensions [49, 50)
# Processing chunk 50 with dimensions [50, 51)
# Processing chunk 51 with dimensions [51, 52)
# Processing chunk 52 with dimensions [52, 53)
# Processing chunk 53 with dimensions [53, 54)
# Processing chunk 54 with dimensions [54, 55)
# Processing chunk 55 with dimensions [55, 56)
# Processing chunk 56 with dimensions [56, 57)
# Processing chunk 57 with dimensions [57, 58)
# Processing chunk 58 with dimensions [58, 59)
# Processing chunk 59 with dimensions [59, 60)
# Processing chunk 60 with dimensions [60, 61)
# Processing chunk 61 with dimensions [61, 62)
# Processing chunk 62 with dimensions [62, 63)
# Processing chunk 63 with dimensions [63, 64)
# Processing chunk 64 with dimensions [64, 65)
# Processing chunk 65 with dimensions [65, 66)
# Processing chunk 66 with dimensions [66, 67)
# Processing chunk 67 with dimensions [67, 68)
# Processing chunk 68 with dimensions [68, 69)
# Processing chunk 69 with dimensions [69, 70)
# Processing chunk 70 with dimensions [70, 71)
# Processing chunk 71 with dimensions [71, 72)
# Processing chunk 72 with dimensions [72, 73)
# Processing chunk 73 with dimensions [73, 74)
# Processing chunk 74 with dimensions [74, 75)
# Processing chunk 75 with dimensions [75, 76)
# Processing chunk 76 with dimensions [76, 77)
# Processing chunk 77 with dimensions [77, 78)
# Processing chunk 78 with dimensions [78, 79)
# Processing chunk 79 with dimensions [79, 80)
# Processing chunk 80 with dimensions [80, 81)
# Processing chunk 81 with dimensions [81, 82)
# Processing chunk 82 with dimensions [82, 83)
# Processing chunk 83 with dimensions [83, 84)
# Processing chunk 84 with dimensions [84, 85)
# Processing chunk 85 with dimensions [85, 86)
# Processing chunk 86 with dimensions [86, 87)
# Processing chunk 87 with dimensions [87, 88)
# Processing chunk 88 with dimensions [88, 89)
# Processing chunk 89 with dimensions [89, 90)
# Processing chunk 90 with dimensions [90, 91)
# Processing chunk 91 with dimensions [91, 92)
# Processing chunk 92 with dimensions [92, 93)
# Processing chunk 93 with dimensions [93, 94)
# Processing chunk 94 with dimensions [94, 95)
# Processing chunk 95 with dimensions [95, 96)
# Processing chunk 96 with dimensions [96, 97)
# Processing chunk 97 with dimensions [97, 98)
# Processing chunk 98 with dimensions [98, 99)
# Processing chunk 99 with dimensions [99, 100)
# Processing chunk 100 with dimensions [100, 101)
# Processing chunk 101 with dimensions [101, 102)
# Processing chunk 102 with dimensions [102, 103)
# Processing chunk 103 with dimensions [103, 104)
# Processing chunk 104 with dimensions [104, 105)
# Processing chunk 105 with dimensions [105, 106)
# Processing chunk 106 with dimensions [106, 107)
# Processing chunk 107 with dimensions [107, 108)
# Processing chunk 108 with dimensions [108, 109)
# Processing chunk 109 with dimensions [109, 110)
# Processing chunk 110 with dimensions [110, 111)
# Processing chunk 111 with dimensions [111, 112)
# Processing chunk 112 with dimensions [112, 113)
# Processing chunk 113 with dimensions [113, 114)
# Processing chunk 114 with dimensions [114, 115)
# Processing chunk 115 with dimensions [115, 116)
# Processing chunk 116 with dimensions [116, 117)
# Processing chunk 117 with dimensions [117, 118)
# Processing chunk 118 with dimensions [118, 119)
# Processing chunk 119 with dimensions [119, 120)
# Processing chunk 120 with dimensions [120, 121)
# Processing chunk 121 with dimensions [121, 122)
# Processing chunk 122 with dimensions [122, 123)
# Processing chunk 123 with dimensions [123, 124)
# Processing chunk 124 with dimensions [124, 125)
# Processing chunk 125 with dimensions [125, 126)
# Processing chunk 126 with dimensions [126, 127)
# Processing chunk 127 with dimensions [127, 128)
# Writing bin: ./hpdic_data/ssd_index_pq_pivots.bin
# bin: #pts = 256, #dims = 128, size = 131080B
# Finished writing bin.
# Writing bin: ./hpdic_data/ssd_index_pq_pivots.bin
# bin: #pts = 128, #dims = 1, size = 520B
# Finished writing bin.
# Writing bin: ./hpdic_data/ssd_index_pq_pivots.bin
# bin: #pts = 129, #dims = 1, size = 524B
# Finished writing bin.
# Writing bin: ./hpdic_data/ssd_index_pq_pivots.bin
# bin: #pts = 4, #dims = 1, size = 40B
# Finished writing bin.
# Saved pq pivot data to ./hpdic_data/ssd_index_pq_pivots.bin of size 136220B.
# Opened: ./hpdic_data/ssd_raw.bin, size: 5120008, cache_size: 5120008
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 4, #dims = 1...
# done.
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 256, #dims = 128...
# done.
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 128, #dims = 1...
# done.
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 129, #dims = 1...
# done.
# Loaded PQ pivot information
# Processing points  [0, 10000)...done.
# Time for generating quantized data: 29.524569 seconds
# Full index fits in RAM budget, should consume at most 0.00744164GiBs, so building in one shot
# L2: Using AVX2 distance computation DistanceL2Float
# Passed, empty search_params while creating index config
# Using only first 10000 from file.. 
# Starting index build with 10000 points... 
# 0% of index build completed.Starting final cleanup..done. Link time: 6.86634s
# Index built with degree: max:32  avg:32  min:32  count(deg<2):0
# Not saving tags as they are not enabled.
# Time taken for save: 0.006155s.
# Time for building merged vamana index: 6.891833 seconds
# Opened: ./hpdic_data/ssd_raw.bin, size: 5120008, cache_size: 5120008
# Vamana index file size=1320024
# Opened: ./hpdic_data/ssd_index_disk.index, cache_size: 67108864
# medoid: 1230B
# max_node_len: 644B
# nnodes_per_sector: 6B
# # sectors: 1667
# Sector #0written
# Finished writing 6832128B
# Writing bin: ./hpdic_data/ssd_index_disk.index
# bin: #pts = 9, #dims = 1, size = 80B
# Finished writing bin.
# Output disk index file written to ./hpdic_data/ssd_index_disk.index
# Finished writing 6832128B
# Time for generating disk layout: 0.052242 seconds
# Opened: ./hpdic_data/ssd_raw.bin, size: 5120008, cache_size: 5120008
# Loading base ./hpdic_data/ssd_raw.bin. #points: 10000. #dim: 128.
# Wrote 1013 points to sample file: ./hpdic_data/ssd_index_sample_data.bin
# Indexing time: 36.4732

# [Step 3] Loading SSD Index (PQFlashIndex)...
# L2: Using AVX2 distance computation DistanceL2Float
# L2: Using AVX2 distance computation DistanceL2Float
# Reading bin file ./hpdic_data/ssd_index_pq_compressed.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_compressed.bin... 
# Metadata: #pts = 10000, #dims = 128...
# done.
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 4, #dims = 1...
# done.
# Offsets: 4096 135176 135696 136220
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 256, #dims = 128...
# done.
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 128, #dims = 1...
# done.
# Reading bin file ./hpdic_data/ssd_index_pq_pivots.bin ...
# Opening bin file ./hpdic_data/ssd_index_pq_pivots.bin... 
# Metadata: #pts = 129, #dims = 1...
# done.
# Loaded PQ Pivots: #ctrs: 256, #dims: 128, #chunks: 128
# Loaded PQ centroids and in-memory compressed vectors. #points: 10000 #dim: 128 #aligned_dim: 128 #chunks: 128
# Disk-Index File Meta-data: # nodes per sector: 6, max node len (bytes): 644, max node degree: 32
# Opened file : ./hpdic_data/ssd_index_disk.index
# Setting up thread-specific contexts for nthreads: 4
# allocating ctx: 0x7b084f360000 to thread-id:135275619039168
# allocating ctx: 0x7b084f34f000 to thread-id:135266419332800
# allocating ctx: 0x7b084e91d000 to thread-id:135266427725504
# allocating ctx: 0x7b084e90c000 to thread-id:135266436118208
# Loading centroid data from medoids vector data of 1 medoid(s)
# done..
# Index loaded successfully via Linux AIO.
# [Step 4] Searching...
# Top-1 ID: 1230 Dist: 8.12346
# Clearing scratch
```

# DiskANN

[![DiskANN Main](https://github.com/microsoft/DiskANN/actions/workflows/push-test.yml/badge.svg?branch=main)](https://github.com/microsoft/DiskANN/actions/workflows/push-test.yml)
[![PyPI version](https://img.shields.io/pypi/v/diskannpy.svg)](https://pypi.org/project/diskannpy/)
[![Downloads shield](https://pepy.tech/badge/diskannpy)](https://pepy.tech/project/diskannpy)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[![DiskANN Paper](https://img.shields.io/badge/Paper-NeurIPS%3A_DiskANN-blue)](https://papers.nips.cc/paper/9527-rand-nsg-fast-accurate-billion-point-nearest-neighbor-search-on-a-single-node.pdf)
[![DiskANN Paper](https://img.shields.io/badge/Paper-Arxiv%3A_Fresh--DiskANN-blue)](https://arxiv.org/abs/2105.09613)
[![DiskANN Paper](https://img.shields.io/badge/Paper-Filtered--DiskANN-blue)](https://harsha-simhadri.org/pubs/Filtered-DiskANN23.pdf)


DiskANN is a suite of scalable, accurate and cost-effective approximate nearest neighbor search algorithms for large-scale vector search that support real-time changes and simple filters.
This code is based on ideas from the [DiskANN](https://papers.nips.cc/paper/9527-rand-nsg-fast-accurate-billion-point-nearest-neighbor-search-on-a-single-node.pdf), [Fresh-DiskANN](https://arxiv.org/abs/2105.09613) and the [Filtered-DiskANN](https://harsha-simhadri.org/pubs/Filtered-DiskANN23.pdf) papers with further improvements. 
This code forked off from [code for NSG](https://github.com/ZJULearning/nsg) algorithm.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

See [guidelines](CONTRIBUTING.md) for contributing to this project.

## Linux build:

Install the following packages through apt-get

```bash
sudo apt install make cmake g++ libaio-dev libgoogle-perftools-dev clang-format libboost-all-dev
```

### Install Intel MKL
#### Ubuntu 20.04 or newer
```bash
sudo apt install libmkl-full-dev
```

#### Earlier versions of Ubuntu
Install Intel MKL either by downloading the [oneAPI MKL installer](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onemkl.html) or using [apt](https://software.intel.com/en-us/articles/installing-intel-free-libs-and-python-apt-repo) (we tested with build 2019.4-070 and 2022.1.2.146).

```
# OneAPI MKL Installer
wget https://registrationcenter-download.intel.com/akdlm/irc_nas/18487/l_BaseKit_p_2022.1.2.146.sh
sudo sh l_BaseKit_p_2022.1.2.146.sh -a --components intel.oneapi.lin.mkl.devel --action install --eula accept -s
```

### Build
```bash
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 
```

## Windows build:

The Windows version has been tested with Enterprise editions of Visual Studio 2022, 2019 and 2017. It should work with the Community and Professional editions as well without any changes. 

**Prerequisites:**

* CMake 3.15+ (available in VisualStudio 2019+ or from https://cmake.org)
* NuGet.exe (install from https://www.nuget.org/downloads)
    * The build script will use NuGet to get MKL, OpenMP and Boost packages.
* DiskANN git repository checked out together with submodules. To check out submodules after git clone:
```
git submodule init
git submodule update
```

* Environment variables: 
    * [optional] If you would like to override the Boost library listed in windows/packages.config.in, set BOOST_ROOT to your Boost folder.

**Build steps:**
* Open the "x64 Native Tools Command Prompt for VS 2019" (or corresponding version) and change to DiskANN folder
* Create a "build" directory inside it
* Change to the "build" directory and run
```
cmake ..
```
OR for Visual Studio 2017 and earlier:
```
<full-path-to-installed-cmake>\cmake ..
```
**This will create a diskann.sln solution**. Now you can:

- Open it from VisualStudio and build either Release or Debug configuration.
- `<full-path-to-installed-cmake>\cmake --build build`
- Use MSBuild:
```
msbuild.exe diskann.sln /m /nologo /t:Build /p:Configuration="Release" /property:Platform="x64"
```

* This will also build gperftools submodule for libtcmalloc_minimal dependency.
* Generated binaries are stored in the x64/Release or x64/Debug directories.

## Usage:

Please see the following pages on using the compiled code:

- [Commandline interface for building and search SSD based indices](workflows/SSD_index.md)  
- [Commandline interface for building and search in memory indices](workflows/in_memory_index.md) 
- [Commandline examples for using in-memory streaming indices](workflows/dynamic_index.md)
- [Commandline interface for building and search in memory indices with label data and filters](workflows/filtered_in_memory.md)
- [Commandline interface for building and search SSD based indices with label data and filters](workflows/filtered_ssd_index.md)
- [diskannpy - DiskANN as a python extension module](python/README.md)

Please cite this software in your work as:

```
@misc{diskann-github,
   author = {Simhadri, Harsha Vardhan and Krishnaswamy, Ravishankar and Srinivasa, Gopal and Subramanya, Suhas Jayaram and Antonijevic, Andrija and Pryce, Dax and Kaczynski, David and Williams, Shane and Gollapudi, Siddarth and Sivashankar, Varun and Karia, Neel and Singh, Aditi and Jaiswal, Shikhar and Mahapatro, Neelam and Adams, Philip and Tower, Bryan and Patel, Yash}},
   title = {{DiskANN: Graph-structured Indices for Scalable, Fast, Fresh and Filtered Approximate Nearest Neighbor Search}},
   url = {https://github.com/Microsoft/DiskANN},
   version = {0.6.1},
   year = {2023}
}
```
