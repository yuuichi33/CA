#pragma once
#include <cstdint>
#include "mem/memory.h"
#include "cpu/csr.h"
#include <array>

namespace mmu {

enum class AccessType { Fetch, Load, Store };

class MMU {
public:
  MMU(mem::Memory* mem, cpu::CSR* csr);
  // translate virtual address -> physical address.
  // returns true on success and sets phys_out; on failure returns false and sets cause_out (mcause code).
  bool translate(uint32_t vaddr, AccessType atype, uint32_t &phys_out, uint32_t &cause_out);
  void set_enabled(bool en);
  bool enabled() const;
  // debug / verbose controls
  void set_verbose(bool v);
  void set_debug_mmu(bool d);
  // flush TLB entries
  void flush_tlb();
  // perform SFENCE.VMA: vaddr (rs1), asid (rs2) semantics
  void sfence_vma(uint32_t vaddr, uint32_t asid);
  // debug: return number of valid TLB entries
  unsigned tlb_count() const;

private:
  mem::Memory* mem_;
  cpu::CSR* csr_;
  bool enabled_;
  bool verbose_ = false;
  bool debug_mmu_ = false;
  // simple 4-entry fully-associative TLB
  struct TLBE {
    bool valid = false;
    uint32_t vpn = 0;    // virtual page number (vaddr >> 12)
    uint32_t ppn = 0;    // physical page number from PTE
    uint32_t pte_raw = 0; // cached PTE bits
    uint32_t pte_addr = 0; // physical address of PTE in memory
    uint32_t asid = 0;    // ASID at time of insertion (sv32: bits [30:22] of satp)
  };
  std::array<TLBE, 4> tlb_;
  unsigned tlb_victim_idx_ = 0;
  uint32_t last_satp_ = 0;
  
};

} // namespace mmu
