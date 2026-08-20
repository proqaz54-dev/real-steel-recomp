#include "arm64_codegen.h"

#include <cstdio>

namespace rsr {

namespace {

char label_buf[32];

const char* label(uint64_t addr) {
    std::snprintf(label_buf, sizeof(label_buf), "L_%llx", (unsigned long long)addr);
    return label_buf;
}

std::string wr(const RegAlloc& ra, int v) {
    if (v == 64) return "w20"; // CTR
    if (v == 65) return "w30"; // LR
    if (v == 31) return "w19";
    if (v >= 0 && v < static_cast<int>(ra.phys.size()) && ra.phys[v] >= 0)
        return "w" + std::to_string(ra.phys[v]);
    return "w16"; // spilled: caller emits reload
}
std::string xr(const RegAlloc& ra, int v) {
    if (v == 64) return "x20"; // CTR
    if (v == 65) return "x30"; // LR
    if (v == 31) return "x19";
    if (v >= 0 && v < static_cast<int>(ra.phys.size()) && ra.phys[v] >= 0)
        return "x" + std::to_string(ra.phys[v]);
    return "x16";
}

// Load spilled operand into scratch; "" if not spilled.
std::string reload(const RegAlloc& ra, int v, bool wide) {
    if (v < 0) return "";
    if (v == 31 || v == 64 || v == 65) return "";
    if (v >= 0 && v < static_cast<int>(ra.phys.size()) && ra.phys[v] >= 0) return "";
    if (v >= static_cast<int>(ra.slot.size()) || ra.slot[v] < 0) return "";
    return (wide ? "  ldr x16, [x19, #-" : "  ldr w16, [x19, #-") +
           std::to_string(ra.slot[v] * 8) + "]\n";
}

// Store result to spill slot; "" if not spilled.
std::string spill_store(const RegAlloc& ra, int v, bool wide) {
    if (v < 0 || v == 31 || v == 64 || v == 65) return "";
    if (v >= static_cast<int>(ra.phys.size()) || ra.phys[v] >= 0) return "";
    if (v >= static_cast<int>(ra.slot.size()) || ra.slot[v] < 0) return "";
    return (wide ? "  str x16, [x19, #-" : "  str w16, [x19, #-") +
           std::to_string(ra.slot[v] * 8) + "]\n";
}

// Valid AArch64 32-bit logical immediate (AND/ORR/BIC/EOR #imm encodable)?
bool log32(uint32_t x) {
    if (x == 0 || x == ~0u) return false;
    for (int w : {2, 4, 8, 16, 32}) {
        uint32_t mask = (w == 32) ? ~0u : ((1u << w) - 1);
        bool ok = true, has_full = false, has_zero = false;
        for (uint32_t i = 0; i < 32 && ok; i += w) {
            uint32_t e = (x >> i) & mask;
            if (e == mask) has_full = true;
            else if (e == 0) has_zero = true;
            else ok = false;
        }
        if (ok && has_full && has_zero) return true;
    }
    return false;
}

// Set x17 = imm (32-bit, sign-extended into a w-view) via movz/movk.
std::string scratch32(uint32_t imm) {
    std::string r = "  movz w17, #" + std::to_string(imm & 0xFFFF) + "\n";
    imm >>= 16;
    r += "  movk w17, #" + std::to_string(imm & 0xFFFF) + ", lsl #16\n";
    return r;
}

// Set x17 = imm (64-bit, sign-extended).
std::string scratch64(int64_t v) {
    uint64_t u = static_cast<uint64_t>(v);
    std::string r = "  movz x17, #" + std::to_string(u & 0xFFFF) + "\n";
    u >>= 16;
    r += "  movk x17, #" + std::to_string(u & 0xFFFF) + ", lsl #16\n";
    u >>= 16;
    r += "  movk x17, #" + std::to_string(u & 0xFFFF) + ", lsl #32\n";
    u >>= 16;
    if (u) r += "  movk x17, #" + std::to_string(u & 0xFFFF) + ", lsl #48\n";
    return r;
}

// Pre-amble that computes addr = x(a) + imm into x17.
std::string addr_pre64(const RegAlloc& ra, int v, int64_t imm) {
    std::string pre = reload(ra, v, true);
    std::string base = xr(ra, v);
    if (imm >= -4096 && imm <= 4095)
        return pre + "  add x17, " + base + ", #" + std::to_string(imm) + "\n";
    return pre + scratch64(imm) + "  add x17, " + base + ", x17\n";
}


const char* cond_of(int bi, bool iff) {
    const char* c;
    switch (bi & 3) {
    case 0: c = "lt"; break;
    case 1: c = "gt"; break;
    case 2: c = "eq"; break;
    default: c = "vs"; break;
    }
    if (!iff) {
        switch (c[0]) {
        case 'l': return "ge";
        case 'g': return "le";
        case 'e': return "ne";
        default: return "vc";
        }
    }
    return c;
}

} // namespace

std::string codegen_arm64(const IRFunc& f, const RegAlloc& ra,
                          bool (*in_range)(uint64_t, void*), void* ctx) {
    std::string out;
    out += "fn_" + std::string(label(f.addr)) + ":\n"; // fn_L_<addr>
    out += std::string("/* IR function @ 0x") + label(f.addr) + " */\n";

    for (const auto& b : f.blocks) {
        out += ".Lbb_" + std::string(label(b.start)) + ":\n";
        for (const auto& ir : b.insns) {
            std::string pre, post, line;
            auto w = [&](int v) { return wr(ra, v); };
            auto x = [&](int v) { return xr(ra, v); };

            switch (ir.op) {
            case IROp::MOV:
                pre += reload(ra, ir.a, false);
                line = "  mov " + w(ir.dst) + ", " + w(ir.a);
                break;
            case IROp::MOV64:
                pre += reload(ra, ir.a, true);
                line = "  mov " + x(ir.dst) + ", " + x(ir.a);
                break;
            case IROp::MOVI:
                if (ir.imm >= -0xFFFF && ir.imm <= 0xFFFF) {
                    line = "  mov " + w(ir.dst) + ", #" + std::to_string(ir.imm);
                } else {
                    pre += scratch32(static_cast<uint32_t>(ir.imm));
                    line = "  mov " + w(ir.dst) + ", w17";
                }
                break;
            case IROp::MOVZ: line = "  movz " + w(ir.dst) + ", #" + std::to_string(ir.imm & 0xFFFF) + ", lsl #16"; break;
            case IROp::ADD:
                pre += reload(ra, ir.a, false) + reload(ra, ir.b, false);
                if (ir.b < 0) {
                    if (ir.imm >= -4096 && ir.imm <= 4095)
                        line = "  add " + w(ir.dst) + ", " + w(ir.a) + ", #" + std::to_string(ir.imm);
                    else { pre += scratch32(static_cast<uint32_t>(ir.imm)); line = "  add " + w(ir.dst) + ", " + w(ir.a) + ", w17"; }
                }
                else
                    line = "  add " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                break;
            case IROp::SUB:
                pre += reload(ra, ir.a, false);
                if (ir.b >= 0) {
                    pre += reload(ra, ir.b, false);
                    line = "  sub " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                } else {
                    if (ir.imm >= -4096 && ir.imm <= 4095)
                        line = "  sub " + w(ir.dst) + ", " + w(ir.a) + ", #" + std::to_string(ir.imm);
                    else { pre += scratch32(static_cast<uint32_t>(ir.imm)); line = "  sub " + w(ir.dst) + ", " + w(ir.a) + ", w17"; }
                }
                break;
            case IROp::MUL:
                pre += reload(ra, ir.a, false) + reload(ra, ir.b, false);
                if (ir.b < 0) { pre += scratch32(static_cast<uint32_t>(ir.imm)); line = "  madd " + w(ir.dst) + ", " + w(ir.a) + ", w17, wzr"; }
                else
                    line = "  mul " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                break;
            case IROp::AND:
                pre += reload(ra, ir.a, false);
                if (ir.b < 0) {
                    uint32_t m = static_cast<uint32_t>(ir.imm);
                    if (m == ~0u) {
                        line = "  mov " + w(ir.dst) + ", " + w(ir.a);
                    } else if (log32(m)) {
                        char hex[24];
                        std::snprintf(hex, sizeof(hex), "0x%x", m);
                        line = std::string("  and ") + w(ir.dst) + ", " + w(ir.a) + ", #" + hex;
                    } else {
                        pre += scratch32(m);
                        line = "  and " + w(ir.dst) + ", " + w(ir.a) + ", w17";
                    }
                } else {
                    pre += reload(ra, ir.b, false);
                    line = "  and " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                }
                break;
            case IROp::OR:
                pre += reload(ra, ir.a, false) + reload(ra, ir.b, false);
                line = "  orr " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                break;
            case IROp::XOR:
                pre += reload(ra, ir.a, false) + reload(ra, ir.b, false);
                line = "  eor " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                break;
            case IROp::NEG:
                pre += reload(ra, ir.a, false);
                line = "  neg " + w(ir.dst) + ", " + w(ir.a);
                break;
            case IROp::SHL:
            case IROp::SHR:
            case IROp::ASR:
            case IROp::ROR: {
                pre += reload(ra, ir.a, false);
                if (ir.b >= 0) pre += reload(ra, ir.b, false);
                const char* m = ir.op == IROp::SHL ? "lsl" : ir.op == IROp::SHR ? "lsr" :
                                ir.op == IROp::ASR ? "asr" : "ror";
                if (ir.b >= 0)
                    line = std::string("  ") + m + " " + w(ir.dst) + ", " + w(ir.a) + ", " + w(ir.b);
                else
                    line = std::string("  ") + m + " " + w(ir.dst) + ", " + w(ir.a) + ", #" + std::to_string(ir.imm);
                break;
            }
            case IROp::EXTS:
                pre += reload(ra, ir.a, false);
                if (ir.imm == 32)   // EXTSW: sign-extend 32->64, needs x-form dest
                    line = "  sxtw " + x(ir.dst) + ", " + w(ir.a);
                else
                    line = std::string("  ") + (ir.imm == 8 ? "sxtb " : "sxth ") +
                           w(ir.dst) + ", " + w(ir.a);
                break;
            case IROp::CLZ:
                pre += reload(ra, ir.a, false);
                line = "  clz " + w(ir.dst) + ", " + w(ir.a);
                break;

            case IROp::CMP: case IROp::CMPU:
                pre += reload(ra, ir.a, false) + reload(ra, ir.b, false);
                line = "  cmp " + w(ir.a) + ", " + w(ir.b);
                break;
            case IROp::CMPI: case IROp::CMPIU:
                pre += reload(ra, ir.a, false);
                if (ir.imm >= -4096 && ir.imm <= 4095)
                    line = "  cmp " + w(ir.a) + ", #" + std::to_string(ir.imm);
                else { pre += scratch32(static_cast<uint32_t>(ir.imm)); line = "  cmp " + w(ir.a) + ", w17"; }
                break;

            case IROp::BR:
                if (ir.imm == -1)
                    line = "  br x20 // ctr-indirect";
                else if (!in_range(ir.label, ctx))
                    line = "  // unresolved branch -> 0x" + std::string(label(ir.label));
                else
                    line = std::string("  b ") + label(ir.label);
                break;
            case IROp::BR_COND: {
                bool ctr = (ir.imm & 0x4000) != 0;
                bool iff = (ir.imm & 0x80) == 0;
                if (!in_range(ir.label, ctx)) {
                    char b[48];
                    std::snprintf(b, sizeof(b), "0x%llx", (unsigned long long)ir.label);
                    line = std::string("  // unresolved conditional -> ") + b;
                    break;
                }
                line = std::string("  b.") + cond_of(static_cast<int>(ir.imm & 0x7F), iff) +
                       " " + label(ir.label) + " // " +
                       (ctr ? "ctr-based" : "flag-based (cr" + std::to_string(static_cast<int>(ir.imm & 0x7F) >> 2) + ")");
                break;
            }
            case IROp::CALL:
                if (in_range(ir.label, ctx))
                    line = std::string("  bl ") + label(ir.label);
                else
                    line = "  // call -> 0x" + std::string(label(ir.label)) + " (import/thunk)";
                break;
            case IROp::RET: line = "  ret"; break;

            case IROp::LDR32:
                pre += addr_pre64(ra, ir.a, ir.imm);
                line = "  ldr " + w(ir.dst) + ", [x17]";
                break;
            case IROp::STR32:
                pre += addr_pre64(ra, ir.a, ir.imm) + reload(ra, ir.b, false);
                line = "  str " + w(ir.b) + ", [x17]";
                break;
            case IROp::LDR8:
                pre += addr_pre64(ra, ir.a, ir.imm);
                line = "  ldrb " + w(ir.dst) + ", [x17]";
                break;
            case IROp::STR8:
                pre += addr_pre64(ra, ir.a, ir.imm) + reload(ra, ir.b, false);
                line = "  strb " + w(ir.b) + ", [x17]";
                break;
            case IROp::LDR16:
                pre += addr_pre64(ra, ir.a, ir.imm);
                line = "  ldrh " + w(ir.dst) + ", [x17]";
                break;
            case IROp::STR16:
                pre += addr_pre64(ra, ir.a, ir.imm) + reload(ra, ir.b, false);
                line = "  strh " + w(ir.b) + ", [x17]";
                break;
            case IROp::LDR64X:
                pre += addr_pre64(ra, ir.a, ir.imm);
                line = "  ldr " + x(ir.dst) + ", [x17]";
                break;
            case IROp::STR64X:
                pre += addr_pre64(ra, ir.a, ir.imm) + reload(ra, ir.b, true);
                line = "  str " + x(ir.b) + ", [x17]";
                break;

            case IROp::DMB:  line = "  dmb ish"; break;
            case IROp::ISB:  line = "  isb"; break;
            case IROp::SC:   line = "  svc #0 // xam syscall"; break;
            case IROp::TRAP: line = "  brk #0"; break;
            default:
                line = "  // unsupported IR op";
                break;
            }

            bool wide_res = ir.op == IROp::LDR64X || ir.op == IROp::MOV64;
            out += pre + line + "\n" + post + spill_store(ra, ir.dst, wide_res);
        }
        out += "\n";
    }
    return out;
}

} // namespace rsr