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

std::pair<uint32_t, int> SimpleCache::load8(uint32_t addr) {
  uint32_t block = addr / line_size_;
  size_t idx = block % num_lines_;
  uint32_t tag = block;
  Line &line = lines_[idx];

  size_t offset = addr % line_size_;
  if (line.valid && line.tag == tag) {
    return {static_cast<uint32_t>(line.data[offset]), 0};
  }

  // miss: fetch block from memory
  line.data.assign(line_size_, 0);
  for (size_t off = 0; off < line_size_; off += 4) {
    uint32_t w = mem_->load32(static_cast<uint32_t>(block * line_size_ + off));
    std::memcpy(&line.data[off], &w, 4);
  }
  line.valid = true;
  line.tag = tag;

  return {static_cast<uint32_t>(line.data[offset]), miss_latency_};
}

std::pair<uint32_t, int> SimpleCache::load16(uint32_t addr) {
  uint32_t block = addr / line_size_;
  size_t idx = block % num_lines_;
  uint32_t tag = block;
  Line &line = lines_[idx];

  size_t offset = addr % line_size_;
  if (line.valid && line.tag == tag && offset + 1 < line_size_) {
    uint16_t v;
    std::memcpy(&v, &line.data[offset], 2);
    return {static_cast<uint32_t>(v), 0};
  }

  // if crosses cache line, fall back to memory reads (may bypass cache)
  uint32_t aligned = addr & ~3u;
  uint32_t w = mem_->load32(aligned);
  uint32_t shift = (addr & 3u) * 8u;
  uint32_t low = (w >> shift) & 0xffu;
  uint32_t high;
  if ((addr & 3u) == 3u) {
    // need next word for the high byte
    uint32_t w2 = mem_->load32(aligned + 4);
    high = (w2 & 0xffu);
  } else {
    high = (w >> (shift + 8)) & 0xffu;
  }
  uint32_t val = (high << 8) | low;
  return {val, miss_latency_};
}

int SimpleCache::store8(uint32_t addr, uint8_t value) {
  // write-through
  mem_->store8(addr, value);
  uint32_t block = addr / line_size_;
  size_t idx = block % num_lines_;
  uint32_t tag = block;
  Line &line = lines_[idx];
  if (line.valid && line.tag == tag) {
    size_t offset = addr % line_size_;
    line.data[offset] = value;
  }
  return 0;
}

int SimpleCache::store16(uint32_t addr, uint16_t value) {
  mem_->store16(addr, value);
  uint32_t block = addr / line_size_;
  size_t idx = block % num_lines_;
  uint32_t tag = block;
  Line &line = lines_[idx];
  if (line.valid && line.tag == tag) {
    size_t offset = addr % line_size_;
    if (offset + 1 < line_size_) {
      std::memcpy(&line.data[offset], &value, 2);
    }
  }
  return 0;
}
