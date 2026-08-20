#include "xex.h"

#include <cstdio>

namespace rsr {

namespace {

constexpr uint32_t kMagicXex2 = 0x58455832; // "XEX2"
constexpr uint32_t kMagicXex1 = 0x58455831; // "XEX1"

constexpr uint32_t kXeSectionHeaders = 0xFFFF0101;

} // namespace

const XexSection* XexImage::section_at(uint64_t addr) const {
    for (const auto& s : sections) {
        uint64_t size = uint64_t(s.page_size) * s.page_count;
        if (addr >= s.vaddr && addr < s.vaddr + size) return &s;
    }
    return nullptr;
}

XexImage load_xex(const std::string& path) {
    XexImage img;

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        img.error = "cannot open " + path;
        return img;
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::rewind(f);
    if (sz < 0x200) {
        img.error = "file too small";
        std::fclose(f);
        return img;
    }
    img.data.resize(static_cast<size_t>(sz));
    if (std::fread(img.data.data(), 1, img.data.size(), f) != img.data.size()) {
        img.error = "read failed";
        std::fclose(f);
        return img;
    }
    std::fclose(f);

    const uint8_t* h = img.data.data();
    uint32_t magic = be32(h + 0x00);
    if (magic == kMagicXex1) {
        img.error = "XEX1 not supported yet (retail games ship XEX2)";
        return img;
    }
    if (magic != kMagicXex2) {
        img.error = "not an XEX2 executable";
        return img;
    }

    img.base       = be32(h + 0x14);
    img.entry      = be32(h + 0x18);
    img.image_size = be32(h + 0x0C);
    uint32_t header_count = be32(h + 0x34);
    uint32_t header_size  = be32(h + 0x38);

    // Optional header descriptors: {id, size} pairs.
    size_t p = 0x3C;
    uint64_t block = p + header_size;
    uint64_t section_block = 0;
    uint64_t optional_total = 0;
    for (uint32_t i = 0; i < header_count && p + 8 <= img.data.size(); i++) {
        uint32_t id   = be32(img.data.data() + p);
        uint32_t size = be32(img.data.data() + p + 4);
        if (id == kXeSectionHeaders) section_block = block;
        optional_total += size;
        block += size;
        p += 8;
    }
    if (!section_block || section_block + 4 > img.data.size()) {
        img.error = "missing section header block";
        return img;
    }

    // XeSectionHeaders: count, then {page_size, page_count} per section.
    // Section 0 is the header page (data at offset 0); payload sections follow
    // the optional-header area, packed sequentially.
    const uint8_t* sb = img.data.data() + section_block;
    uint32_t count = be32(sb);
    uint64_t off = section_block + 4;
    uint64_t data_start = (0x3C + header_size + optional_total + 0x1FF) & ~uint64_t(0x1FF);
    uint64_t cursor = data_start;
    uint64_t pages_before = 0;
    for (uint32_t i = 0; i < count && off + 8 <= img.data.size(); i++) {
        XexSection s;
        s.page_size  = be32(img.data.data() + off);
        s.page_count = be32(img.data.data() + off + 4);
        s.data_offset = (i == 0) ? 0 : cursor;
        s.vaddr = img.base + pages_before * 0x1000;
        if (i != 0) cursor = (cursor + uint64_t(s.page_size) * s.page_count + 0x1FF) & ~uint64_t(0x1FF);
        pages_before += s.page_count;
        img.sections.push_back(s);
        off += 8;
    }

    img.ok = !img.sections.empty();
    if (!img.ok) img.error = "no sections";
    return img;
}

XexImage load_flat(const std::string& path, uint64_t base, uint64_t entry,
                   const std::vector<std::pair<uint64_t, uint64_t>>& code_ranges) {
    XexImage img;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        img.error = "cannot open " + path;
        return img;
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::rewind(f);
    if (sz <= 0) {
        img.error = "empty image";
        std::fclose(f);
        return img;
    }
    img.data.resize(static_cast<size_t>(sz));
    if (std::fread(img.data.data(), 1, img.data.size(), f) != img.data.size()) {
        img.error = "read failed";
        std::fclose(f);
        return img;
    }
    std::fclose(f);

    img.base = base;
    img.entry = entry;
    img.image_size = static_cast<uint64_t>(sz);
    for (const auto& r : code_ranges) {
        XexSection s;
        s.page_size = 1;
        s.page_count = static_cast<uint32_t>(r.second - r.first);
        s.data_offset = r.first;
        s.vaddr = base + r.first;
        img.sections.push_back(s);
    }
    img.ok = !img.sections.empty();
    if (!img.ok) img.error = "no code ranges";
    return img;
}

} // namespace rsr