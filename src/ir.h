#pragma once

#include "ppc32_decode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rsr {

// Three-address IR. Virtual registers: v0..v31 = PPC r0..r31,
// v32..v63 = FP fr0..fr31, v64 = CTR, v65 = LR (host x20/x30).
enum class IROp {
    NOP,
    MOV, MOVI, MOVZ, MOV64, // mov dst,src / mov dst,#imm / movz dst,#(imm<<16) / mov x64
    ADD, SUB, MUL, AND, OR, XOR, NEG, NOT,
    SHL, SHR, ASR, ROR,     // register or immediate variants (b<0 => imm)
    EXTS, CLZ,
    CMP, CMPU, CMPI, CMPIU,
    BR, BR_COND, CALL, RET, // BR_COND.imm: bi | (0x80 if branch-if-false)
    LDR32, STR32, LDR16, STR16, LDR8, STR8,
    LDR64X, STR64X,
    ADDR,                   // base + imm into vreg
    SC, DMB, ISB, TRAP, UNSUP,
};

struct IRInsn {
    IROp op = IROp::NOP;
    int dst = -1, a = -1, b = -1; // virtual registers, -1 = none
    int64_t imm = 0;
    uint64_t label = 0;           // branch target (guest address)
    int64_t pc = 0;
};

struct IRBlock {
    uint64_t start = 0, end = 0;  // guest address range
    std::vector<IRInsn> insns;
    int preds = 0;
};

struct IRFunc {
    uint64_t addr = 0;
    std::vector<IRBlock> blocks;
};

// Builds basic blocks + 3-addr IR for the guest range [start,end).
// Direct bl targets (in-range) are appended to `callees`.
IRFunc build_ir(const std::vector<Insn>& insns,
                uint64_t start, uint64_t end,
                std::vector<uint64_t>& callees);

std::string ir_to_string(const IRFunc& f);

} // namespace rsr