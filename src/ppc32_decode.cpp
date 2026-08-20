#include "ppc32_decode.h"

#include <cstdio>
#include <cstring>

namespace rsr {

namespace {

// Extract register fields from a big-endian word (host bytes -> guest bits).
struct Fields {
    uint32_t op;       // primary opcode (bits 26..31)
    uint32_t rt, ra, rb, rs;
    uint32_t xo;       // extended opcode (opcodes 19/30/31)
    uint32_t spr;
    int16_t d16;       // D-form displacement
};

Fields split(uint32_t w) {
    Fields f;
    f.op  = (w >> 26) & 0x3F;
    f.rt  = (w >> 21) & 0x1F;
    f.rs  = (w >> 21) & 0x1F;
    f.ra  = (w >> 16) & 0x1F;
    f.rb  = (w >> 11) & 0x1F;
    f.xo  = (w >> 1) & 0x3FF;
    f.spr = ((w >> 16) & 0x1F) | (((w >> 11) & 0x1F) << 5);
    f.d16 = static_cast<int16_t>(w & 0xFFFF);
    return f;
}

} // namespace

Insn decode(uint32_t word, int64_t pc) {
    Fields f = split(word);
    Insn i;
    i.raw = word;
    i.pc = pc;
    i.rd = f.rt;
    i.ra = f.ra;
    i.rb = f.rb;
    i.rs = f.rs;
    i.rt = f.rt;

    switch (f.op) {
    case 7:  i.op = Op::MULLI;  i.d = f.d16; i.rd = f.rt; i.ra = f.ra; break;
    case 8:  i.op = Op::SUBFIC; i.d = f.d16; break;
    case 10: i.op = Op::CMPLI;  i.uimm = f.d16; break; // cmpli L=0
    case 11: i.op = Op::CMPI;   i.d = f.d16; break;
    case 12: i.op = Op::ADDIC;  i.d = f.d16; break;
    case 14: i.op = Op::ADDI;   i.d = f.d16; break;
    case 15: i.op = Op::ADDIS;  i.d = f.d16; break;

    case 16: { // bc
        i.op = Op::BC;
        i.bo = f.rt;
        i.bi = f.ra;
        i.d = static_cast<int16_t>(word & 0xFFFC);
        i.lk = word & 1;
        break;
    }
    case 18: { // b / bl
        i.op = (word & 1) ? Op::BL : Op::B;
        int32_t li = static_cast<int32_t>(word & 0x03FFFFFC);
        if (li & 0x02000000) li |= 0xFC000000; // sign-extend 26-bit
        i.d = li;
        i.lk = word & 1;
        i.aa = (word >> 1) & 1;
        break;
    }
    case 19: { // X-form specials
        if (f.xo == 16) {
            i.op = Op::BCLR;
            i.bo = f.rt; i.bi = f.ra;
            i.lk = word & 1;
        } else if (f.xo == 528) {
            i.op = Op::BCTR;
            i.bo = f.rt; i.bi = f.ra;
            i.lk = word & 1;
        } else if (f.xo == 150) {
            i.op = Op::ISYNC;
        } else {
            i.op = Op::UNKNOWN;
        }
        break;
    }
    case 20: i.op = Op::RLWIMI; i.sh = f.rb; i.mb = (word >> 6) & 0x1F; i.me = (word >> 1) & 0x1F; break;
    case 21: i.op = Op::RLWINM; i.sh = f.rb; i.mb = (word >> 6) & 0x1F; i.me = (word >> 1) & 0x1F; break;
    case 24: i.op = Op::ORI;   i.uimm = word & 0xFFFF; break;
    case 25: i.op = Op::ORIS;  i.uimm = word & 0xFFFF; break;
    case 26: i.op = Op::XORI;  i.uimm = word & 0xFFFF; break;
    case 27: i.op = Op::XORIS; i.uimm = word & 0xFFFF; break;
    case 28: i.op = Op::AND;   i.uimm = word & 0xFFFF; break; // andi.
    case 29: i.op = Op::AND;   i.uimm = word & 0xFFFF; break; // andis.

    case 32: i.op = Op::LWZ; i.d = f.d16; break;
    case 33: i.op = Op::LWZU; i.d = f.d16; break;
    case 34: i.op = Op::LBZ; i.d = f.d16; break;
    case 35: i.op = Op::LBZU; i.d = f.d16; break;
    case 36: i.op = Op::STW; i.rs = f.rt; i.d = f.d16; break;
    case 37: i.op = Op::STWU; i.rs = f.rt; i.d = f.d16; break;
    case 38: i.op = Op::STB; i.rs = f.rt; i.d = f.d16; break;
    case 39: i.op = Op::STBU; i.rs = f.rt; i.d = f.d16; break;
    case 42: i.op = Op::LHA; i.d = f.d16; break;
    case 43: i.op = Op::LHAU; i.d = f.d16; break;

    case 58: { // DS-form loads
        uint32_t xo = (word >> 2) & 0x7;
        i.d = static_cast<int16_t>(word & 0xFFFC);
        if (xo == 0) i.op = Op::LD;
        else if (xo == 1) i.op = Op::LDU;
        else if (xo == 2) i.op = Op::LWA;
        else i.op = Op::UNKNOWN;
        break;
    }
    case 62: { // DS-form stores
        uint32_t xo = (word >> 2) & 0x7;
        i.rs = f.rt;
        i.d = static_cast<int16_t>(word & 0xFFFC);
        if (xo == 0) i.op = Op::STD;
        else if (xo == 1) i.op = Op::STDU;
        else i.op = Op::UNKNOWN;
        break;
    }

    case 31: { // X-form
        switch (f.xo) {
        case 0:   i.op = Op::CMPW;  break;
        case 32:  i.op = Op::CMPLW; break;
        case 40:  i.op = Op::SUBF;  break;
        case 8:   i.op = Op::SUBF;  break; // subfc (demo: same shape)
        case 24:  i.op = Op::SLW;   break;
        case 26:  i.op = Op::CNTLZW; break;
        case 28:  i.op = Op::AND;   break;
        case 60:  i.op = Op::ANDC;  break;
        case 104: i.op = Op::NEG;   break;
        case 124: i.op = Op::NOR;   break;
        case 235: i.op = Op::MULLW; break;
        case 266: i.op = Op::ADD;   break;
        case 284: i.op = Op::EQV;   break;
        case 316: i.op = Op::XOR;   break;
        case 339:
            i.op = (f.spr == 8) ? Op::MFLR : (f.spr == 9 ? Op::MFCTR : Op::MFSPR);
            i.spr = f.spr;
            break;
        case 412: i.op = Op::ORC;   break;
        case 444: i.op = Op::OR;    break;
        case 467:
            i.op = (f.spr == 8) ? Op::MTLR : (f.spr == 9 ? Op::MTCTR : Op::MTSPR);
            i.spr = f.spr;
            break;
        case 476: i.op = Op::NAND;  break;
        case 491: i.op = Op::DIVW;  break;
        case 536: i.op = Op::SRW;   break;
        case 792: i.op = Op::SRAW;  break;
        case 824: i.op = Op::SRAWI; i.sh = f.rb; break;
        case 922: i.op = Op::EXTSH; break;
        case 954: i.op = Op::EXTSB; break;
        case 598: i.op = Op::SYNC;  break;
        default:  i.op = Op::UNKNOWN; break;
        }
        break;
    }
    default:
        i.op = Op::UNKNOWN;
        break;
    }

    if (word == 0x60000000) i.op = Op::NOP; // ori r0,r0,0
    return i;
}

std::string to_string(const Insn& i) {
    char buf[160];
    const char* n = "????";
    switch (i.op) {
    case Op::ADD: n = "add"; break;
    case Op::ADDI: n = "addi"; break;
    case Op::ADDIS: n = "addis"; break;
    case Op::ADDIC: n = "addic"; break;
    case Op::SUBF: n = "subf"; break;
    case Op::SUBFIC: n = "subfic"; break;
    case Op::MULLI: n = "mulli"; break;
    case Op::MULLW: n = "mullw"; break;
    case Op::DIVW: n = "divw"; break;
    case Op::NEG: n = "neg"; break;
    case Op::AND: n = "and"; break;
    case Op::ANDC: n = "andc"; break;
    case Op::OR: n = "or"; break;
    case Op::ORI: n = "ori"; break;
    case Op::ORIS: n = "oris"; break;
    case Op::XORI: n = "xori"; break;
    case Op::XORIS: n = "xoris"; break;
    case Op::ORC: n = "orc"; break;
    case Op::XOR: n = "xor"; break;
    case Op::NOR: n = "nor"; break;
    case Op::NAND: n = "nand"; break;
    case Op::EQV: n = "eqv"; break;
    case Op::CNTLZW: n = "cntlzw"; break;
    case Op::EXTSB: n = "extsb"; break;
    case Op::EXTSH: n = "extsh"; break;
    case Op::SLW: n = "slw"; break;
    case Op::SRW: n = "srw"; break;
    case Op::SRAW: n = "sraw"; break;
    case Op::SRAWI: n = "srawi"; break;
    case Op::RLWINM: n = "rlwinm"; break;
    case Op::RLWIMI: n = "rlwimi"; break;
    case Op::B: n = "b"; break;
    case Op::BL: n = "bl"; break;
    case Op::BC: n = "bc"; break;
    case Op::BCLR: n = "bclr"; break;
    case Op::BCTR: n = "bctr"; break;
    case Op::NOP: n = "nop"; break;
    case Op::LWZ: n = "lwz"; break;
    case Op::LWZU: n = "lwzu"; break;
    case Op::STW: n = "stw"; break;
    case Op::STWU: n = "stwu"; break;
    case Op::LBZ: n = "lbz"; break;
    case Op::LBZU: n = "lbzu"; break;
    case Op::STB: n = "stb"; break;
    case Op::STBU: n = "stbu"; break;
    case Op::LHA: n = "lha"; break;
    case Op::LHAU: n = "lhau"; break;
    case Op::LD: n = "ld"; break;
    case Op::LDU: n = "ldu"; break;
    case Op::LWA: n = "lwa"; break;
    case Op::STD: n = "std"; break;
    case Op::STDU: n = "stdu"; break;
    case Op::MFLR: n = "mflr"; break;
    case Op::MTLR: n = "mtlr"; break;
    case Op::MFCTR: n = "mfctr"; break;
    case Op::MTCTR: n = "mtctr"; break;
    case Op::MFSPR: n = "mfspr"; break;
    case Op::MTSPR: n = "mtspr"; break;
    case Op::CMPW: n = "cmpw"; break;
    case Op::CMPLW: n = "cmplw"; break;
    case Op::CMPI: n = "cmpi"; break;
    case Op::CMPLI: n = "cmpli"; break;
    case Op::SYNC: n = "sync"; break;
    case Op::ISYNC: n = "isync"; break;
    default: break;
    }
    snprintf(buf, sizeof(buf), "%08x: %-8s", i.raw, n);
    return std::string(buf);
}

} // namespace rsr