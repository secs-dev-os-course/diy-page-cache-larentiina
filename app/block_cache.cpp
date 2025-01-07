#include "../include/block_cache.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

CachePage::CachePage(off_t off, size_t block_size, int file_descriptor)
    : offset(off), data(block_size), modified(false), fd(file_descriptor) {}

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

// Метод чтения данных
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
      auto &page = *it->second;
      std::memcpy((char *)buf + bytes_read, page.data.data() + page_offset, bytes_to_read);
    } else {
      CachePage page(block_offset, block_size_, fd);
      void *aligned_buf = aligned_alloc(4096, block_size_);
      if (!aligned_buf) {
        std::cerr << "Ошибка выделения памяти\n";
        return -1;
      }

      ssize_t ret = ::pread(fd, aligned_buf, block_size_, block_offset);
      if (ret == 0) break;  // Конец файла
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


// Метод записи данных
ssize_t BlockCache::write(int fd, const void *buf, size_t count) {
  if (file_descriptors_.find(fd) == file_descriptors_.end()) {
    std::cerr << "Ошибка: неверный дескриптор файла\n";
    return -1;
  }

  off_t offset = fd_offsets_[fd];
  size_t bytes_written = 0;

  while (bytes_written < count) {
    off_t block_offset = (offset / block_size_) * block_size_;
    size_t page_offset = offset % block_size_;
    size_t bytes_to_write = std::min(count - bytes_written, block_size_ - page_offset);

    auto it = cache_map_.find(block_offset);
    if (it == cache_map_.end()) {
      CachePage page(block_offset, block_size_, fd);

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

      if (cache_.size() >= max_cache_size_) {
        evict_page();
      }

      page.modified = false;
      cache_.push_back(std::move(page));
      cache_map_[block_offset] = --cache_.end();
    }

    auto &page = *(it == cache_map_.end() ? --cache_.end() : it->second);
    std::memcpy(page.data.data() + page_offset, (char *)buf + bytes_written, bytes_to_write);
    page.modified = true;

    bytes_written += bytes_to_write;
    offset += bytes_to_write;
  }

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

// Метод синхронизации страниц на диск
int BlockCache::fsync(int fd) {
  if (file_descriptors_.find(fd) == file_descriptors_.end()) {
    std::cerr << "Ошибка: неверный дескриптор файла\n";
    return -1;
  }

  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    auto &page = *it;
    if (page.modified && page.fd == fd) {
      ssize_t ret = ::pwrite(fd, page.data.data(), block_size_, page.offset);
      if (ret == -1) {
        perror("Ошибка записи при fsync");
        return -1;
      }
      page.modified = false;
    }
  }

  return 0;
}



