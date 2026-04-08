#include "mmu/mmu.h"
#include <stdexcept>
#include <iostream>

namespace mmu {

MMU::MMU(mem::Memory* mem, cpu::CSR* csr) : mem_(mem), csr_(csr), enabled_(false) {}

void MMU::set_verbose(bool v) { verbose_ = v; }
void MMU::set_debug_mmu(bool d) { debug_mmu_ = d; }

void MMU::set_enabled(bool en) { enabled_ = en; }
bool MMU::enabled() const { return enabled_; }

bool MMU::translate(uint32_t vaddr, AccessType atype, uint32_t &phys_out, uint32_t &cause_out) {
  // Read satp CSR (0x180) to determine mode and root PPN
  uint32_t satp = csr_->read(0x180);
  uint32_t mode = (satp >> 31) & 0x1u;
  if (!enabled_ || mode == 0u) {
    phys_out = vaddr; // identity mapping
    return true;
  }

  if (mode != 1u) {
    // unsupported mode -> page fault
    cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
    return false;
  }

  // Sv32: two-level page table
  uint32_t ppn = satp & 0x003fffffu; // 22-bit PPN
  uint32_t root = ppn << 12;
  uint32_t vpn[2];
  vpn[1] = (vaddr >> 22) & 0x3ffu;
  vpn[0] = (vaddr >> 12) & 0x3ffu;
  uint32_t offset = vaddr & 0xfffu;

  if (verbose_ || debug_mmu_) {
    std::cerr << "MMU: translate vaddr=0x" << std::hex << vaddr << std::dec << " mode=sv32 root=0x" << std::hex << root << std::dec << "\n";
  }

  for (int level = 1; level >= 0; --level) {
    uint32_t pte_addr = root + vpn[level] * 4u;
    uint32_t pte;
    try {
      pte = mem_->load32(pte_addr);
    } catch (const std::out_of_range&) {
      cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
      return false;
    }

    bool valid = (pte & 0x1u) != 0u;
    bool r = ((pte >> 1) & 0x1u) != 0u;
    bool w = ((pte >> 2) & 0x1u) != 0u;
    bool x = ((pte >> 3) & 0x1u) != 0u;
    bool u = ((pte >> 4) & 0x1u) != 0u;
    bool a = ((pte >> 6) & 0x1u) != 0u;
    bool d = ((pte >> 7) & 0x1u) != 0u;

    if (verbose_ || debug_mmu_) {
      std::cerr << "MMU: level=" << level << " pte_addr=0x" << std::hex << pte_addr << " pte=0x" << pte << std::dec
                << " V=" << valid << " R=" << r << " W=" << w << " X=" << x << " U=" << u << " A=" << a << " D=" << d << "\n";
    }

    if (!valid) {
      cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
      return false;
    }


    bool is_leaf = r || x;
    if (is_leaf) {
      if (level != 0) {
        // superpages not supported in this basic implementation
        cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
        return false;
      }

      // privilege checks: user cannot access kernel pages (U=0)
      int priv = csr_->get_privilege();
      if (priv == 0 && !u) {
        cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
        return false;
      }

      // permission checks: X for fetch, R for load, W for store
      if (atype == AccessType::Fetch && !x) {
        cause_out = 12u;
        return false;
      }
      if (atype == AccessType::Load && !r) {
        cause_out = 13u;
        return false;
      }
      if (atype == AccessType::Store && !w) {
        cause_out = 15u;
        return false;
      }


      // privilege/SUM: if current privilege is supervisor (S-mode) and page is user (U=1) and SUM not set, deny
      if (priv == 1 && u && !csr_->get_sum()) {
        // supervisor not permitted to access user page when SUM=0
        cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
        return false;
      }

      // update A/D bits
      uint32_t new_pte = pte;
      if (!a) new_pte |= (1u << 6);
      if (atype == AccessType::Store && !d) new_pte |= (1u << 7);
      if (new_pte != pte) {
        mem_->store32(pte_addr, new_pte);
        if (verbose_ || debug_mmu_) std::cerr << "MMU: updated PTE at 0x" << std::hex << pte_addr << " -> 0x" << new_pte << std::dec << "\n";
      }

      uint32_t ppn_pte = pte >> 10u;
      phys_out = (ppn_pte << 12u) | offset;
      return true;
    }

    // non-leaf: follow to next level
    uint32_t next_ppn = pte >> 10u;
    root = next_ppn << 12u;
  }

  cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
  return false;
}

} // namespace mmu
