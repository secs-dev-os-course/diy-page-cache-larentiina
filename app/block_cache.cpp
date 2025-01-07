#include "../include/block_cache.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

CachePage::CachePage(off_t off, size_t block_size)
    : offset(off), data(block_size), modified(false), referenced(false) {}

BlockCache::BlockCache(size_t block_size, size_t max_cache_size)
    : block_size_(block_size), max_cache_size_(max_cache_size) {}

//открытие файла
int BlockCache::open(const char *path) {
  int fd = ::open(path, O_RDWR | O_DIRECT);
  if (fd == -1) {
    perror("Ошибка открытия файла");
    return -1;
  }
  file_descriptors_[fd] = fd;
  fd_offsets_[fd] = 0;
  return fd;
}

int BlockCache::close(int fd) {
  // Проверяем, что дескриптор файла существует
  if (file_descriptors_.find(fd) == file_descriptors_.end()) {
    std::cerr << "Ошибка: файл не открыт\n";
    return -1;
  }

  // Выполняем синхронизацию данных на диск (fsync)
  int ret = fsync(fd);
  if (ret == -1) {
    std::cerr << "Ошибка синхронизации данных при закрытии файла\n";
    return -1;
  }

  // Закрываем файл
  ret = ::close(fd);
  if (ret == -1) {
    perror("Ошибка закрытия файла");
    return -1;
  }

  // Удаляем информацию о файле из карт
  file_descriptors_.erase(fd);
  fd_offsets_.erase(fd);

  return 0;
}

ssize_t BlockCache::read(int fd, void *buf, size_t count) {
    if (file_descriptors_.find(fd) == file_descriptors_.end()) {
        std::cerr << "Ошибка: неверный дескриптор файла\n";
        return -1;
    }

    off_t offset = fd_offsets_[fd];
    size_t bytes_read = 0;

    while (bytes_read < count) {
        off_t block_offset = (offset / block_size_) * block_size_;
        size_t page_offset = offset % block_size_;
        size_t bytes_to_read = std::min(count - bytes_read, block_size_ - page_offset);

        auto it = cache_map_.find(block_offset);
        if (it != cache_map_.end()) {
            auto& page = *it->second;
            std::memcpy((char *)buf + bytes_read, page.data.data() + page_offset, bytes_to_read);
            page.referenced = true;
        } else {
            CachePage page(block_offset, block_size_);
            void *aligned_buf = aligned_alloc(4096, block_size_);
            if (!aligned_buf) {
                std::cerr << "Ошибка выделения памяти\n";
                return -1;
            }

            ssize_t ret = ::pread(fd, aligned_buf, block_size_, block_offset);
            if (ret == 0) break; // Конец файла
            if (ret == -1) {
                free(aligned_buf);
                perror("Ошибка чтения");
                return -1;
            }

            std::memcpy(page.data.data(), aligned_buf, block_size_);
            free(aligned_buf);

            if (cache_.size() >= max_cache_size_) {
                evict_page();
            }

            page.referenced = true;
            page.modified = false;

            cache_.push_back(std::move(page));
            cache_map_[block_offset] = std::prev(cache_.end());

            std::memcpy((char *)buf + bytes_read, cache_.back().data.data() + page_offset, bytes_to_read);
        }

        bytes_read += bytes_to_read;
        offset += bytes_to_read;
    }

    fd_offsets_[fd] = offset;
    return bytes_read;
}


// Метод для вытеснения страницы по принципу FIFO
void BlockCache::evict_page() {
  if (cache_.empty()) {
    return;
  }

  // Выталкиваем страницу из начала списка
  auto& page_to_evict = cache_.front();
  cache_map_.erase(page_to_evict.offset);  // Убираем её из карты

  cache_.pop_front();
}


ssize_t BlockCache::write(int fd, const void *buf, size_t count) {
  if (file_descriptors_.find(fd) == file_descriptors_.end()) {
    std::cerr << "Ошибка: неверный дескриптор файла\n";
    return -1;
  }

  off_t offset = fd_offsets_[fd];
  size_t bytes_written = 0;

  while (bytes_written < count) {
    // Рассчитываем смещение блока и смещение внутри блока
    off_t block_offset = (offset / block_size_) * block_size_;
    size_t page_offset = offset % block_size_;
    size_t bytes_to_write = std::min(count - bytes_written, block_size_ - page_offset);

    // Проверяем, есть ли страница с данными в кэше
    auto it = cache_map_.find(block_offset);
    if (it == cache_map_.end()) {
      // Если страницы нет в кэше, загружаем её с диска
      CachePage page(block_offset, block_size_);

      // Выделяем память и читаем данные с диска
      void *aligned_buf = aligned_alloc(4096, block_size_);
      if (!aligned_buf) {
        std::cerr << "Ошибка выделения памяти\n";
        return -1;
      }

      ssize_t ret = ::pread(fd, aligned_buf, block_size_, block_offset);
      if (ret == -1) {
        free(aligned_buf);
        perror("Ошибка чтения при записи");
        return -1;
      }

      std::memcpy(page.data.data(), aligned_buf, block_size_);
      free(aligned_buf);

      // Если кэш переполнен, выполняем вытеснение страницы
      if (cache_.size() >= max_cache_size_) {
        evict_page();
      }

      // Добавляем страницу в кэш
      page.referenced = true;
      page.modified = false;
      cache_.push_back(std::move(page));

      // Обновляем карту для быстрого доступа
      auto cache_it = --cache_.end();
      cache_map_[block_offset] = cache_it;
    }

    // Записываем данные в кэш
    auto& page = *(it == cache_map_.end() ? --cache_.end() : it->second);
    std::memcpy(page.data.data() + page_offset, (char *)buf + bytes_written, bytes_to_write);
    page.modified = true;

    // Обновляем количество записанных байтов и смещение
    bytes_written += bytes_to_write;
    offset += bytes_to_write;
  }

  // Обновляем смещение для файла
  fd_offsets_[fd] = offset;
  return bytes_written;
}



off_t BlockCache::lseek(int fd, off_t offset, int whence) {
  if (file_descriptors_.find(fd) == file_descriptors_.end()) {
    std::cerr << "Ошибка: неверный дескриптор файла\n";
    return -1;
  }

  off_t new_offset = ::lseek(fd, offset, whence);
  if (new_offset == -1) {
    perror("Ошибка lseek");
    return -1;
  }

  fd_offsets_[fd] = new_offset;
  return new_offset;
}

int BlockCache::fsync(int fd) {
  // Проверяем, что дескриптор файла валиден
  if (file_descriptors_.find(fd) == file_descriptors_.end()) {
    std::cerr << "Ошибка: неверный дескриптор файла\n";
    return -1;
  }

  // Проходим по всем страницам в кэше
  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    auto& page = *it;

    // Если страница была изменена, записываем её на диск
    if (page.modified) {
      ssize_t ret = ::pwrite(fd, page.data.data(), block_size_, page.offset);
      if (ret == -1) {
        perror("Ошибка записи при fsync");
        return -1;
      }
      // После успешной записи сбрасываем флаг изменения
      page.modified = false;
    }
  }

  // Успешное завершение операции
  return 0;
}


