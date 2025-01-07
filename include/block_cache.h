#pragma once

#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <list> 

// Структура, представляющая страницу в кэше
struct CachePage {
  off_t offset;
  std::vector<char> data;
  bool modified;
  bool referenced;

  CachePage(off_t off, size_t block_size);
};

// Класс для работы с блочным кэшом
class BlockCache {
public:
  explicit BlockCache(size_t block_size, size_t max_cache_size);

  int open(const char *path);
  int close(int fd);
   ssize_t read(int fd, void *buf, size_t count);
   ssize_t write(int fd, const void *buf, size_t count);
   off_t lseek(int fd, off_t offset, int whence);
  int fsync(int fd);

private:
  size_t block_size_;                  // Размер блока
  size_t max_cache_size_;              // Максимальный размер кэша
  std::unordered_map<off_t, std::list<CachePage>::iterator> cache_map_;  // Для быстрого доступа к страницам
  std::list<CachePage> cache_;         // Для FIFO управления порядком
  std::unordered_map<int, off_t> fd_offsets_;  // Отслеживание смещений файлов
  std::unordered_map<int, int> file_descriptors_;  // Для хранения открытых файлов

  void evict_page();                  // Метод для вытеснения страницы
 };