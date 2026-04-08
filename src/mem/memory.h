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
  uint8_t load8(uint32_t addr);
  uint16_t load16(uint32_t addr);
  void store32(uint32_t addr, uint32_t value);
  void store8(uint32_t addr, uint8_t value);
  void store16(uint32_t addr, uint16_t value);
  // store raw bytes into memory (resizes backing store if necessary)
  void store_bytes(uint32_t addr, const uint8_t* buf, size_t len);
  // map a device at [base, base+size)
  void map_device(uint32_t base, uint32_t size, Device* dev);
  // current backing memory size in bytes
  size_t size() const;
  // whether [addr, addr+size) is fully inside an MMIO mapped device range
  bool is_mapped_device_range(uint32_t addr, uint32_t size) const;
  // last out-of-bounds access info (for crash reporting)
  uint32_t last_oob_addr() const;
  uint32_t last_oob_size() const;
  bool last_oob_is_write() const;

private:
  std::vector<uint8_t> data_;
  struct Mapping { uint32_t base; uint32_t size; Device* dev; };
  std::vector<Mapping> mappings_;
  uint32_t last_oob_addr_ = 0;
  uint32_t last_oob_size_ = 0;
  bool last_oob_is_write_ = false;
};

} // namespace mem
