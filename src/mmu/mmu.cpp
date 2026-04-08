#include "mmu/mmu.h"
#include <stdexcept>
#include <iostream>

namespace mmu {

MMU::MMU(mem::Memory* mem, cpu::CSR* csr) : mem_(mem), csr_(csr), enabled_(false) {
  // init
  last_satp_ = 0;
  tlb_victim_idx_ = 0;
  for (auto &e : tlb_) e.valid = false;
}

void MMU::set_verbose(bool v) { verbose_ = v; }
void MMU::set_debug_mmu(bool d) { debug_mmu_ = d; }

void MMU::set_enabled(bool en) {
  if (enabled_ != en) {
    enabled_ = en;
    // flush TLB on enable/disable transition
    flush_tlb();
  } else {
    enabled_ = en;
  }
}
bool MMU::enabled() const { return enabled_; }

void MMU::flush_tlb() {
  for (auto &e : tlb_) e.valid = false;
  tlb_victim_idx_ = 0;
  last_satp_ = 0;
  if (verbose_ || debug_mmu_) std::cerr << "MMU: TLB flushed\n";
}

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
  uint32_t asid_cur = (satp >> 22) & 0x1ffu;
  uint32_t vpn[2];
  vpn[1] = (vaddr >> 22) & 0x3ffu;
  vpn[0] = (vaddr >> 12) & 0x3ffu;
  uint32_t offset = vaddr & 0xfffu;

  // if SATP changed since last translate, flush TLB
  if (satp != last_satp_) {
    flush_tlb();
    last_satp_ = satp;
  }

  uint32_t vpage = vaddr >> 12u;
  // TLB lookup (only when sv32 enabled)
  for (auto &e : tlb_) {
    if (!e.valid) continue;
    // require matching VPN and ASID
    if (e.vpn != vpage) continue;
    if (e.asid != asid_cur) continue;
    // validate and reload current PTE
    uint32_t cur_pte = 0;
    try {
      cur_pte = mem_->load32(e.pte_addr);
    } catch (...) {
      // treat as miss and fall through to walk
      break;
    }
    if ((cur_pte & 0x1u) == 0u) {
      // invalid PTE now; invalidate TLB entry
      e.valid = false;
      break;
    }
    bool r = ((cur_pte >> 1) & 0x1u) != 0u;
    bool w = ((cur_pte >> 2) & 0x1u) != 0u;
    bool x = ((cur_pte >> 3) & 0x1u) != 0u;
    bool u = ((cur_pte >> 4) & 0x1u) != 0u;
    bool a = ((cur_pte >> 6) & 0x1u) != 0u;
    bool d = ((cur_pte >> 7) & 0x1u) != 0u;

    int priv = csr_->get_privilege();
    if (priv == 0 && !u) { cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u); return false; }
    if (atype == AccessType::Fetch && !x) { cause_out = 12u; return false; }
    if (atype == AccessType::Load && !r) { cause_out = 13u; return false; }
    if (atype == AccessType::Store && !w) { cause_out = 15u; return false; }
    if (priv == 1 && u && !csr_->get_sum()) { cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u); return false; }

    // update A/D bits if necessary
    uint32_t new_pte = cur_pte;
    if (!a) new_pte |= (1u << 6);
    if (atype == AccessType::Store && !d) new_pte |= (1u << 7);
    if (new_pte != cur_pte) {
      mem_->store32(e.pte_addr, new_pte);
      e.pte_raw = new_pte;
      if (verbose_ || debug_mmu_) std::cerr << "MMU: updated PTE at 0x" << std::hex << e.pte_addr << " -> 0x" << new_pte << std::dec << " (via TLB)\n";
    }

    uint32_t ppn_pte = (new_pte >> 10u);
    phys_out = (ppn_pte << 12u) | offset;
    if (verbose_ || debug_mmu_) std::cerr << "MMU: TLB hit vpn=0x" << std::hex << vpage << " -> phys=0x" << phys_out << std::dec << "\n";
    return true;
  }

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

      uint32_t ppn_pte = (new_pte >> 10u);
      phys_out = (ppn_pte << 12u) | offset;
      // insert into TLB (simple round-robin replacement)
      {
        auto &ent = tlb_[tlb_victim_idx_ % tlb_.size()];
        ent.valid = true;
        ent.vpn = vaddr >> 12u;
        ent.ppn = ppn_pte;
        ent.pte_raw = new_pte;
        ent.pte_addr = pte_addr;
        ent.asid = asid_cur;
        tlb_victim_idx_ = (tlb_victim_idx_ + 1) % tlb_.size();
        if (verbose_ || debug_mmu_) std::cerr << "MMU: TLB insert vpn=0x" << std::hex << ent.vpn << " -> ppn=0x" << ent.ppn << "\n" << std::dec;
      }
      return true;
    }

    // non-leaf: follow to next level
    uint32_t next_ppn = pte >> 10u;
    root = next_ppn << 12u;
  }

  cause_out = (atype == AccessType::Fetch) ? 12u : (atype == AccessType::Load ? 13u : 15u);
  return false;
}

void MMU::sfence_vma(uint32_t vaddr, uint32_t asid) {
  // Implement SFENCE.VMA semantics:
  // - if vaddr==0 && asid==0 => flush all TLB entries
  // - if vaddr==0 && asid!=0 => invalidate entries matching ASID
  // - if vaddr!=0 && asid==0 => invalidate entries matching VPN (all ASIDs)
  // - if vaddr!=0 && asid!=0 => invalidate entries matching VPN and ASID
  if (!enabled_) {
    if (verbose_ || debug_mmu_) std::cerr << "MMU: sfence_vma no-op (MMU disabled)\n";
    return;
  }
  if (vaddr == 0 && asid == 0) {
    flush_tlb();
    return;
  }
  uint32_t vpage = vaddr >> 12u;
  for (auto &e : tlb_) {
    if (!e.valid) continue;
    if (vaddr == 0) {
      // invalidate by ASID only
      if (asid != 0 && e.asid == asid) {
        if (verbose_ || debug_mmu_) std::cerr << "MMU: sfence invalidate asid=0x" << std::hex << asid << " vpn=0x" << e.vpn << std::dec << "\n";
        e.valid = false;
      }
    } else {
      // invalidate by VPN (and optionally ASID)
      if (e.vpn != vpage) continue;
      if (asid == 0) {
        if (verbose_ || debug_mmu_) std::cerr << "MMU: sfence invalidate vpn=0x" << std::hex << vpage << std::dec << "\n";
        e.valid = false;
      } else {
        if (e.asid == asid) {
          if (verbose_ || debug_mmu_) std::cerr << "MMU: sfence invalidate vpn=0x" << std::hex << vpage << " asid=0x" << asid << std::dec << "\n";
          e.valid = false;
        }
      }
    }
  }
}

unsigned MMU::tlb_count() const {
  unsigned cnt = 0;
  for (const auto &e : tlb_) if (e.valid) ++cnt;
  return cnt;
}

} // namespace mmu
