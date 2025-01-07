#include "../include/utils.h"
#include <iostream>
#include <chrono>
#include <cstdlib>

int main(int argc, char *argv[]) {
    const std::string file_path = "benchmark_file.bin";
   
    int repeat_count = std::atoi(argv[1]);
    
    for (int i = 0; i < repeat_count; ++i) {

        auto start = std::chrono::high_resolution_clock::now();
        read_file_with_cache(file_path);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed_time = end - start;
        std::cout << "Время выполнения: " << elapsed_time.count() << " ms" << std::endl;
    }

    return 0;
}
