#include "arm64_emit.h"

#include <cstdio>

namespace rsr {

// Register mapping (interim; final ABI TBD):
//   PPC r0..r30 -> X0..X30   (32-bit ops use W0..W30)
//   PPC r31 (stack pointer) -> X19
//   PPC lr  -> host LR (X30)  via mflr/mtlr
//   PPC ctr -> X20
//   PPC cr0 condition bits -> host flags estimated for branches (best effort)
namespace {

const char* xr(int r) {
    static thread_local char b[8];
    if (r == 31) return "x19"; // guest stack pointer
    std::snprintf(b, sizeof(b), "x%d", r);
    return b;
}

const char* wr(int r) {
    static thread_local char b[8];
    if (r == 31) return "w19"; // guest stack pointer (low half)
    std::snprintf(b, sizeof(b), "w%d", r);
    return b;
}

const char* cond_from_bi(uint32_t bi) {
    switch (bi & 3) {
    case 0: return "lt";
    case 1: return "gt";
    case 2: return "eq";
    default: return "ls";
    }
}

} // namespace

std::string emit_arm64(const Insn& i, const std::string& label) {
    char buf[256];
    std::string src_str = to_string(i);
    const char* src = src_str.c_str();

    switch (i.op) {
    case Op::NOP:
        std::snprintf(buf, sizeof(buf), "  nop                         // %s", src);
        break;

    case Op::ADD:
        std::snprintf(buf, sizeof(buf), "  add %s, %s, %s                // %s",
                      wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::ADDI:
        if (i.ra == 0) std::snprintf(buf, sizeof(buf), "  mov %s, #%lld                 // %s", wr(i.rt), (long long)i.d, src);
        else           std::snprintf(buf, sizeof(buf), "  add %s, %s, #%lld            // %s", wr(i.rt), wr(i.ra), (long long)i.d, src);
        break;
    case Op::ADDIS:
        if (i.ra == 0) std::snprintf(buf, sizeof(buf), "  movz %s, #%u, lsl #16         // %s", wr(i.rt), (unsigned)(i.d & 0xFFFF), src);
        else           std::snprintf(buf, sizeof(buf), "  add %s, %s, %s               // %s  (TODO precise for addis+ra)",
                                     wr(i.rt), wr(i.ra), wr(i.rt), src);
        break;
    case Op::SUBF:
        std::snprintf(buf, sizeof(buf), "  sub %s, %s, %s                // %s",
                      wr(i.rd), wr(i.rb), wr(i.ra), src);
        break;
    case Op::MULLW:
        std::snprintf(buf, sizeof(buf), "  mul %s, %s, %s                 // %s", wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;

    case Op::AND:
        std::snprintf(buf, sizeof(buf), "  and %s, %s, %s                 // %s", wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::ANDC:
        std::snprintf(buf, sizeof(buf), "  bic %s, %s, %s                 // %s", wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::OR:
        std::snprintf(buf, sizeof(buf), "  orr %s, %s, %s                 // %s", wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::ORC:
        std::snprintf(buf, sizeof(buf), "  orn %s, %s, %s                 // %s", wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::XOR:
        std::snprintf(buf, sizeof(buf), "  eor %s, %s, %s                 // %s", wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::NOR:
        std::snprintf(buf, sizeof(buf), "  orr %s, %s, %s                 // %s  (TODO: NOR needs MVP)",
                      wr(i.rd), wr(i.ra), wr(i.rb), src);
        break;
    case Op::ORI:
        std::snprintf(buf, sizeof(buf), "  orr %s, %s, #%u                // %s", wr(i.rt), wr(i.ra), i.uimm, src);
        break;
    case Op::ORIS:
        std::snprintf(buf, sizeof(buf), "  orr %s, %s, #%u, lsl #16       // %s", wr(i.rt), wr(i.ra), i.uimm, src);
        break;
    case Op::XORI:
        std::snprintf(buf, sizeof(buf), "  eor %s, %s, #%u                // %s", wr(i.rt), wr(i.ra), i.uimm, src);
        break;
    case Op::XORIS:
        std::snprintf(buf, sizeof(buf), "  eor %s, %s, #%u, lsl #16       // %s", wr(i.rt), wr(i.ra), i.uimm, src);
        break;
    case Op::SLW:
        std::snprintf(buf, sizeof(buf), "  lsl %s, %s, %s                 // %s", wr(i.rd), wr(i.rs), wr(i.rb), src);
        break;
    case Op::SRW:
        std::snprintf(buf, sizeof(buf), "  lsr %s, %s, %s                 // %s", wr(i.rd), wr(i.rs), wr(i.rb), src);
        break;
    case Op::SRAW:
        std::snprintf(buf, sizeof(buf), "  asr %s, %s, %s                 // %s", wr(i.rd), wr(i.rs), wr(i.rb), src);
        break;
    case Op::SRAWI:
        std::snprintf(buf, sizeof(buf), "  asr %s, %s, #%u                // %s", wr(i.rd), wr(i.rs), i.sh, src);
        break;
    case Op::CNTLZW:
        std::snprintf(buf, sizeof(buf), "  clz %s, %s                     // %s", wr(i.rd), wr(i.rs), src);
        break;
    case Op::EXTSB:
        std::snprintf(buf, sizeof(buf), "  sxtb %s, %s                    // %s", wr(i.rd), wr(i.rs), src);
        break;
    case Op::EXTSH:
        std::snprintf(buf, sizeof(buf), "  sxth %s, %s                    // %s", wr(i.rd), wr(i.rs), src);
        break;

    case Op::B:
    case Op::BL:
        if (label.empty())
            std::snprintf(buf, sizeof(buf), "  %s                            // %s", i.op == Op::BL ? "bl" : "b", src);
        else
            std::snprintf(buf, sizeof(buf), "  %s %s                         // %s", i.op == Op::BL ? "bl" : "b", label.c_str(), src);
        break;
    case Op::BC:
        if (label.empty())
            std::snprintf(buf, sizeof(buf), "  b.%s                          // %s", cond_from_bi(i.bi), src);
        else
            std::snprintf(buf, sizeof(buf), "  b.%s %s                       // %s", cond_from_bi(i.bi), label.c_str(), src);
        break;
    case Op::BCLR:
        std::snprintf(buf, sizeof(buf), "  ret                           // %s", src);
        break;
    case Op::BCTR:
        std::snprintf(buf, sizeof(buf), "  br x20                        // %s  (ctr -> x20)", src);
        break;

    case Op::LWZ:
        if (i.ra == 0) std::snprintf(buf, sizeof(buf), "  ldr %s, [xzr, #%lld]           // %s", wr(i.rt), (long long)i.d, src);
        else           std::snprintf(buf, sizeof(buf), "  ldr %s, [%s, #%lld]           // %s", wr(i.rt), xr(i.ra), (long long)i.d, src);
        break;
    case Op::STW:
        std::snprintf(buf, sizeof(buf), "  str %s, [%s, #%lld]           // %s", wr(i.rs), xr(i.ra), (long long)i.d, src);
        break;
    case Op::LD:
        std::snprintf(buf, sizeof(buf), "  ldr %s, [%s, #%lld]           // %s", xr(i.rt), xr(i.ra), (long long)i.d, src);
        break;
    case Op::STD:
        std::snprintf(buf, sizeof(buf), "  str %s, [%s, #%lld]           // %s", xr(i.rs), xr(i.ra), (long long)i.d, src);
        break;
    case Op::LWZU:
        std::snprintf(buf, sizeof(buf), "  ldr %s, [%s, #%lld]!          // %s", wr(i.rt), xr(i.ra), (long long)i.d, src);
        break;
    case Op::STWU:
        std::snprintf(buf, sizeof(buf), "  str %s, [%s, #%lld]!          // %s", wr(i.rs), xr(i.ra), (long long)i.d, src);
        break;
    case Op::LHA:
        std::snprintf(buf, sizeof(buf), "  ldrsh %s, [%s, #%lld]         // %s", wr(i.rt), xr(i.ra), (long long)i.d, src);
        break;
    case Op::LBZ:
        std::snprintf(buf, sizeof(buf), "  ldrb %s, [%s, #%lld]          // %s", wr(i.rt), xr(i.ra), (long long)i.d, src);
        break;
    case Op::STB:
        std::snprintf(buf, sizeof(buf), "  strb %s, [%s, #%lld]          // %s", wr(i.rs), xr(i.ra), (long long)i.d, src);
        break;

    case Op::MFLR:
        std::snprintf(buf, sizeof(buf), "  mov %s, x30                    // %s", xr(i.rd), src);
        break;
    case Op::MTLR:
        std::snprintf(buf, sizeof(buf), "  mov x30, %s                    // %s", xr(i.rs), src);
        break;
    case Op::MFCTR:
        std::snprintf(buf, sizeof(buf), "  mov %s, x20                    // %s", xr(i.rd), src);
        break;
    case Op::MTCTR:
        std::snprintf(buf, sizeof(buf), "  mov x20, %s                    // %s", xr(i.rs), src);
        break;

    case Op::CMPW:
        std::snprintf(buf, sizeof(buf), "  cmp %s, %s                     // %s", wr(i.ra), wr(i.rb), src);
        break;
    case Op::CMPI:
        std::snprintf(buf, sizeof(buf), "  cmp %s, #%lld                  // %s", wr(i.ra), (long long)i.d, src);
        break;

    case Op::SYNC:
        std::snprintf(buf, sizeof(buf), "  dmb ish                       // %s", src);
        break;
    case Op::ISYNC:
        std::snprintf(buf, sizeof(buf), "  isb                           // %s", src);
        break;

    default:
        std::snprintf(buf, sizeof(buf), "  .byte 0x00                     // UNSUPPORTED: %s (raw %08x)", to_string(i).c_str(), i.raw);
        break;
    }
    return buf;
}

} // namespace rsr