cmake_minimum_required(VERSION 3.12)
project(BlockCache)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_SOURCES
    app/block_cache.cpp
    app/utils.cpp
)
set(TEST_SOURCES
    test/block_cache_test.cpp
)

set(BENCH_SOURCES
    app/bench.cpp
)
include_directories(include)

add_library(block_cache SHARED ${LIB_SOURCES})
target_include_directories(block_cache PUBLIC include)

add_executable(bench ${BENCH_SOURCES})
target_link_libraries(bench PRIVATE block_cache)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})


add_executable(block_cache_test ${TEST_SOURCES})
target_link_libraries(block_cache_test PRIVATE block_cache GTest::GTest GTest::Main pthread)
add_test(NAME BlockCacheTests COMMAND block_cache_test)