#include "mem/memory.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

#include <algorithm>

namespace mem {

Memory::Memory(size_t sz) : data_(sz, 0) {}

uint32_t Memory::load32(uint32_t addr) {
  // check MMIO mappings first
  for (auto &m : mappings_) {
    if (addr >= m.base && addr + 4 <= m.base + m.size) {
      return m.dev->load32(addr - m.base);
    }
  }

  if (addr + 4 > data_.size()) {
    last_oob_addr_ = addr;
    last_oob_size_ = 4;
    last_oob_is_write_ = false;
    throw std::out_of_range("memory read out of bounds");
  }
  uint32_t val;
  std::memcpy(&val, &data_[addr], 4);
  return val;
}

uint8_t Memory::load8(uint32_t addr) {
  // check MMIO mappings first
  for (auto &m : mappings_) {
    if (addr >= m.base && addr < m.base + m.size) {
      uint32_t dev_off = addr - m.base;
      uint32_t aligned = dev_off & ~3u;
      uint32_t w = m.dev->load32(aligned);
      uint32_t shift = (dev_off & 3u) * 8u;
      return static_cast<uint8_t>((w >> shift) & 0xffu);
    }
  }
  if (addr + 1 > data_.size()) {
    last_oob_addr_ = addr;
    last_oob_size_ = 1;
    last_oob_is_write_ = false;
    throw std::out_of_range("memory read out of bounds");
  }
  return data_[addr];
}

uint16_t Memory::load16(uint32_t addr) {
  // check MMIO mappings first
  for (auto &m : mappings_) {
    if (addr >= m.base && addr + 2 <= m.base + m.size) {
      uint32_t dev_off = addr - m.base;
      uint32_t aligned = dev_off & ~3u;
      uint32_t w = m.dev->load32(aligned);
      uint32_t shift = (dev_off & 3u) * 8u;
      return static_cast<uint16_t>((w >> shift) & 0xffffu);
    }
  }
  if (addr + 2 > data_.size()) {
    last_oob_addr_ = addr;
    last_oob_size_ = 2;
    last_oob_is_write_ = false;
    throw std::out_of_range("memory read out of bounds");
  }
  uint16_t v;
  std::memcpy(&v, &data_[addr], 2);
  return v;
}

void Memory::store32(uint32_t addr, uint32_t value) {
  // check MMIO mappings first
  for (const auto &m : mappings_) {
    if (addr >= m.base && addr + 4 <= m.base + m.size) {
      // debug: report MMIO stores
      std::cerr << "Memory: MMIO store32 addr=0x" << std::hex << addr << " off=0x" << (addr - m.base) << " val=0x" << value << std::dec << "\n";
      m.dev->store32(addr - m.base, value);
      return;
    }
  }

  if (addr + 4 > data_.size()) {
    last_oob_addr_ = addr;
    last_oob_size_ = 4;
    last_oob_is_write_ = true;
    throw std::out_of_range("memory write out of bounds");
  }
  std::memcpy(&data_[addr], &value, 4);
}

void Memory::store8(uint32_t addr, uint8_t value) {
  for (const auto &m : mappings_) {
    if (addr >= m.base && addr < m.base + m.size) {
      uint32_t dev_off = addr - m.base;
      uint32_t aligned = dev_off & ~3u;
      uint32_t cur = m.dev->load32(aligned);
      uint32_t shift = (dev_off & 3u) * 8u;
      uint32_t mask = 0xffu << shift;
      uint32_t nw = (cur & ~mask) | (static_cast<uint32_t>(value) << shift);
      m.dev->store32(aligned, nw);
      return;
    }
  }
  if (addr + 1 > data_.size()) {
    last_oob_addr_ = addr;
    last_oob_size_ = 1;
    last_oob_is_write_ = true;
    throw std::out_of_range("memory write out of bounds");
  }
  data_[addr] = value;
}

void Memory::store16(uint32_t addr, uint16_t value) {
  for (const auto &m : mappings_) {
    if (addr >= m.base && addr + 2 <= m.base + m.size) {
      uint32_t dev_off = addr - m.base;
      uint32_t aligned = dev_off & ~3u;
      uint32_t cur = m.dev->load32(aligned);
      uint32_t shift = (dev_off & 3u) * 8u;
      uint32_t mask = 0xffffu << shift;
      uint32_t nw = (cur & ~mask) | (static_cast<uint32_t>(value) << shift);
      m.dev->store32(aligned, nw);
      return;
    }
  }
  if (addr + 2 > data_.size()) {
    last_oob_addr_ = addr;
    last_oob_size_ = 2;
    last_oob_is_write_ = true;
    throw std::out_of_range("memory write out of bounds");
  }
  std::memcpy(&data_[addr], &value, 2);
}

void Memory::store_bytes(uint32_t addr, const uint8_t* buf, size_t len) {
  // if any byte touches an MMIO mapping, abort (ELF loader shouldn't write MMIO)
  for (const auto &m : mappings_) {
    if (!(addr + len <= m.base || addr >= m.base + m.size)) {
      throw std::runtime_error("store_bytes would overlap MMIO mapping");
    }
  }

  // resize backing memory if necessary
  if (addr + len > data_.size()) data_.resize(addr + len, 0);
  std::memcpy(&data_[addr], buf, len);
}

void Memory::map_device(uint32_t base, uint32_t size, Device* dev) {
  // overwrite any existing mapping at same base
  mappings_.push_back(Mapping{base, size, dev});
  std::cerr << "Memory: map_device base=0x" << std::hex << base << " size=0x" << size << std::dec << "\n";
}

Memory::~Memory() {
  for (auto &m : mappings_) delete m.dev;
}

size_t Memory::size() const {
  return data_.size();
}

bool Memory::is_mapped_device_range(uint32_t addr, uint32_t size) const {
  for (const auto &m : mappings_) {
    if (addr >= m.base && addr + size <= m.base + m.size) return true;
  }
  return false;
}

uint32_t Memory::last_oob_addr() const {
  return last_oob_addr_;
}

uint32_t Memory::last_oob_size() const {
  return last_oob_size_;
}

bool Memory::last_oob_is_write() const {
  return last_oob_is_write_;
}

} // namespace mem
