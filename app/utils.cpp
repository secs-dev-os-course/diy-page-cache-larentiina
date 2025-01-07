#include "../include/block_cache.h"
#include "../include/utils.h"
#include <iostream>
#include <string>

BlockCache cache(8192, 512);

void read_file_with_cache(const std::string &file_path) {
  const size_t block_size = 512; 
  char buffer[block_size];

  std::cout << "[INFO] Начало чтения файла: " << file_path << std::endl;

  // Открытие файла через кэш
  int fd = cache.open(file_path.c_str());
  if (fd == -1) {
    std::cerr << "[ERROR] Ошибка открытия файла: " << file_path << std::endl;
    return;
  }
  std::cout << "[INFO] Файл успешно открыт через кэш. FD: " << fd << std::endl;

  ssize_t bytes_read;
  size_t total_bytes_read = 0;

  // Чтение файла блоками
  while ((bytes_read = cache.read(fd, buffer, block_size)) > 0) {
    total_bytes_read += bytes_read;

    // // Лог успешного чтения блока
    // std::cout << "[DEBUG] Прочитано блок: " << bytes_read
    //           << " байт. Всего прочитано: " << total_bytes_read << " байт."
    //           << std::endl;
  }

  if (bytes_read == 0) {
    std::cout << "[INFO] Достигнут конец файла." << std::endl;
  } else if (bytes_read == -1) {
    std::cerr << "[ERROR] Ошибка чтения файла: " << file_path << std::endl;
  }

  // Закрытие файла
  if (cache.close(fd) == -1) {
    std::cerr << "[ERROR] Ошибка закрытия файла: " << file_path << std::endl;
  } else {
    std::cout << "[INFO] Файл успешно закрыт: " << file_path << std::endl;
  }

  std::cout << "[INFO] Общее количество прочитанных байт: " << total_bytes_read
            << " байт." << std::endl;
}
