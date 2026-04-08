#pragma once
#include <cstdint>
#include "mem/memory.h"
#include "cpu/csr.h"

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

private:
  mem::Memory* mem_;
  cpu::CSR* csr_;
  bool enabled_;
  bool verbose_ = false;
  bool debug_mmu_ = false;
  
};

} // namespace mmu
