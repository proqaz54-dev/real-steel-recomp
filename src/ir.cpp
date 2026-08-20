#include "ir.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

namespace rsr {

namespace {

bool terminal(const Insn& i) {
    switch (i.op) {
    case Op::B: case Op::BL: case Op::BC: case Op::BCLR: case Op::BCTR:
    case Op::SC: case Op::TRAP:
        return true;
    default: return false;
    }
}

uint64_t target_of(const Insn& i) {
    if (i.op == Op::B || i.op == Op::BL)
        return i.aa ? (i.d & 0xFFFFFFFF) : static_cast<uint64_t>(i.pc + i.d);
    if (i.op == Op::BC)
        return static_cast<uint64_t>(i.pc + i.d);
    return 0;
}

int vg(int r) { return r; } // GPR
constexpr int VCTR = 64;    // guest CTR (host x20)
constexpr int VLR = 65;     // guest LR  (host x30)

// Lowers BC / BCLR / BCTR into a sequence honoring BO[2]/BO[3] CTR tests.
void lower_branch(const Insn& i, std::vector<IRInsn>& out) {
    bool on_zero = false;
    bool has_ctr = ctr_test(i, on_zero);
    bool has_cr = (i.bo & 0x10) == 0;

    if (has_ctr && has_cr) {
        // Combined CTR+CR tests: rare (bdnzt/bdnzf etc.) - left unsupported,
        // never fabricate semantics.
        IRInsn u;
        u.op = IROp::UNSUP;
        u.pc = i.pc;
        out.push_back(u);
        return;
    }

    if (has_ctr) {
        IRInsn d; d.op = IROp::SUB; d.dst = VCTR; d.a = VCTR; d.imm = 1; d.pc = i.pc;
        IRInsn c; c.op = IROp::CMPI; c.a = VCTR; c.imm = 0; c.pc = i.pc;
        out.push_back(d);
        out.push_back(c);
        IRInsn b;
        b.op = IROp::BR_COND;
        b.imm = (on_zero ? 2 /*eq*/ : (2 | 0x80) /*ne*/) | 0x4000 /*ctr*/;
        b.pc = i.pc;
        if (i.op == Op::BC) {
            b.label = static_cast<uint64_t>(i.pc + i.d);
        } else if (i.op == Op::BCLR) {
            // bdnzl: continue to LR when not taken -> resume after this insn
            b.label = static_cast<uint64_t>(i.pc + 4);
        } else {
            b.op = IROp::UNSUP; // bdnzctr: indirect + ctr
        }
        out.push_back(b);
        if (i.op == Op::BCLR) {
            IRInsn r; r.op = IROp::RET; r.pc = i.pc;
            out.push_back(r);
        }
        return;
    }

    switch (i.op) {
    case Op::BC: {
        IRInsn b;
        b.op = IROp::BR_COND;
        b.label = static_cast<uint64_t>(i.pc + i.d);
        b.imm = i.bi | ((i.bo & 0x08) ? 0 : 0x80);
        b.pc = i.pc;
        out.push_back(b);
        break;
    }
    case Op::BCLR: {
        IRInsn r; r.op = IROp::RET; r.pc = i.pc;
        out.push_back(r);
        break;
    }
    case Op::BCTR: {
        // indirect through LR... no, through CTR
        IRInsn b;
        b.op = IROp::BR;
        b.a = VCTR;
        b.imm = -1;
        b.pc = i.pc;
        out.push_back(b);
        break;
    }
    default: break;
    }
}

IRInsn lower(const Insn& i) {
    IRInsn r;
    r.pc = i.pc;
    switch (i.op) {
    case Op::ADDI:  r.op = IROp::ADD;  r.dst = vg(i.rt); r.a = vg(i.ra); r.imm = i.d; break;
    case Op::ADDIS: r.op = IROp::MOVZ; r.dst = vg(i.rt); r.imm = i.d; break;
    case Op::ADD:   r.op = IROp::ADD;  r.dst = vg(i.rd); r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::SUBF:  r.op = IROp::SUB;  r.dst = vg(i.rd); r.a = vg(i.rb); r.b = vg(i.ra); break;
    case Op::NEG:   r.op = IROp::NEG;  r.dst = vg(i.rd); r.a = vg(i.rs); break;
    case Op::MULLI: r.op = IROp::MUL;  r.dst = vg(i.rt); r.a = vg(i.ra); r.imm = i.d; break;
    case Op::MULLW: r.op = IROp::MUL;  r.dst = vg(i.rd); r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::AND:   r.op = IROp::AND;  r.dst = vg(i.rd); r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::OR:    r.op = IROp::OR;   r.dst = vg(i.rd); r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::XOR:   r.op = IROp::XOR;  r.dst = vg(i.rd); r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::SLW:   r.op = IROp::SHL;  r.dst = vg(i.rd); r.a = vg(i.rs); r.b = vg(i.rb); break;
    case Op::SRW:   r.op = IROp::SHR;  r.dst = vg(i.rd); r.a = vg(i.rs); r.b = vg(i.rb); break;
    case Op::SRAW:  r.op = IROp::ASR;  r.dst = vg(i.rd); r.a = vg(i.rs); r.b = vg(i.rb); break;
    case Op::SRAWI: r.op = IROp::ASR;  r.dst = vg(i.rd); r.a = vg(i.rs); r.imm = i.sh; break;
    case Op::CNTLZW:r.op = IROp::CLZ;  r.dst = vg(i.rd); r.a = vg(i.rs); break;
    case Op::EXTSB: r.op = IROp::EXTS; r.dst = vg(i.rd); r.a = vg(i.rs); r.imm = 8; break;
    case Op::EXTSH: r.op = IROp::EXTS; r.dst = vg(i.rd); r.a = vg(i.rs); r.imm = 16; break;
    case Op::EXTSW: r.op = IROp::EXTS; r.dst = vg(i.rd); r.a = vg(i.rs); r.imm = 32; break;
    case Op::RLWINM: // ror + and: handled as two IR insns via ROR+AND marker
        if (i.sh == 0)
            r.op = IROp::AND;   // pure mask
        else {
            r.op = IROp::ROR;   // rotate part; mask appended separately below
            r.dst = vg(i.rd); r.a = vg(i.rs); r.imm = i.sh;
        }
        break;
    case Op::CMPW:  r.op = IROp::CMP;  r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::CMPLW: r.op = IROp::CMPU; r.a = vg(i.ra); r.b = vg(i.rb); break;
    case Op::CMPI:  r.op = IROp::CMPI; r.a = vg(i.ra); r.imm = i.d; break;
    case Op::CMPLI: r.op = IROp::CMPIU; r.a = vg(i.ra); r.imm = static_cast<int64_t>(i.uimm); break;

    case Op::B:     r.op = IROp::BR;      r.label = target_of(i); break;
    case Op::BL:    r.op = IROp::CALL;    r.label = target_of(i); break;
    case Op::MFLR:  r.op = IROp::MOV; r.dst = vg(i.rd); r.a = VLR; break;
    case Op::MTLR:  r.op = IROp::MOV; r.dst = VLR; r.a = vg(i.rs); break;
    case Op::MFCTR: r.op = IROp::MOV; r.dst = vg(i.rd); r.a = VCTR; break;
    case Op::MTCTR: r.op = IROp::MOV; r.dst = VCTR; r.a = vg(i.rs); break;

    case Op::LWZ:    r.op = IROp::LDR32; r.dst = vg(i.rt); r.a = vg(i.ra); r.imm = i.d; break;
    case Op::STW:    r.op = IROp::STR32; r.a = vg(i.ra); r.b = vg(i.rs); r.imm = i.d; break;
    case Op::LBZ:    r.op = IROp::LDR8;  r.dst = vg(i.rt); r.a = vg(i.ra); r.imm = i.d; break;
    case Op::STB:    r.op = IROp::STR8;  r.a = vg(i.ra); r.b = vg(i.rs); r.imm = i.d; break;
    case Op::LHZ:    r.op = IROp::LDR16; r.dst = vg(i.rt); r.a = vg(i.ra); r.imm = i.d; break;
    case Op::STH:    r.op = IROp::STR16; r.a = vg(i.ra); r.b = vg(i.rs); r.imm = i.d; break;
    case Op::LD:     r.op = IROp::LDR64X; r.dst = vg(i.rt); r.a = vg(i.ra); r.imm = i.d; break;
    case Op::STD:    r.op = IROp::STR64X; r.a = vg(i.ra); r.b = vg(i.rs); r.imm = i.d; break;

    case Op::SYNC:  r.op = IROp::DMB; break;
    case Op::ISYNC: r.op = IROp::ISB; break;
    case Op::SC:    r.op = IROp::SC; break;
    case Op::TRAP:  r.op = IROp::TRAP; break;

    default:
        r.op = IROp::UNSUP;
        break;
    }
    return r;
}

// Appends the mask half of rlwinm/rlwimi/rlwnm after the rotate where needed.
void append_mask(const Insn& i, const IRInsn& rot, std::vector<IRInsn>& out) {
    if (i.op != Op::RLWINM && i.op != Op::RLWIMI && i.op != Op::RLWNM) return;
    uint32_t mask = rot_mask(i.mb, i.me);
    if (i.sh == 0) {
        // pure mask: AND directly, dst already set by lower()
        out.push_back(rot);
        return;
    }
    IRInsn m;
    m.op = IROp::AND;
    m.dst = vg(i.rd);
    m.a = vg(i.rd);       // rotate result
    m.imm = static_cast<int64_t>(mask);
    m.pc = i.pc;
    out.push_back(m);
}

} // namespace

IRFunc build_ir(const std::vector<Insn>& insns,
                uint64_t start, uint64_t end,
                std::vector<uint64_t>& callees) {
    IRFunc f;
    f.addr = start;

    std::set<uint64_t> targets;
    targets.insert(start);
    for (const auto& in : insns) {
        if (static_cast<uint64_t>(in.pc) < start || static_cast<uint64_t>(in.pc) >= end) continue;
        if (in.op == Op::B || in.op == Op::BC || in.op == Op::BL) {
            uint64_t t = target_of(in);
            if (t >= start && t < end) targets.insert(t);
            if (in.op == Op::BL && t >= start && t < end) callees.push_back(t);
        }
    }
    for (const auto& in : insns)
        if (static_cast<uint64_t>(in.pc) >= start && static_cast<uint64_t>(in.pc) < end && terminal(in) && static_cast<uint64_t>(in.pc) + 4 < end)
            targets.insert(in.pc + 4);

    std::vector<uint64_t> starts;
    for (const auto& in : insns)
        if (static_cast<uint64_t>(in.pc) >= start && static_cast<uint64_t>(in.pc) < end && targets.count(static_cast<uint64_t>(in.pc)))
            starts.push_back(static_cast<uint64_t>(in.pc));

    for (size_t i = 0; i < starts.size(); i++) {
        IRBlock b;
        b.start = starts[i];
        b.end = (i + 1 < starts.size()) ? starts[i + 1] : end;
        for (const auto& in : insns) {
            if (static_cast<uint64_t>(in.pc) < b.start || static_cast<uint64_t>(in.pc) >= b.end) continue;
            if (in.op == Op::BC || in.op == Op::BCLR || in.op == Op::BCTR) {
                lower_branch(in, b.insns);
            } else {
                IRInsn ir = lower(in);
                b.insns.push_back(ir);
                append_mask(in, ir, b.insns);
            }
            if (terminal(in)) break;
        }
        f.blocks.push_back(std::move(b));
    }

    std::map<uint64_t, size_t> idx;
    for (size_t i = 0; i < f.blocks.size(); i++) idx[f.blocks[i].start] = i;
    for (const auto& b : f.blocks)
        for (const auto& ir : b.insns)
            if ((ir.op == IROp::BR || ir.op == IROp::BR_COND) && idx.count(ir.label))
                f.blocks[idx[ir.label]].preds++;
    return f;
}

std::string ir_to_string(const IRFunc& f) {
    std::string out;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "  IR fn@0x%llx blocks=%zu\n",
                  (unsigned long long)f.addr, f.blocks.size());
    out += buf;
    for (const auto& b : f.blocks) {
        std::snprintf(buf, sizeof(buf), "    bb 0x%llx..0x%llx preds=%d\n",
                      (unsigned long long)b.start, (unsigned long long)b.end, b.preds);
        out += buf;
        for (const auto& ir : b.insns) {
            std::snprintf(buf, sizeof(buf), "      %-10s d=%-2d a=%-2d b=%-2d imm=%-6lld ->0x%llx\n",
                          ir.op == IROp::ADD ? "add" : ir.op == IROp::SUB ? "sub" :
                          ir.op == IROp::MUL ? "mul" : ir.op == IROp::AND ? "and" :
                          ir.op == IROp::OR ? "or" : ir.op == IROp::XOR ? "xor" :
                          ir.op == IROp::NEG ? "neg" : ir.op == IROp::SHL ? "shl" :
                          ir.op == IROp::SHR ? "shr" : ir.op == IROp::ASR ? "asr" :
                          ir.op == IROp::ROR ? "ror" : ir.op == IROp::EXTS ? "exts" :
                          ir.op == IROp::CLZ ? "clz" : ir.op == IROp::MOV ? "mov" : ir.op == IROp::MOVZ ? "movz" :
                          ir.op == IROp::MOVI ? "movi" : ir.op == IROp::CMP ? "cmp" :
                          ir.op == IROp::CMPU ? "cmpu" : ir.op == IROp::CMPI ? "cmpi" :
                          ir.op == IROp::CMPIU ? "cmpiu" : ir.op == IROp::BR ? "br" :
                          ir.op == IROp::BR_COND ? "br.cond" : ir.op == IROp::CALL ? "call" :
                          ir.op == IROp::RET ? "ret" : ir.op == IROp::LDR32 ? "ldr32" :
                          ir.op == IROp::STR32 ? "str32" : ir.op == IROp::LDR8 ? "ldr8" :
                          ir.op == IROp::STR8 ? "str8" : ir.op == IROp::LDR16 ? "ldr16" :
                          ir.op == IROp::STR16 ? "str16" : ir.op == IROp::LDR64X ? "ldr64" :
                          ir.op == IROp::STR64X ? "str64" : ir.op == IROp::DMB ? "dmb" :
                          ir.op == IROp::ISB ? "isb" : ir.op == IROp::SC ? "sc" :
                          ir.op == IROp::TRAP ? "trap" : ir.op == IROp::UNSUP ? "UNSUP" : "nop",
                          ir.dst, ir.a, ir.b, (long long)ir.imm, (unsigned long long)ir.label);
            out += buf;
        }
    }
    return out;
}

} // namespace rsr