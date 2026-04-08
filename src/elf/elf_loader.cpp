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
    // Try to locate semihosting symbols (tohost/fromhost) by scanning section symbol tables
    out->tohost = 0;
    out->fromhost = 0;
    // section header table
    uint32_t e_shoff = read_u32_le(e + 32);
    uint16_t e_shentsize = read_u16_le(e + 46);
    uint16_t e_shnum = read_u16_le(e + 48);
    uint16_t e_shstrndx = read_u16_le(e + 50);
    if (e_shoff != 0 && e_shnum != 0 && e_shoff + (size_t)e_shentsize * e_shnum <= buf.size() && e_shstrndx < e_shnum) {
      const uint8_t* shdrs = buf.data() + e_shoff;
      // read section header string table
      const uint8_t* shstr_sh = shdrs + (size_t)e_shentsize * e_shstrndx;
      uint32_t shstr_off = read_u32_le(shstr_sh + 16);
      uint32_t shstr_size = read_u32_le(shstr_sh + 20);
      if (shstr_off + shstr_size <= buf.size()) {
        const uint8_t* shstr = buf.data() + shstr_off;
        // iterate sections to find symbol tables
        for (unsigned si = 0; si < e_shnum; ++si) {
          const uint8_t* sh = shdrs + (size_t)e_shentsize * si;
          uint32_t sh_name = read_u32_le(sh + 0);
          uint32_t sh_type = read_u32_le(sh + 4);
          uint32_t sh_offset = read_u32_le(sh + 16);
          uint32_t sh_size = read_u32_le(sh + 20);
          uint32_t sh_link = read_u32_le(sh + 24);
          uint32_t sh_entsize = read_u32_le(sh + 36);
          const char* secname = (sh_name < shstr_size) ? reinterpret_cast<const char*>(shstr + sh_name) : "";
          if (sh_type == 2u || sh_type == 11u) { // SHT_SYMTAB=2 or SHT_DYNSYM=11
            // string table for this symtab is at section index sh_link
            if (sh_link >= e_shnum) continue;
            const uint8_t* str_sh = shdrs + (size_t)e_shentsize * sh_link;
            uint32_t str_off = read_u32_le(str_sh + 16);
            uint32_t str_size = read_u32_le(str_sh + 20);
            if (sh_offset + sh_size > buf.size() || str_off + str_size > buf.size()) continue;
            const uint8_t* symtab = buf.data() + sh_offset;
            const uint8_t* strtab = buf.data() + str_off;
            uint32_t nents = sh_entsize ? static_cast<uint32_t>(sh_size / sh_entsize) : 0u;
            for (uint32_t j = 0; j < nents; ++j) {
              const uint8_t* sym = symtab + (size_t)j * sh_entsize;
              if ((size_t)(sym + 16 - buf.data()) > buf.size()) break;
              uint32_t st_name = read_u32_le(sym + 0);
              uint32_t st_value = read_u32_le(sym + 4);
              // st_size at +8 (ignored), st_info at +12
              const char* sname = (st_name < str_size) ? reinterpret_cast<const char*>(strtab + st_name) : "";
              if (sname && sname[0]) {
                std::string name(sname);
                if (name == "tohost") {
                  out->tohost = is_dso ? (base + st_value) : st_value;
                } else if (name == "fromhost") {
                  out->fromhost = is_dso ? (base + st_value) : st_value;
                } else if (out->test_entry == 0 && name.rfind("test_", 0) == 0) {
                  // pick up a test entry symbol if present (riscv-tests stubs use test_2 etc)
                  out->test_entry = is_dso ? (base + st_value) : st_value;
                }
                if (out->tohost != 0 && out->fromhost != 0 && out->test_entry != 0) break;
              }
            }
            if (out->tohost != 0 && out->fromhost != 0) break;
          }
        }
      }
    }
  }
  return true;
}

} // namespace elf
