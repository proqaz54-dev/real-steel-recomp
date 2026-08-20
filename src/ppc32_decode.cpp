#include "ppc32_decode.h"

#include <cstdio>
#include <cstring>

namespace rsr {

namespace {

struct Fields {
    uint32_t op;
    uint32_t rt, ra, rb, rs;
    uint32_t xo;    // extended opcode (19/31/59/63)
    uint32_t spr;   // SPR field (5<<5 | 5)
    int16_t d16;
    uint32_t crf_d, crf_a, crf_b; // CR field numbers
};

Fields split(uint32_t w) {
    Fields f;
    f.op    = (w >> 26) & 0x3F;
    f.rt    = (w >> 21) & 0x1F;
    f.ra    = (w >> 16) & 0x1F;
    f.rb    = (w >> 11) & 0x1F;
    f.xo    = (w >> 1) & 0x3FF;
    f.spr   = ((w >> 16) & 0x1F) | (((w >> 11) & 0x1F) << 5);
    f.d16   = static_cast<int16_t>(w & 0xFFFF);
    f.crf_d = (w >> 23) & 0x7;
    f.crf_a = (w >> 18) & 0x7;
    f.crf_b = (w >> 13) & 0x7;
    return f;
}

void common(Insn& i, const Fields& f, uint32_t word) {
    i.raw = word;
    i.rd = f.rt; i.ra = f.ra; i.rb = f.rb;
    i.rs = f.rt; i.rt = f.rt;
    i.rc = word & 1;
}

Op xo31(uint32_t xo) {
    switch (xo) {
    case 0:    return Op::CMPW;
    case 8:    return Op::SUBFC;
    case 10:   return Op::ADDC;
    case 11:   return Op::MULHWU;
    case 19:   return Op::MFCR;
    case 23:   return Op::UNKNOWN; // lwzx (added when X-form loads are done)
    case 24:   return Op::SLW;
    case 26:   return Op::CNTLZW;
    case 27:   return Op::SLD;
    case 28:   return Op::AND;
    case 32:   return Op::CMPLW;
    case 40:   return Op::SUBF;
    case 54:   return Op::UNKNOWN; // dcbst (treated as sync-ish, see xo31_sync)
    case 58:   return Op::CNTLZD;
    case 60:   return Op::ANDC;
    case 75:   return Op::MULHW;
    case 86:   return Op::UNKNOWN; // dcbf
    case 104:  return Op::NEG;
    case 122:  return Op::POPCNTB;
    case 124:  return Op::NOR;
    case 136:  return Op::SUBFE;
    case 138:  return Op::ADDE;
    case 144:  return Op::MTCRF;
    case 200:  return Op::SUBFZE;
    case 202:  return Op::ADDZE;
    case 232:  return Op::SUBFME;
    case 234:  return Op::ADDME;
    case 235:  return Op::MULLW;
    case 266:  return Op::ADD;
    case 279:  return Op::UNKNOWN; // lhzx
    case 284:  return Op::EQV;
    case 316:  return Op::XOR;
    case 339:  return Op::UNKNOWN; // mfspr (resolved by caller)
    case 343:  return Op::UNKNOWN; // lhax
    case 407:  return Op::UNKNOWN; // sthx
    case 412:  return Op::ORC;
    case 444:  return Op::OR;
    case 459:  return Op::DIVWU;
    case 467:  return Op::UNKNOWN; // mtspr (resolved by caller)
    case 476:  return Op::NAND;
    case 491:  return Op::DIVW;
    case 534:  return Op::LWBRX;
    case 536:  return Op::SRW;
    case 539:  return Op::SRD;
    case 598:  return Op::SYNC;
    case 790:  return Op::LHBRX;
    case 792:  return Op::SRAW;
    case 824:  return Op::SRAWI;
    case 918:  return Op::STWBRX;
    case 922:  return Op::EXTSH;
    case 954:  return Op::EXTSB;
    case 986:  return Op::EXTSW;
    default:   return Op::UNKNOWN;
    }
}

Op xo59(uint32_t xo) {
    switch (xo) {
    case 12:   return Op::FRSP;
    case 18:   return Op::PS_DIV;
    case 20:   return Op::PS_SUB;
    case 21:   return Op::PS_ADD;
    case 24:   return Op::FRES;
    case 25:   return Op::PS_MUL;
    case 26:   return Op::UNKNOWN; // ps_rsqrte
    case 28:   return Op::PS_SUB;  // ps_msub
    case 29:   return Op::PS_MADD;
    case 30:   return Op::PS_SUB;  // ps_nmsub
    case 31:   return Op::PS_ADD;  // ps_nmadd
    case 40:   return Op::PS_NEG;
    case 72:   return Op::PS_MR;
    case 264:  return Op::PS_ABS;
    default:   return Op::UNKNOWN;
    }
}

Op xo63(uint32_t xo) {
    switch (xo) {
    case 0:    return Op::FCMPU;
    case 12:   return Op::FRSP;
    case 18:   return Op::FDIV;
    case 20:   return Op::FSUB;
    case 21:   return Op::FADD;
    case 22:   return Op::FSQRT;
    case 28:   return Op::FMSUB;
    case 29:   return Op::FMADD;
    case 30:   return Op::FNMSUB;
    case 31:   return Op::FNMADD;
    case 32:   return Op::FCMPU;   // fcmpo
    case 40:   return Op::FNEG;
    case 72:   return Op::FMR;
    case 136:  return Op::FNEG;    // fnabs
    case 264:  return Op::FABS;
    case 583:  return Op::MFFS;
    case 711:  return Op::MTFSF;
    default:   return Op::UNKNOWN;
    }
}

} // namespace

Insn decode(uint32_t word, int64_t pc) {
    Fields f = split(word);
    Insn i;
    common(i, f, word);
    i.pc = pc;

    if (word == 0x60000000) { i.op = Op::NOP; return i; }

    switch (f.op) {
    case 4:  i.op = Op::TRAP; break;
    case 7:  i.op = Op::MULLI; i.d = f.d16; break;
    case 8:  i.op = Op::SUBFIC; i.d = f.d16; break;
    case 10: i.op = Op::CMPLI; i.uimm = static_cast<uint16_t>(f.d16); i.crfd = f.crf_d; break;
    case 11: i.op = Op::CMPI; i.d = f.d16; i.crfd = f.crf_d; break;
    case 12: i.op = Op::ADDIC; i.d = f.d16; break;
    case 13: i.op = Op::ADDIC; i.d = f.d16; i.rc = true; break;
    case 14: i.op = Op::ADDI; i.d = f.d16; break;
    case 15: i.op = Op::ADDIS; i.d = f.d16; break;

    case 16: { // bc
        i.op = Op::BC;
        i.bo = f.rt;
        i.bi = f.ra;
        i.d = static_cast<int16_t>(word & 0xFFFC);
        i.lk = word & 1;
        i.aa = (word >> 1) & 1;
        break;
    }
    case 17: i.op = Op::SC; break;
    case 18: { // b / bl
        i.op = (word & 1) ? Op::BL : Op::B;
        int32_t li = static_cast<int32_t>(word & 0x03FFFFFC);
        if (li & 0x02000000) li |= 0xFC000000;
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
        } else if (f.xo == 0) {
            i.op = Op::MCRF;
            i.crfd = f.crf_d; i.crfa = f.crf_a;
        } else {
            i.op = Op::UNKNOWN;
        }
        break;
    }
    case 20: i.op = Op::RLWIMI;
        i.sh = f.rb; i.mb = (word >> 6) & 0x1F; i.me = (word >> 1) & 0x1F; break;
    case 21: i.op = Op::RLWINM;
        i.sh = f.rb; i.mb = (word >> 6) & 0x1F; i.me = (word >> 1) & 0x1F; break;
    case 23: i.op = Op::RLWNM;
        i.sh = f.rb; i.mb = (word >> 6) & 0x1F; i.me = (word >> 1) & 0x1F; break;
    case 24: i.op = Op::ORI;   i.uimm = word & 0xFFFF; break;
    case 25: i.op = Op::ORIS;  i.uimm = word & 0xFFFF; break;
    case 26: i.op = Op::XORI;  i.uimm = word & 0xFFFF; break;
    case 27: i.op = Op::XORIS; i.uimm = word & 0xFFFF; break;
    case 28: i.op = Op::ANDI;  i.uimm = word & 0xFFFF; break;
    case 29: i.op = Op::ANDIS; i.uimm = word & 0xFFFF; break;

    case 32: i.op = Op::LWZ;  i.d = f.d16; break;
    case 33: i.op = Op::LWZU; i.d = f.d16; break;
    case 34: i.op = Op::LBZ;  i.d = f.d16; break;
    case 35: i.op = Op::LBZU; i.d = f.d16; break;
    case 36: i.op = Op::STW;  i.rs = f.rt; i.d = f.d16; break;
    case 37: i.op = Op::STWU; i.rs = f.rt; i.d = f.d16; break;
    case 38: i.op = Op::STB;  i.rs = f.rt; i.d = f.d16; break;
    case 39: i.op = Op::STBU; i.rs = f.rt; i.d = f.d16; break;
    case 40: i.op = Op::LHZ;  i.d = f.d16; break;
    case 41: i.op = Op::LHZU; i.d = f.d16; break;
    case 42: i.op = Op::LHA;  i.d = f.d16; break;
    case 43: i.op = Op::LHAU; i.d = f.d16; break;
    case 44: i.op = Op::STH;  i.rs = f.rt; i.d = f.d16; break;
    case 45: i.op = Op::STHU; i.rs = f.rt; i.d = f.d16; break;
    case 46: i.op = Op::LMW;  i.d = f.d16; break;
    case 47: i.op = Op::STMW; i.rs = f.rt; i.d = f.d16; break;

    case 48: i.op = Op::LFS;  i.frt = f.rt; i.d = f.d16; break;
    case 49: i.op = Op::LFSU; i.frt = f.rt; i.d = f.d16; break;
    case 50: i.op = Op::LFD;  i.frt = f.rt; i.d = f.d16; break;
    case 51: i.op = Op::LFDU; i.frt = f.rt; i.d = f.d16; break;
    case 52: i.op = Op::STFS; i.frs = f.rt; i.d = f.d16; break;
    case 53: i.op = Op::STFSU; i.frs = f.rt; i.d = f.d16; break;
    case 54: i.op = Op::STFD; i.frs = f.rt; i.d = f.d16; break;
    case 55: i.op = Op::STFDU; i.frs = f.rt; i.d = f.d16; break;

    case 56: i.op = Op::PSQ_L; i.frt = f.rt; i.d = f.d16; break;
    case 57: i.op = Op::PSQ_L; i.frt = f.rt; i.d = f.d16; break; // psq_lu
    case 60: i.op = Op::PSQ_ST; i.frs = f.rt; i.d = f.d16; break;
    case 61: i.op = Op::PSQ_ST; i.frs = f.rt; i.d = f.d16; break; // psq_stu

    case 58: { // DS-form loads (ld/ldu/lwa)
        uint32_t xo = (word >> 2) & 0x7;
        i.d = static_cast<int16_t>(word & 0xFFFC);
        if (xo == 0) i.op = Op::LD;
        else if (xo == 1) i.op = Op::LDU;
        else if (xo == 2) i.op = Op::LWA;
        else i.op = Op::UNKNOWN;
        break;
    }
    case 62: { // DS-form stores (std/stdu)
        uint32_t xo = (word >> 2) & 0x7;
        i.rs = f.rt;
        i.d = static_cast<int16_t>(word & 0xFFFC);
        if (xo == 0) i.op = Op::STD;
        else if (xo == 1) i.op = Op::STDU;
        else i.op = Op::UNKNOWN;
        break;
    }

    case 31: {
        i.op = xo31(f.xo);
        if (i.op == Op::UNKNOWN && f.xo == 23) i.op = Op::UNKNOWN;
        if (f.xo == 339) i.op = (f.spr == 8) ? Op::MFLR : (f.spr == 9 ? Op::MFCTR : Op::MFSPR);
        if (f.xo == 467) i.op = (f.spr == 8) ? Op::MTLR : (f.spr == 9 ? Op::MTCTR : Op::MTSPR);
        if (f.xo == 144) i.op = Op::MTCRF;
        if (i.op == Op::SRAWI) i.sh = f.rb;
        if (i.op == Op::MFSPR || i.op == Op::MTSPR) i.spr = f.spr;
        if (i.op == Op::MFCR) i.rd = f.rt;
        if (i.op == Op::MTCRF) i.rs = f.rt;
        break;
    }

    case 59: {
        i.op = xo59(f.xo);
        i.frd = f.rt; i.fra = f.ra; i.frb = f.rb;
        i.frs = f.rt;
        break;
    }
    case 63: {
        i.op = xo63(f.xo);
        i.frd = f.rt; i.fra = f.ra; i.frb = f.rb;
        i.frs = f.rt;
        if (i.op == Op::FCMPU) i.crfd = f.crf_d;
        break;
    }
    default:
        i.op = Op::UNKNOWN;
        break;
    }

    i.oe = (word >> 10) & 1;

    // X-form load/store (opcode 31) that weren't in xo31 table:
    if (f.op == 31 && i.op == Op::UNKNOWN) {
        switch (f.xo) {
        case 23:  i.op = Op::UNKNOWN; break; // lwzx -> TODO (revisit later)
        case 55:  i.op = Op::UNKNOWN; break;
        case 87:  i.op = Op::UNKNOWN; break;
        case 119: i.op = Op::UNKNOWN; break;
        case 279: i.op = Op::UNKNOWN; break;
        case 311: i.op = Op::UNKNOWN; break;
        case 343: i.op = Op::UNKNOWN; break;
        case 375: i.op = Op::UNKNOWN; break;
        case 151: i.op = Op::UNKNOWN; break;
        case 183: i.op = Op::UNKNOWN; break;
        case 215: i.op = Op::UNKNOWN; break;
        case 247: i.op = Op::UNKNOWN; break;
        case 407: i.op = Op::UNKNOWN; break;
        case 439: i.op = Op::UNKNOWN; break;
        default: break;
        }
    }

    return i;
}

int64_t call_target(const Insn& i) {
    if (i.op != Op::BL) return -1;
    return i.aa ? (i.d & 0xFFFFFFFF) : (i.pc + i.d);
}

const char* cond_name(const Insn& i) {
    // bi -> CR bit: (bi>>2) = field, bi&3: 0=LT 1=GT 2=EQ 3=SO
    bool branch_if_true = (i.bo & 0x10) != 0; // BO bit4 set => cr true
    bool negate = !branch_if_true;
    switch (i.bi & 3) {
    case 0: return negate ? "ge" : "lt";
    case 1: return negate ? "le" : "gt";
    case 2: return negate ? "ne" : "eq";
    default: return negate ? "vc" : "vs";
    }
}

std::string to_string(const Insn& i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%08x", i.raw);
    (void)buf;
    const char* n = "????";
    switch (i.op) {
    case Op::ADD: n = "add"; break;
    case Op::ADDC: n = "addc"; break;
    case Op::ADDE: n = "adde"; break;
    case Op::ADDZE: n = "addze"; break;
    case Op::ADDME: n = "addme"; break;
    case Op::ADDI: n = "addi"; break;
    case Op::ADDIS: n = "addis"; break;
    case Op::ADDIC: n = "addic"; break;
    case Op::SUBF: n = "subf"; break;
    case Op::SUBFC: n = "subfc"; break;
    case Op::SUBFE: n = "subfe"; break;
    case Op::SUBFZE: n = "subfze"; break;
    case Op::SUBFME: n = "subfme"; break;
    case Op::SUBFIC: n = "subfic"; break;
    case Op::NEG: n = "neg"; break;
    case Op::MULLI: n = "mulli"; break;
    case Op::MULLW: n = "mullw"; break;
    case Op::MULHW: n = "mulhw"; break;
    case Op::MULHWU: n = "mulhwu"; break;
    case Op::DIVW: n = "divw"; break;
    case Op::DIVWU: n = "divwu"; break;
    case Op::AND: n = "and"; break;
    case Op::ANDC: n = "andc"; break;
    case Op::ANDI: n = "andi."; break;
    case Op::ANDIS: n = "andis."; break;
    case Op::OR: n = "or"; break;
    case Op::ORC: n = "orc"; break;
    case Op::ORI: n = "ori"; break;
    case Op::ORIS: n = "oris"; break;
    case Op::XOR: n = "xor"; break;
    case Op::XORI: n = "xori"; break;
    case Op::XORIS: n = "xoris"; break;
    case Op::NOR: n = "nor"; break;
    case Op::NAND: n = "nand"; break;
    case Op::EQV: n = "eqv"; break;
    case Op::CNTLZW: n = "cntlzw"; break;
    case Op::CNTLZD: n = "cntlzd"; break;
    case Op::POPCNTB: n = "popcntb"; break;
    case Op::EXTSB: n = "extsb"; break;
    case Op::EXTSH: n = "extsh"; break;
    case Op::EXTSW: n = "extsw"; break;
    case Op::SLW: n = "slw"; break;
    case Op::SLD: n = "sld"; break;
    case Op::SRW: n = "srw"; break;
    case Op::SRD: n = "srd"; break;
    case Op::SRAW: n = "sraw"; break;
    case Op::SRAWI: n = "srawi"; break;
    case Op::RLWINM: n = "rlwinm"; break;
    case Op::RLWIMI: n = "rlwimi"; break;
    case Op::RLWNM: n = "rlwnm"; break;
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
    case Op::LHZ: n = "lhz"; break;
    case Op::LHZU: n = "lhzu"; break;
    case Op::LHA: n = "lha"; break;
    case Op::LHAU: n = "lhau"; break;
    case Op::STH: n = "sth"; break;
    case Op::STHU: n = "sthu"; break;
    case Op::LMW: n = "lmw"; break;
    case Op::STMW: n = "stmw"; break;
    case Op::LWBRX: n = "lwbrx"; break;
    case Op::LHBRX: n = "lhbrx"; break;
    case Op::STWBRX: n = "stwbrx"; break;
    case Op::LD: n = "ld"; break;
    case Op::LDU: n = "ldu"; break;
    case Op::LWA: n = "lwa"; break;
    case Op::STD: n = "std"; break;
    case Op::STDU: n = "stdu"; break;
    case Op::LFS: n = "lfs"; break;
    case Op::LFSU: n = "lfsu"; break;
    case Op::LFD: n = "lfd"; break;
    case Op::LFDU: n = "lfdu"; break;
    case Op::STFS: n = "stfs"; break;
    case Op::STFSU: n = "stfsu"; break;
    case Op::STFD: n = "stfd"; break;
    case Op::STFDU: n = "stfdu"; break;
    case Op::FADD: n = "fadd"; break;
    case Op::FSUB: n = "fsub"; break;
    case Op::FMUL: n = "fmul"; break;
    case Op::FDIV: n = "fdiv"; break;
    case Op::FSQRT: n = "fsqrt"; break;
    case Op::FRES: n = "fres"; break;
    case Op::FRSP: n = "frsp"; break;
    case Op::FABS: n = "fabs"; break;
    case Op::FNEG: n = "fneg"; break;
    case Op::FMR: n = "fmr"; break;
    case Op::FCMPU: n = "fcmpu"; break;
    case Op::FMADD: n = "fmadd"; break;
    case Op::FMSUB: n = "fmsub"; break;
    case Op::FNMADD: n = "fnmadd"; break;
    case Op::FNMSUB: n = "fnmsub"; break;
    case Op::MFFS: n = "mffs"; break;
    case Op::MTFSF: n = "mtfsf"; break;
    case Op::PS_ADD: n = "ps_add"; break;
    case Op::PS_SUB: n = "ps_sub"; break;
    case Op::PS_MUL: n = "ps_mul"; break;
    case Op::PS_DIV: n = "ps_div"; break;
    case Op::PS_ABS: n = "ps_abs"; break;
    case Op::PS_NEG: n = "ps_neg"; break;
    case Op::PS_MR: n = "ps_mr"; break;
    case Op::PS_MADD: n = "ps_madd"; break;
    case Op::PSQ_L: n = "psq_l"; break;
    case Op::PSQ_ST: n = "psq_st"; break;
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
    case Op::MFCR: n = "mfcr"; break;
    case Op::MTCRF: n = "mtcrf"; break;
    case Op::MCRF: n = "mcrf"; break;
    case Op::SYNC: n = "sync"; break;
    case Op::ISYNC: n = "isync"; break;
    case Op::TRAP: n = "trap"; break;
    case Op::SC: n = "sc"; break;
    default: break;
    }
    std::string out = n;
    return out;
}

} // namespace rsr