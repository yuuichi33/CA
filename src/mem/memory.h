#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace mem {

class Device {
public:
  virtual uint32_t load32(uint32_t offset) = 0;
  virtual void store32(uint32_t offset, uint32_t value) = 0;
  virtual ~Device() = default;
};

class Memory {
public:
  explicit Memory(size_t sz = 64 * 1024);
  ~Memory();
  uint32_t load32(uint32_t addr);
  void store32(uint32_t addr, uint32_t value);
  // store raw bytes into memory (resizes backing store if necessary)
  void store_bytes(uint32_t addr, const uint8_t* buf, size_t len);
  // map a device at [base, base+size)
  void map_device(uint32_t base, uint32_t size, Device* dev);
  // current backing memory size in bytes
  size_t size() const;

private:
  std::vector<uint8_t> data_;
  struct Mapping { uint32_t base; uint32_t size; Device* dev; };
  std::vector<Mapping> mappings_;
};

} // namespace mem
