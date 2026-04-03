#include "cache/simple_cache.h"
#include <cstring>
#include <stdexcept>

SimpleCache::SimpleCache(mem::Memory* mem, size_t cache_size, size_t line_size, int miss_latency)
  : mem_(mem), cache_size_(cache_size), line_size_(line_size), miss_latency_(miss_latency) {
  if (line_size_ == 0 || (line_size_ & 3) != 0) throw std::invalid_argument("line_size must be multiple of 4");
  num_lines_ = cache_size_ / line_size_;
  if (num_lines_ == 0) num_lines_ = 1;
  lines_.resize(num_lines_);
}

std::pair<uint32_t, int> SimpleCache::load32(uint32_t addr) {
  uint32_t block = addr / line_size_;
  size_t idx = block % num_lines_;
  uint32_t tag = block;
  Line &line = lines_[idx];

  size_t offset = addr % line_size_;
  if (line.valid && line.tag == tag) {
    uint32_t val;
    std::memcpy(&val, &line.data[offset], 4);
    return {val, 0};
  }

  // miss: fetch block from memory
  line.data.assign(line_size_, 0);
  for (size_t off = 0; off < line_size_; off += 4) {
    uint32_t w = mem_->load32(static_cast<uint32_t>(block * line_size_ + off));
    std::memcpy(&line.data[off], &w, 4);
  }
  line.valid = true;
  line.tag = tag;

  uint32_t val;
  std::memcpy(&val, &line.data[offset], 4);
  return {val, miss_latency_};
}

int SimpleCache::store32(uint32_t addr, uint32_t value) {
  // write-through: write to memory immediately, update cache line if present
  mem_->store32(addr, value);
  uint32_t block = addr / line_size_;
  size_t idx = block % num_lines_;
  uint32_t tag = block;
  Line &line = lines_[idx];
  if (line.valid && line.tag == tag) {
    size_t offset = addr % line_size_;
    std::memcpy(&line.data[offset], &value, 4);
  }
  return 0;
}
