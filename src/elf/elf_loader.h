#pragma once
#include <string>
#include <cstdint>
namespace mem { class Memory; }

namespace elf {

// Result information returned by ELF loader (optional)
struct LoadInfo {
	uint32_t entry = 0;
	// runtime address of program header table (if available)
	uint32_t phdr = 0;
	uint32_t phent = 0;
	uint32_t phnum = 0;
	// load base for ET_DYN (0 for ET_EXEC)
	uint32_t base = 0;
};

// Load a 32-bit little-endian ELF file into `mem`.
// On success, sets `entry` to ELF entry point and returns true.
// If `out` is non-null, fills additional loader metadata (phdr/phnum/etc).
bool load_elf(const std::string& path, mem::Memory& mem, uint32_t& entry, LoadInfo* out = nullptr);

} // namespace elf
