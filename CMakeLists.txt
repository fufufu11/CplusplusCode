cmake_minimum_required(VERSION 3.14)
project(DistributedKV LANGUAGES CXX)

# --- 全局设置 ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- 编译选项优化 ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
    add_compile_options(/W4 /Zc:__cplusplus /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2 -g)
endif()

# --- Google Test 接入 (使用固定版本以提高稳定性) ---
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://ghproxy.net/https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# 对于 Windows 环境，防止 GTest 修改运行时库选择 (MT/MD)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- 核心库 ---
# 建议将核心逻辑定义为库，方便主程序和测试共用
add_library(DistributedKV_lib INTERFACE)
target_include_directories(DistributedKV_lib INTERFACE ${PROJECT_SOURCE_DIR}/include)

# --- SSTable Reader 实现 ---
add_library(sstable_reader_lib STATIC src/sstable_reader.cpp)
target_link_libraries(sstable_reader_lib PUBLIC DistributedKV_lib)

# --- 可执行程序 ---
add_executable(DistributedKV_bin src/main.cpp)
target_link_libraries(DistributedKV_bin PRIVATE sstable_reader_lib)

# --- 示例 / 基准测试 ---
add_executable(benchmark_skiplist examples/benchmark_skiplist.cpp)
target_link_libraries(benchmark_skiplist PRIVATE DistributedKV_lib)

add_executable(sstable_demo examples/sstable_demo.cpp)
target_link_libraries(sstable_demo PRIVATE sstable_reader_lib)

# --- 单元测试 ---
add_executable(skiplist_test tests/skiplist_test.cpp)
target_link_libraries(skiplist_test PRIVATE DistributedKV_lib GTest::gtest_main)

add_executable(kv_store_test tests/kv_store_test.cpp)
target_link_libraries(kv_store_test PRIVATE sstable_reader_lib GTest::gtest_main)

add_executable(wal_record_test tests/wal_record_test.cpp)
target_link_libraries(wal_record_test PRIVATE DistributedKV_lib GTest::gtest_main)

add_executable(sstable_builder_test tests/sstable_builder_test.cpp)
target_link_libraries(sstable_builder_test PRIVATE DistributedKV_lib GTest::gtest_main)

add_executable(sstable_reader_test tests/sstable_reader_test.cpp)
target_link_libraries(sstable_reader_test PRIVATE sstable_reader_lib GTest::gtest_main)

add_executable(flush_integration_test tests/flush_integration_test.cpp)
target_link_libraries(flush_integration_test PRIVATE sstable_reader_lib GTest::gtest_main)

add_executable(compaction_merger_test tests/compaction_merger_test.cpp)
target_link_libraries(compaction_merger_test PRIVATE sstable_reader_lib GTest::gtest_main)

# --- Raft 模块测试 ---
add_executable(raft_test tests/raft_test.cc)
target_include_directories(raft_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(raft_test PRIVATE DistributedKV_lib GTest::gtest_main)

# 让 ctest 能够识别到此测试
include(GoogleTest)
gtest_discover_tests(skiplist_test)
gtest_discover_tests(kv_store_test)
gtest_discover_tests(wal_record_test)
gtest_discover_tests(sstable_builder_test)
gtest_discover_tests(sstable_reader_test)
gtest_discover_tests(flush_integration_test)
gtest_discover_tests(compaction_merger_test)
gtest_discover_tests(raft_test)

# --- 打印状态 ---
message(STATUS "--------------------------------------------")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "--------------------------------------------")
