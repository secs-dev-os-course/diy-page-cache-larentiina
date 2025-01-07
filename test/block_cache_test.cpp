#include "../include/block_cache.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class BlockCacheTest : public ::testing::Test {
protected:
  const size_t block_size = 4096;
  const size_t max_cache_size = 4;
  const char *test_file = "test_file.bin";

  void SetUp() override {
    std::ofstream file(test_file, std::ios::binary);
    for (int i = 0; i < block_size * 10; ++i) {
      file.put(static_cast<char>(i % 256));
    }
    file.close();
  }

  void TearDown() override { std::filesystem::remove(test_file); }
};

TEST_F(BlockCacheTest, OpenCloseFile) {
  BlockCache cache(block_size, max_cache_size);
  int fd = cache.open(test_file);
  ASSERT_NE(fd, -1) << "Файл не открылся";
  EXPECT_EQ(cache.close(fd), 0) << "Файл не закрылся корректно";
}

TEST_F(BlockCacheTest, ReadFromFile) {
  BlockCache cache(block_size, max_cache_size);
  int fd = cache.open(test_file);
  ASSERT_NE(fd, -1);

  char buffer[block_size] = {0};
  ssize_t bytes_read = cache.read(fd, buffer, block_size);
  ASSERT_EQ(bytes_read, block_size);

  for (size_t i = 0; i < block_size; ++i) {
    ASSERT_EQ(buffer[i], static_cast<char>(i % 256));
  }

  EXPECT_EQ(cache.close(fd), 0);
}

TEST_F(BlockCacheTest, WriteToFile) {
  BlockCache cache(block_size, max_cache_size);
  int fd = cache.open(test_file);
  ASSERT_NE(fd, -1);

  char buffer[block_size];
  std::fill_n(buffer, block_size, 'A');

  ssize_t bytes_written = cache.write(fd, buffer, block_size);
  ASSERT_EQ(bytes_written, block_size);

  char read_back[block_size] = {0};
  cache.lseek(fd, 0, SEEK_SET);
  ssize_t bytes_read = cache.read(fd, read_back, block_size);
  ASSERT_EQ(bytes_read, block_size);

  for (size_t i = 0; i < block_size; ++i) {
    ASSERT_EQ(read_back[i], 'A');
  }

  EXPECT_EQ(cache.close(fd), 0);
}

TEST_F(BlockCacheTest, CacheEviction) {
  BlockCache cache(block_size, max_cache_size);
  int fd = cache.open(test_file);
  ASSERT_NE(fd, -1);

  char buffer[block_size];
  for (size_t i = 0; i < max_cache_size + 1; ++i) {
    cache.read(fd, buffer, block_size);
  }

  EXPECT_EQ(cache.close(fd), 0);
}

