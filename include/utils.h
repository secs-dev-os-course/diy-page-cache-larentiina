#pragma once

#include <string>

// Функция для чтения файла блоками по 512 байт с использованием кэша
void read_file_with_cache(const std::string &file_path);
