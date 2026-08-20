#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rsr {

struct XexSection {
    uint32_t page_size = 0x1000; // bytes per page
    uint32_t page_count = 0;     // pages in this section
    uint64_t data_offset = 0;    // file offset
    uint64_t vaddr = 0;          // guest memory address
};

struct XexImage {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> data;
    uint64_t entry = 0;
    uint64_t base = 0;
    uint64_t image_size = 0;
    std::vector<XexSection> sections;

    // Section whose [vaddr, vaddr+size) contains addr, or nullptr.
    const XexSection* section_at(uint64_t addr) const;
};

// Loads a big-endian Xbox 360 executable (XEX2). XEX1 is detected but not
// yet parsed.
XexImage load_xex(const std::string& path);

inline uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0]) << 8 | p[1]; }
inline uint32_t be32(const uint8_t* p) { return (static_cast<uint32_t>(be16(p)) << 16) | be16(p + 2); }
inline uint64_t be64(const uint8_t* p) { return (static_cast<uint64_t>(be32(p)) << 32) | be32(p + 4); }

} // namespace rsr