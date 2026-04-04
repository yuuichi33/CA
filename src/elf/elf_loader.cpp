#include "elf/elf_loader.h"
#include "mem/memory.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstddef>
#include <algorithm>
#include <cstdint>

namespace elf {

static inline uint16_t read_u16_le(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static inline uint32_t read_u32_le(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool load_elf(const std::string& path, mem::Memory& mem, uint32_t& entry, LoadInfo* out) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return false;

  // read whole file
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  if (buf.size() < 52) return false; // too small for ELF32 header

  const uint8_t* e = buf.data();
  // check ELF magic
  if (!(e[0] == 0x7f && e[1] == 'E' && e[2] == 'L' && e[3] == 'F')) return false;
  // class must be ELF32 (1), data little endian (1)
  if (e[4] != 1 || e[5] != 1) return false;

  uint16_t e_ehsize = read_u16_le(e + 40);
  uint16_t e_phentsize = read_u16_le(e + 42);
  uint16_t e_phnum = read_u16_le(e + 44);
  uint32_t e_entry = read_u32_le(e + 24);
  uint32_t e_phoff = read_u32_le(e + 28);
  uint16_t e_type = read_u16_le(e + 16);
  const bool is_dso = (e_type == 3); // ET_DYN == 3

  if (e_phoff + (size_t)e_phentsize * e_phnum > buf.size()) return false;

  // choose a load base for ET_DYN to avoid enormous sparse allocations
  uint32_t base = 0;
  if (is_dso) {
    // align to page (4KiB) at or above current memory size
    const uint32_t page = 0x1000u;
    size_t cur = mem.size();
    uint32_t b = static_cast<uint32_t>((cur + page - 1) & ~(page - 1));
    if (b == 0) b = page; // ensure non-zero
    base = b;
  }

  // iterate program headers and load PT_LOAD segments (apply base for ET_DYN)
  for (uint16_t i = 0; i < e_phnum; ++i) {
    size_t phoff = e_phoff + i * e_phentsize;
    if (phoff + 32 > buf.size()) return false;
    const uint8_t* ph = buf.data() + phoff;
    uint32_t p_type = read_u32_le(ph + 0);
    uint32_t p_offset = read_u32_le(ph + 4);
    uint32_t p_vaddr = read_u32_le(ph + 8);
    uint32_t p_filesz = read_u32_le(ph + 16);
    uint32_t p_memsz = read_u32_le(ph + 20);

    if (p_type != 1) continue; // only PT_LOAD
    if (p_offset + p_filesz > buf.size()) return false;

    uint32_t dest = is_dso ? (base + p_vaddr) : p_vaddr;

    // write file content
    mem.store_bytes(dest, buf.data() + p_offset, p_filesz);
    // zero uninitialized portion
    if (p_memsz > p_filesz) {
      std::vector<uint8_t> zeros(p_memsz - p_filesz, 0);
      mem.store_bytes(dest + p_filesz, zeros.data(), zeros.size());
    }
  }

  // process dynamic relocations if any (DT_RELA/DT_REL) by scanning PT_DYNAMIC
  const uint32_t DT_NULL = 0;
  const uint32_t DT_RELA = 7;
  const uint32_t DT_RELASZ = 8;
  const uint32_t DT_RELAENT = 9;
  const uint32_t DT_REL = 17;
  const uint32_t DT_RELSZ = 18;
  const uint32_t DT_RELENT = 19;
  const uint32_t R_RISCV_RELATIVE = 3;

  for (uint16_t i = 0; i < e_phnum; ++i) {
    size_t phoff = e_phoff + i * e_phentsize;
    const uint8_t* ph = buf.data() + phoff;
    uint32_t p_type = read_u32_le(ph + 0);
    if (p_type != 2) continue; // PT_DYNAMIC
    uint32_t p_offset = read_u32_le(ph + 4);
    uint32_t p_vaddr = read_u32_le(ph + 8);
    uint32_t p_filesz = read_u32_le(ph + 16);
    if (p_offset + p_filesz > buf.size()) return false;

    // parse dynamic entries from file buffer
    uint32_t rela_addr = 0, rela_size = 0, rela_ent = 0;
    uint32_t rel_addr = 0, rel_size = 0, rel_ent = 0;
    for (uint32_t off = 0; off + 8 <= p_filesz; off += 8) {
      uint32_t d_tag = read_u32_le(buf.data() + p_offset + off + 0);
      uint32_t d_val = read_u32_le(buf.data() + p_offset + off + 4);
      if (d_tag == DT_NULL) break;
      if (d_tag == DT_RELA) rela_addr = d_val;
      else if (d_tag == DT_RELASZ) rela_size = d_val;
      else if (d_tag == DT_RELAENT) rela_ent = d_val;
      else if (d_tag == DT_REL) rel_addr = d_val;
      else if (d_tag == DT_RELSZ) rel_size = d_val;
      else if (d_tag == DT_RELENT) rel_ent = d_val;
    }

    // helper to get runtime address for dynamic pointers
    auto runtime_addr = [&](uint32_t a)->uint32_t { return is_dso ? (base + a) : a; };

    // handle RELA entries
    if (rela_addr != 0 && rela_size != 0) {
      uint32_t ent = rela_ent ? rela_ent : 12u; // default Elf32_Rela
      uint32_t n = rela_size / ent;
      uint32_t rtab = runtime_addr(rela_addr);
      for (uint32_t j = 0; j < n; ++j) {
        uint32_t off_addr = static_cast<uint32_t>(mem.load32(rtab + j * ent + 0));
        uint32_t info = static_cast<uint32_t>(mem.load32(rtab + j * ent + 4));
        int32_t addend = static_cast<int32_t>(mem.load32(rtab + j * ent + 8));
        uint32_t type = info & 0xffu;
        if (type == R_RISCV_RELATIVE) {
          uint32_t val = runtime_addr(static_cast<uint32_t>(addend));
          mem.store32(off_addr, val);
        }
      }
    }

    // handle REL entries
    if (rel_addr != 0 && rel_size != 0) {
      uint32_t ent = rel_ent ? rel_ent : 8u; // default Elf32_Rel
      uint32_t n = rel_size / ent;
      uint32_t rtab = runtime_addr(rel_addr);
      for (uint32_t j = 0; j < n; ++j) {
        uint32_t off_addr = static_cast<uint32_t>(mem.load32(rtab + j * ent + 0));
        uint32_t info = static_cast<uint32_t>(mem.load32(rtab + j * ent + 4));
        uint32_t type = info & 0xffu;
        if (type == R_RISCV_RELATIVE) {
          // addend taken from memory at target
          int32_t addend = static_cast<int32_t>(mem.load32(off_addr));
          uint32_t val = runtime_addr(static_cast<uint32_t>(addend));
          mem.store32(off_addr, val);
        }
      }
    }
  }

  entry = is_dso ? (base + e_entry) : e_entry;
  if (out) {
    out->entry = entry;
    out->phdr = is_dso ? (base + e_phoff) : e_phoff;
    out->phent = e_phentsize;
    out->phnum = e_phnum;
    out->base = base;
  }
  return true;
}

} // namespace elf
