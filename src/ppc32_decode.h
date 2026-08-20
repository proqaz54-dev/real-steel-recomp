#pragma once

#include <cstdint>
#include <string>

namespace rsr {

enum class Op {
    UNKNOWN,
    // integer
    ADD, ADDC, ADDE, ADDZE, ADDME, ADDIC, ADDI, ADDIS,
    SUBF, SUBFC, SUBFE, SUBFZE, SUBFME, SUBFIC, NEG,
    MULLI, MULLW, MULHW, MULHWU, DIVW, DIVWU,
    AND, ANDC, OR, ORC, XOR, NOR, NAND, EQV,
    CNTLZW, CNTLZD, EXTSB, EXTSH, EXTSW, POPCNTB,
    SLW, SLD, SRW, SRD, SRAW, SRAWI, SRAWCI, RLWINM, RLWIMI, RLWNM,
    ORI, ORIS, XORI, XORIS, ANDI, ANDIS,
    // branch
    B, BL, BC, BCLR, BCTR, NOP,
    // load/store
    LWZ, LWZU, STW, STWU, LBZ, LBZU, STB, STBU,
    LHA, LHAU, LHZ, LHZU, STH, STHU,
    LMW, STMW, LWBRX, LHBRX, STWBRX, STHBRX,
    LWZX, LWZUX, LBZX, LBZUX, LHZX, LHZUX, LHAX, LHAUX,
    STWX, STWUX, STBX, STBUX, STHX, STHUX, LWARX, STWCX,
    LD, LDU, LWA, STD, STDU, DCBZ, DCBST, DCBF,
    LDX, LDUX, LDARX, STDX, STDCX,
    // floating point
    LFS, LFSU, LFD, LFDU, STFS, STFSU, STFD, STFDU,
    FADD, FSUB, FMUL, FDIV, FSQRT, FRES, FRSP, FABS, FNEG, FMR, FNABS, FSEL, FRSQRTE,
    FCMPU, FMADD, FMSUB, FNMADD, FNMSUB, MFFS, MTFSF,
    PS_ADD, PS_SUB, PS_MUL, PS_DIV, PS_ABS, PS_NEG, PS_MR, PS_MADD, PSQ_LX, PSQ_STX, PSQ_L, PSQ_ST,
    // condition register / special
    MFLR, MTLR, MFCTR, MTCTR, MFSPR, MTSPR,
    CMPW, CMPLW, CMPI, CMPLI, CMPB,
    MFCR, MTCRF, MCRF, MCRXR, CRAND, CROR, CRXOR, CRNAND, CREQV, CRANDC, CRNOR, CRORC,
    SYNC, ISYNC, EIEIO, TRAP, SC,
};

struct Insn {
    Op op = Op::UNKNOWN;
    uint32_t raw = 0;
    int64_t pc = 0;

    int rd = 0, ra = 0, rb = 0, rs = 0, rt = 0; // register fields
    int frd = 0, fra = 0, frb = 0, frs = 0, frt = 0; // FP registers
    int64_t d = 0;      // sign-extended displacement / branch offset
    uint32_t uimm = 0;  // unsigned immediate
    uint32_t sh = 0, mb = 0, me = 0; // rotate/mask
    uint32_t spr = 0;   // special purpose register (mfspr/mtspr)
    uint32_t crfd = 0, crfa = 0, crfb = 0; // CR field indices
    bool lk = false;    // link bit (bl)
    bool aa = false;    // absolute address (b)
    bool oe = false;    // overflow enable
    bool rc = false;    // record (updates CR)
    uint32_t bo = 0, bi = 0;
};

// Decodes one big-endian PowerPC32 instruction at guest address pc.
Insn decode(uint32_t word, int64_t pc);

std::string to_string(const Insn& i);

// Empty if this instruction can't transfer control to a new function.
// Otherwise returns the target PC of a direct call (bl) target.
int64_t call_target(const Insn& i);

// Human-readable condition/BO summary (best effort). Returns "" when the
// branch is not primarily a CR-bit test (e.g. CTR-only bdnz/bdz).
const char* cond_name(const Insn& i);

// Empty if the instruction has no CTR test (bc/bclr/bcctr with BO[2]=0).
// Otherwise returns true branch-takes-on-CTR==0 (bdz family) vs !=0.
bool ctr_test(const Insn& i, bool& branch_on_zero);

// Rotate-left mask for rlwinm/rlwimi/rlwnm (mb..me circular, 32-bit).
uint32_t rot_mask(int mb, int me);

} // namespace rsr