#pragma once

#include <cstdint>
#include <string>

namespace rsr {

enum class Op {
    UNKNOWN,
    // integer
    ADD, ADDI, ADDIS, ADDIC, SUBF, SUBFIC, MULLI, MULLW, DIVW, NEG,
    AND, ANDC, OR, ORC, XOR, NOR, NAND, EQV, CNTLZW, EXTSB, EXTSH,
    ORI, ORIS, XORI, XORIS,
    SLW, SRW, SRAW, SRAWI, RLWINM, RLWIMI,
    // branch
    B, BL, BC, BCLR, BCTR, NOP,
    // load/store
    LWZ, LWZU, STW, STWU, LBZ, LBZU, STB, STBU, LHA, LHAU,
    LD, LDU, LWA, STD, STDU,
    // special
    MFLR, MTLR, MFCTR, MTCTR, MFSPR, MTSPR,
    CMPW, CMPLW, CMPI, CMPLI,
    SYNC, ISYNC,
};

struct Insn {
    Op op = Op::UNKNOWN;
    uint32_t raw = 0;
    int64_t pc = 0;

    int rd = 0, ra = 0, rb = 0, rs = 0, rt = 0; // register fields
    int64_t d = 0;      // sign-extended displacement / branch offset
    uint32_t uimm = 0;  // unsigned immediate
    uint32_t sh = 0, mb = 0, me = 0; // rotate/mask
    uint32_t spr = 0;   // special purpose register (mfspr/mtspr)
    bool lk = false;    // link bit (bl)
    bool aa = false;    // absolute address (b)
    uint32_t bo = 0, bi = 0;
};

// Decodes one big-endian PowerPC32 instruction at guest address pc.
Insn decode(uint32_t word, int64_t pc);

std::string to_string(const Insn& i);

} // namespace rsr