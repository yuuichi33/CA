#include "cache/simple_cache.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

// Construct using CacheConfig
SimpleCache::SimpleCache(mem::Memory* mem, const CacheConfig& cfg)
  : mem_(mem), cache_size_(cfg.cache_size), line_size_(cfg.line_size),
    associativity_(cfg.associativity == 0 ? 1 : cfg.associativity),
    miss_latency_(cfg.miss_latency), access_counter_(0) {
  if (line_size_ == 0 || (line_size_ & 3) != 0) throw std::invalid_argument("line_size must be multiple of 4");
  size_t total_lines = cache_size_ / line_size_;
  if (total_lines == 0) total_lines = 1;
  num_sets_ = total_lines / associativity_;
  if (num_sets_ == 0) num_sets_ = 1;
  sets_.assign(num_sets_, std::vector<Line>(associativity_));
  write_back_ = cfg.write_back;
  write_allocate_ = cfg.write_allocate;
}

SimpleCache::SimpleCache(mem::Memory* mem, size_t cache_size, size_t line_size, int miss_latency)
  : mem_(mem), cache_size_(cache_size), line_size_(line_size), associativity_(1), miss_latency_(miss_latency), access_counter_(0) {
  if (line_size_ == 0 || (line_size_ & 3) != 0) throw std::invalid_argument("line_size must be multiple of 4");
  size_t total_lines = cache_size_ / line_size_;
  if (total_lines == 0) total_lines = 1;
  num_sets_ = total_lines / associativity_;
  if (num_sets_ == 0) num_sets_ = 1;
  sets_.assign(num_sets_, std::vector<Line>(associativity_));
  write_back_ = false;
  write_allocate_ = false;
}

static inline uint32_t block_base_addr(uint32_t block, size_t line_size) {
  return static_cast<uint32_t>(block * line_size);
}

std::pair<uint32_t, int> SimpleCache::load32(uint32_t addr) {
  uint32_t block = addr / line_size_;
  size_t set_idx = block % num_sets_;
  uint32_t tag = block / num_sets_;
  auto &set = sets_[set_idx];

  size_t offset = addr % line_size_;
  // search for hit
  for (size_t way = 0; way < associativity_; ++way) {
    Line &line = set[way];
    if (line.valid && line.tag == tag) {
      uint32_t val;
      std::memcpy(&val, &line.data[offset], 4);
      line.lru = ++access_counter_;
      return {val, 0};
    }
  }

  // miss: choose victim (invalid preferred, else LRU)
  size_t victim = 0;
  bool found_invalid = false;
  for (size_t way = 0; way < associativity_; ++way) {
    if (!set[way].valid) { victim = way; found_invalid = true; break; }
  }
  if (!found_invalid) {
    uint32_t min_lru = set[0].lru;
    size_t min_i = 0;
    for (size_t way = 1; way < associativity_; ++way) {
      if (set[way].lru < min_lru) { min_lru = set[way].lru; min_i = way; }
    }
    victim = min_i;
    // TODO: handle write-back eviction when implemented
  }
  Line &line = set[victim];
  // if evicting a dirty line (write-back), write it back to memory first
  if (write_back_ && line.valid && line.dirty) {
    uint32_t victim_block = line.tag * static_cast<uint32_t>(num_sets_) + static_cast<uint32_t>(set_idx);
    uint32_t victim_base = block_base_addr(victim_block, line_size_);
    for (size_t off = 0; off < line_size_; off += 4) {
      uint32_t w;
      std::memcpy(&w, &line.data[off], 4);
      mem_->store32(static_cast<uint32_t>(victim_base + off), w);
    }
    line.dirty = false;
  }
  line.data.assign(line_size_, 0);
  uint32_t base = block_base_addr(block, line_size_);
  for (size_t off = 0; off < line_size_; off += 4) {
    uint32_t w = mem_->load32(static_cast<uint32_t>(base + off));
    std::memcpy(&line.data[off], &w, 4);
  }
  line.valid = true;
  line.tag = tag;
  line.lru = ++access_counter_;

  uint32_t val;
  std::memcpy(&val, &line.data[offset], 4);
  return {val, miss_latency_};
}

int SimpleCache::store32(uint32_t addr, uint32_t value) {
  if (write_back_) {
    // write-back mode: update cache line and mark dirty on hit; allocate on miss depending on write_allocate_
    uint32_t block = addr / line_size_;
    size_t set_idx = block % num_sets_;
    uint32_t tag = block / num_sets_;
    auto &set = sets_[set_idx];
    size_t offset = addr % line_size_;
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (line.valid && line.tag == tag) {
        std::memcpy(&line.data[offset], &value, 4);
        line.dirty = true;
        line.lru = ++access_counter_;
        return 0;
      }
    }
    // miss
    if (!write_allocate_) {
      // write-around: write directly to memory
      mem_->store32(addr, value);
      return 0;
    }
    // write-allocate: bring block into cache then write
    size_t victim = 0;
    bool found_invalid = false;
    for (size_t way = 0; way < associativity_; ++way) {
      if (!set[way].valid) { victim = way; found_invalid = true; break; }
    }
    if (!found_invalid) {
      uint32_t min_lru = set[0].lru;
      size_t min_i = 0;
      for (size_t way = 1; way < associativity_; ++way) {
        if (set[way].lru < min_lru) { min_lru = set[way].lru; min_i = way; }
      }
      victim = min_i;
    }
    Line &line = set[victim];
    if (line.valid && line.dirty) {
      uint32_t victim_block = line.tag * static_cast<uint32_t>(num_sets_) + static_cast<uint32_t>(set_idx);
      uint32_t victim_base = block_base_addr(victim_block, line_size_);
      for (size_t off = 0; off < line_size_; off += 4) {
        uint32_t w;
        std::memcpy(&w, &line.data[off], 4);
        mem_->store32(static_cast<uint32_t>(victim_base + off), w);
      }
      line.dirty = false;
    }
    // load block from memory
    line.data.assign(line_size_, 0);
    uint32_t base = block_base_addr(block, line_size_);
    for (size_t off = 0; off < line_size_; off += 4) {
      uint32_t w = mem_->load32(static_cast<uint32_t>(base + off));
      std::memcpy(&line.data[off], &w, 4);
    }
    line.valid = true;
    line.tag = tag;
    // write into cache and mark dirty
    std::memcpy(&line.data[offset], &value, 4);
    line.dirty = true;
    line.lru = ++access_counter_;
    return 0;
  } else {
    // write-through: write to memory immediately, update cache line if present
    mem_->store32(addr, value);
    uint32_t block = addr / line_size_;
    size_t set_idx = block % num_sets_;
    uint32_t tag = block / num_sets_;
    auto &set = sets_[set_idx];
    size_t offset = addr % line_size_;
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (line.valid && line.tag == tag) {
        std::memcpy(&line.data[offset], &value, 4);
        line.lru = ++access_counter_;
      }
    }
    return 0;
  }
}

std::pair<uint32_t, int> SimpleCache::load8(uint32_t addr) {
  uint32_t block = addr / line_size_;
  size_t set_idx = block % num_sets_;
  uint32_t tag = block / num_sets_;
  auto &set = sets_[set_idx];

  size_t offset = addr % line_size_;
  for (size_t way = 0; way < associativity_; ++way) {
    Line &line = set[way];
    if (line.valid && line.tag == tag) {
      line.lru = ++access_counter_;
      return {static_cast<uint32_t>(line.data[offset]), 0};
    }
  }

  // miss: fetch block
  size_t victim = 0;
  bool found_invalid = false;
  for (size_t way = 0; way < associativity_; ++way) {
    if (!set[way].valid) { victim = way; found_invalid = true; break; }
  }
  if (!found_invalid) {
    uint32_t min_lru = set[0].lru;
    size_t min_i = 0;
    for (size_t way = 1; way < associativity_; ++way) {
      if (set[way].lru < min_lru) { min_lru = set[way].lru; min_i = way; }
    }
    victim = min_i;
  }

  Line &line = set[victim];
  // evict if dirty
  if (write_back_ && line.valid && line.dirty) {
    uint32_t victim_block = line.tag * static_cast<uint32_t>(num_sets_) + static_cast<uint32_t>(set_idx);
    uint32_t victim_base = block_base_addr(victim_block, line_size_);
    for (size_t off = 0; off < line_size_; off += 4) {
      uint32_t w;
      std::memcpy(&w, &line.data[off], 4);
      mem_->store32(static_cast<uint32_t>(victim_base + off), w);
    }
    line.dirty = false;
  }
  line.data.assign(line_size_, 0);
  uint32_t base = block_base_addr(block, line_size_);
  for (size_t off = 0; off < line_size_; off += 4) {
    uint32_t w = mem_->load32(static_cast<uint32_t>(base + off));
    std::memcpy(&line.data[off], &w, 4);
  }
  line.valid = true;
  line.tag = tag;
  line.lru = ++access_counter_;

  return {static_cast<uint32_t>(line.data[offset]), miss_latency_};
}

std::pair<uint32_t, int> SimpleCache::load16(uint32_t addr) {
  uint32_t block = addr / line_size_;
  size_t set_idx = block % num_sets_;
  uint32_t tag = block / num_sets_;
  auto &set = sets_[set_idx];

  size_t offset = addr % line_size_;
  for (size_t way = 0; way < associativity_; ++way) {
    Line &line = set[way];
    if (line.valid && line.tag == tag && offset + 1 < line_size_) {
      uint16_t v;
      std::memcpy(&v, &line.data[offset], 2);
      line.lru = ++access_counter_;
      return {static_cast<uint32_t>(v), 0};
    }
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
  if (write_back_) {
    uint32_t block = addr / line_size_;
    size_t set_idx = block % num_sets_;
    uint32_t tag = block / num_sets_;
    auto &set = sets_[set_idx];
    size_t offset = addr % line_size_;
    // hit?
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (line.valid && line.tag == tag) {
        line.data[offset] = value;
        line.dirty = true;
        line.lru = ++access_counter_;
        return 0;
      }
    }
    // miss
    if (!write_allocate_) {
      // write-around: write directly to memory
      mem_->store8(addr, value);
      return 0;
    }
    // write-allocate: bring block into cache then write
    size_t victim = 0;
    bool found_invalid = false;
    for (size_t way = 0; way < associativity_; ++way) {
      if (!set[way].valid) { victim = way; found_invalid = true; break; }
    }
    if (!found_invalid) {
      uint32_t min_lru = set[0].lru;
      size_t min_i = 0;
      for (size_t way = 1; way < associativity_; ++way) {
        if (set[way].lru < min_lru) { min_lru = set[way].lru; min_i = way; }
      }
      victim = min_i;
    }
    Line &line = set[victim];
    if (line.valid && line.dirty) {
      uint32_t victim_block = line.tag * static_cast<uint32_t>(num_sets_) + static_cast<uint32_t>(set_idx);
      uint32_t victim_base = block_base_addr(victim_block, line_size_);
      for (size_t off = 0; off < line_size_; off += 4) {
        uint32_t w;
        std::memcpy(&w, &line.data[off], 4);
        mem_->store32(static_cast<uint32_t>(victim_base + off), w);
      }
      line.dirty = false;
    }
    // load block
    line.data.assign(line_size_, 0);
    uint32_t base = block_base_addr(block, line_size_);
    for (size_t off = 0; off < line_size_; off += 4) {
      uint32_t w = mem_->load32(static_cast<uint32_t>(base + off));
      std::memcpy(&line.data[off], &w, 4);
    }
    line.valid = true;
    line.tag = tag;
    line.data[offset] = value;
    line.dirty = true;
    line.lru = ++access_counter_;
    return 0;
  } else {
    // write-through
    mem_->store8(addr, value);
    uint32_t block = addr / line_size_;
    size_t set_idx = block % num_sets_;
    uint32_t tag = block / num_sets_;
    auto &set = sets_[set_idx];
    size_t offset = addr % line_size_;
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (line.valid && line.tag == tag) {
        line.data[offset] = value;
        line.lru = ++access_counter_;
        break;
      }
    }
    return 0;
  }
}

int SimpleCache::store16(uint32_t addr, uint16_t value) {
  if (write_back_) {
    uint32_t block = addr / line_size_;
    size_t set_idx = block % num_sets_;
    uint32_t tag = block / num_sets_;
    auto &set = sets_[set_idx];
    size_t offset = addr % line_size_;
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (line.valid && line.tag == tag) {
        if (offset + 1 < line_size_) {
          std::memcpy(&line.data[offset], &value, 2);
        }
        line.dirty = true;
        line.lru = ++access_counter_;
        return 0;
      }
    }
    // miss
    if (!write_allocate_) {
      mem_->store16(addr, value);
      return 0;
    }
    // write-allocate
    size_t victim = 0;
    bool found_invalid = false;
    for (size_t way = 0; way < associativity_; ++way) {
      if (!set[way].valid) { victim = way; found_invalid = true; break; }
    }
    if (!found_invalid) {
      uint32_t min_lru = set[0].lru;
      size_t min_i = 0;
      for (size_t way = 1; way < associativity_; ++way) {
        if (set[way].lru < min_lru) { min_lru = set[way].lru; min_i = way; }
      }
      victim = min_i;
    }
    Line &line = set[victim];
    if (line.valid && line.dirty) {
      uint32_t victim_block = line.tag * static_cast<uint32_t>(num_sets_) + static_cast<uint32_t>(set_idx);
      uint32_t victim_base = block_base_addr(victim_block, line_size_);
      for (size_t off = 0; off < line_size_; off += 4) {
        uint32_t w;
        std::memcpy(&w, &line.data[off], 4);
        mem_->store32(static_cast<uint32_t>(victim_base + off), w);
      }
      line.dirty = false;
    }
    line.data.assign(line_size_, 0);
    uint32_t base = block_base_addr(block, line_size_);
    for (size_t off = 0; off < line_size_; off += 4) {
      uint32_t w = mem_->load32(static_cast<uint32_t>(base + off));
      std::memcpy(&line.data[off], &w, 4);
    }
    line.valid = true;
    line.tag = tag;
    if (offset + 1 < line_size_) {
      std::memcpy(&line.data[offset], &value, 2);
    }
    line.dirty = true;
    line.lru = ++access_counter_;
    return 0;
  } else {
    mem_->store16(addr, value);
    uint32_t block = addr / line_size_;
    size_t set_idx = block % num_sets_;
    uint32_t tag = block / num_sets_;
    auto &set = sets_[set_idx];
    size_t offset = addr % line_size_;
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (line.valid && line.tag == tag) {
        if (offset + 1 < line_size_) {
          std::memcpy(&line.data[offset], &value, 2);
        }
        line.lru = ++access_counter_;
        break;
      }
    }
    return 0;
  }
}

void SimpleCache::flush_all() {
  for (size_t set_idx = 0; set_idx < num_sets_; ++set_idx) {
    auto &set = sets_[set_idx];
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      if (write_back_ && line.valid && line.dirty) {
        uint32_t victim_block = line.tag * static_cast<uint32_t>(num_sets_) + static_cast<uint32_t>(set_idx);
        uint32_t victim_base = block_base_addr(victim_block, line_size_);
        for (size_t off = 0; off < line_size_; off += 4) {
          uint32_t w;
          std::memcpy(&w, &line.data[off], 4);
          mem_->store32(static_cast<uint32_t>(victim_base + off), w);
        }
        line.dirty = false;
      }
      line.valid = false;
    }
  }
}

void SimpleCache::invalidate_all() {
  for (size_t set_idx = 0; set_idx < num_sets_; ++set_idx) {
    auto &set = sets_[set_idx];
    for (size_t way = 0; way < associativity_; ++way) {
      Line &line = set[way];
      line.valid = false;
      line.dirty = false;
    }
  }
}
