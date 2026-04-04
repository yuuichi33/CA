#include <iostream>
#include <fstream>
#include <vector>
#include "elf/elf_loader.h"
#include "mem/memory.h"

static void write_minimal_elf(const std::string &path, uint32_t instr_word) {
  // construct minimal ELF32 with one PT_LOAD at vaddr 0
  std::vector<uint8_t> buf;
  buf.resize(84 + 4); // header(52)+ph(32)+segment(4)
  // e_ident
  buf[0] = 0x7f; buf[1] = 'E'; buf[2] = 'L'; buf[3] = 'F';
  buf[4] = 1; // ELFCLASS32
  buf[5] = 1; // little endian
  buf[6] = 1; // version
  // e_type (ET_EXEC=2)
  buf[16] = 2; buf[17] = 0;
  // e_machine = EM_RISCV (243 = 0xF3)
  buf[18] = 0xF3; buf[19] = 0;
  // e_version
  buf[20] = 1; buf[21] = 0; buf[22] = 0; buf[23] = 0;
  // e_entry = 0
  buf[24] = 0; buf[25] = 0; buf[26] = 0; buf[27] = 0;
  // e_phoff = 52
  buf[28] = 52; buf[29] = 0; buf[30] = 0; buf[31] = 0;
  // e_shoff = 0
  // e_flags = 0
  // e_ehsize = 52
  buf[40] = 52; buf[41] = 0;
  // e_phentsize = 32
  buf[42] = 32; buf[43] = 0;
  // e_phnum = 1
  buf[44] = 1; buf[45] = 0;

  // program header at offset 52
  size_t ph = 52;
  // p_type = PT_LOAD (1)
  buf[ph + 0] = 1; buf[ph + 1] = 0; buf[ph + 2] = 0; buf[ph + 3] = 0;
  // p_offset = 84 (52+32)
  uint32_t p_offset = 84;
  buf[ph + 4] = static_cast<uint8_t>(p_offset & 0xff);
  buf[ph + 5] = static_cast<uint8_t>((p_offset >> 8) & 0xff);
  buf[ph + 6] = static_cast<uint8_t>((p_offset >> 16) & 0xff);
  buf[ph + 7] = static_cast<uint8_t>((p_offset >> 24) & 0xff);
  // p_vaddr = 0
  // p_paddr = 0
  // p_filesz = 4
  buf[ph + 16] = 4; buf[ph + 17] = 0; buf[ph + 18] = 0; buf[ph + 19] = 0;
  // p_memsz = 4
  buf[ph + 20] = 4; buf[ph + 21] = 0; buf[ph + 22] = 0; buf[ph + 23] = 0;

  // segment data at offset 84
  size_t seg = p_offset;
  buf[seg + 0] = static_cast<uint8_t>(instr_word & 0xff);
  buf[seg + 1] = static_cast<uint8_t>((instr_word >> 8) & 0xff);
  buf[seg + 2] = static_cast<uint8_t>((instr_word >> 16) & 0xff);
  buf[seg + 3] = static_cast<uint8_t>((instr_word >> 24) & 0xff);

  std::ofstream ofs(path, std::ios::binary);
  ofs.write(reinterpret_cast<const char*>(buf.data()), buf.size());
}

int main() {
  std::string path = "./tmp_min_elf.bin";
  // instruction: ADDI x1,x0,1 = 0x00100093
  uint32_t ins = 0x00100093u;
  write_minimal_elf(path, ins);

  mem::Memory m(64*1024);
  uint32_t entry = 0xFFFFFFFFu;
  if (!elf::load_elf(path, m, entry)) {
    std::cerr << "elf load failed\n";
    return 1;
  }

  // entry should be 0
  if (entry != 0) {
    std::cerr << "unexpected entry=" << entry << "\n";
    return 2;
  }

  uint32_t w = m.load32(0);
  if (w != ins) {
    std::cerr << "word mismatch: got " << std::hex << w << " expected " << ins << "\n";
    return 3;
  }

  std::cout << "elf loader ok\n";
  return 0;
}
