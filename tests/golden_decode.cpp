#include "ppc32_decode.h"
#include <cstdio>
#include <string>

using rsr::Op;

namespace build {
unsigned D(unsigned op, unsigned rt, unsigned ra, int imm) {
    return (op << 26) | (rt << 21) | (ra << 16) | (imm & 0xFFFF);
}
unsigned X(unsigned xo, unsigned rt, unsigned ra, unsigned rb, bool rc = false) {
    return (31 << 26) | (rt << 21) | (ra << 16) | (rb << 11) | (xo << 1) | (rc ? 1 : 0);
}
unsigned XF(unsigned xo, unsigned frt, unsigned fra, unsigned frb, bool rc = false) {
    return (63 << 26) | (frt << 21) | (fra << 16) | (frb << 11) | (xo << 1) | (rc ? 1 : 0);
}
unsigned XS(unsigned xo, unsigned frt, unsigned fra, unsigned frb, bool rc = false) {
    return (59 << 26) | (frt << 21) | (fra << 16) | (frb << 11) | (xo << 1) | (rc ? 1 : 0);
}
unsigned I(unsigned op, int li, bool abs = false, bool lk = false) {
    return (op << 26) | (li & 0x03FFFFFC) | (abs ? 2 : 0) | (lk ? 1 : 0);
}
unsigned BC(unsigned bo, unsigned bi, int bd) {
    return (16 << 26) | (bo << 21) | (bi << 16) | (bd & 0xFFFC);
}
unsigned M(unsigned sh, unsigned mb, unsigned me, unsigned rt, unsigned ra, bool rc = false) {
    return (21 << 26) | (rt << 21) | (ra << 16) | (sh << 11) | (mb << 6) | (me << 1) | (rc ? 1 : 0);
}
// XFX mfspr/mtspr: XO at bits 1-10 (LSB), spr = (hi<<5)|lo split b11-15/b16-20.
unsigned SPR(unsigned xo, unsigned rt, unsigned spr) {
    unsigned lo = spr & 0x1F, hi = (spr >> 5) & 0x1F;
    return (31 << 26) | (rt << 21) | (lo << 16) | (hi << 11) | (xo << 1);
}
}

struct Case { unsigned word; const char* expect; };

// Opcode numbers verified against Xenia ppc_opcode_table_gen.cc.
static const Case kCases[] = {
    // D-form integer
    {build::D(14, 5, 4, -32768), "addi"},
    {build::D(14, 5, 0, 0), "addi"},
    {build::D(15, 5, 4, -8), "addis"},
    {build::D(12, 3, 4, 100), "addic"},
    {build::D(7, 3, 4, 100), "mulli"},
    {build::D(8, 3, 4, 100), "subfic"},
    {build::D(24, 3, 4, 0x1234), "ori"},
    {build::D(25, 3, 4, 0x1234), "oris"},
    {build::D(26, 3, 4, 0x1234), "xori"},
    {build::D(27, 3, 4, 0x1234), "xoris"},
    {build::D(28, 5, 4, 0x00FF), "andi."},
    {build::D(29, 5, 4, 0x00FF), "andis."},
    {build::D(10, 3, 4, 100), "cmpli"},
    {build::D(11, 3, 4, 100), "cmpi"},
    // X-form integer (XO from Xenia)
    {build::X(266, 3, 3, 3), "add"},
    {build::X(10, 3, 3, 3), "addc"},
    {build::X(138, 3, 3, 3), "adde"},
    {build::X(40, 3, 3, 3), "subf"},
    {build::X(104, 3, 3, 3), "neg"},
    {build::X(235, 3, 3, 3), "mullw"},
    {build::X(75, 3, 3, 3), "mulhw"},
    {build::X(11, 3, 3, 3), "mulhwu"},
    {build::X(459, 3, 3, 3), "divwu"},
    {build::X(491, 3, 3, 3), "divw"},
    {build::X(28, 3, 3, 3), "and"},
    {build::X(60, 3, 3, 3), "andc"},
    {build::X(444, 3, 3, 3), "or"},
    {build::X(124, 3, 3, 3), "nor"},
    {build::X(476, 3, 3, 3), "nand"},
    {build::X(316, 3, 3, 3), "xor"},
    {build::X(284, 3, 3, 3), "eqv"},
    {build::X(412, 3, 3, 3), "orc"},
    {build::X(24, 3, 4, 5), "slw"},
    {build::X(536, 3, 4, 5), "srw"},
    {build::X(792, 3, 4, 5), "sraw"},
    {build::X(824, 3, 4, 0), "srawi"},
    {build::X(26, 3, 4, 0), "cntlzw"},
    {build::X(922, 3, 4, 0), "extsh"},
    {build::X(954, 3, 4, 0), "extsb"},
    {build::X(986, 3, 4, 0), "extsw"},
    {build::X(68, 3, 4, 5), "????"},   // reserved -> unknown
    {build::X(0, 3, 4, 5), "cmpw"},
    {build::X(32, 3, 4, 5), "cmplw"},
    // branches
    {build::BC(12, 2, 0x10), "bc"},
    {build::BC(4, 2, 0x10), "bc"},
    {build::BC(16, 0, 0xFFF8), "bc"},   // bdnz
    {build::BC(18, 0, 0xFFF8), "bc"},   // bdz
    {0x4E800020, "bclr"},               // blr: BCLR BO=20 BI=0 bd=0
    {0x4E800420, "bctr"},               // bctr
    {build::I(18, 0x10), "b"},
    {build::I(18, 0x10, false, true), "bl"},
    {build::I(18, -0x10, true), "b"},
    {build::I(18, 0x10, false, true), "bl"},
    // loads/stores
    {build::D(32, 3, 1, 8), "lwz"},
    {build::D(33, 3, 1, 8), "lwzu"},
    {build::D(34, 3, 1, 8), "lbz"},
    {build::D(35, 3, 1, 8), "lbzu"},
    {build::D(36, 3, 1, 8), "stw"},
    {build::D(37, 3, 1, 8), "stwu"},
    {build::D(38, 3, 1, 8), "stb"},
    {build::D(39, 3, 1, 8), "stbu"},
    {build::D(40, 3, 1, 8), "lhz"},
    {build::D(41, 3, 1, 8), "lhzu"},
    {build::D(42, 3, 1, 8), "lha"},
    {build::D(43, 3, 1, 8), "lhau"},
    {build::D(44, 3, 1, 8), "sth"},
    {build::D(45, 3, 1, 8), "sthu"},
    {build::D(46, 20, 1, 8), "lmw"},
    {build::D(47, 20, 1, 8), "stmw"},
    {build::X(23, 6, 7, 8), "lwzx"},
    {build::X(55, 6, 7, 8), "lwzux"},
    {build::X(87, 6, 7, 8), "lbzx"},
    {build::X(119, 6, 7, 8), "lbzux"},
    {build::X(279, 6, 7, 8), "lhzx"},
    {build::X(311, 6, 7, 8), "lhzux"},
    {build::X(343, 6, 7, 8), "lhax"},
    {build::X(375, 6, 7, 8), "lhaux"},
    {build::X(151, 9, 10, 11), "stwx"},
    {build::X(183, 9, 10, 11), "stwux"},
    {build::X(215, 9, 10, 11), "stbx"},
    {build::X(247, 9, 10, 11), "stbux"},
    {build::X(407, 9, 10, 11), "sthx"},
    {build::X(439, 9, 10, 11), "sthux"},
    {build::X(20, 3, 4, 5), "lwarx"},
    {build::X(150, 3, 4, 5), "stwcx."},
    {build::X(21, 3, 4, 5), "ldx"},
    {build::X(53, 3, 4, 5), "ldux"},
    {build::X(149, 3, 4, 5), "stdx"},
    {build::X(534, 3, 4, 5), "lwbrx"},
    {build::X(790, 3, 4, 5), "lhbrx"},
    {build::X(918, 3, 4, 5), "stwbrx"},
    // rotate
    {build::M(16, 16, 31, 5, 4), "rlwinm"},
    {build::M(16, 0, 31, 5, 4), "rlwinm"},
    // FPU double (primary 63)
    {build::XF(21, 0, 0, 0), "fadd"},
    {build::XF(20, 0, 0, 0), "fsub"},
    {build::XF(25, 0, 0, 0), "fmul"},
    {build::XF(18, 0, 0, 0), "fdiv"},
    {build::XF(22, 0, 0, 0), "fsqrt"},
    {build::XF(72, 0, 0, 0), "fmr"},
    {build::XF(264, 0, 0, 0), "fabs"},
    {build::XF(40, 0, 0, 0), "fneg"},
    {build::XF(136, 0, 0, 0), "fnabs"},
    {build::XF(29, 0, 0, 0), "fmadd"},
    {build::XF(28, 0, 0, 0), "fmsub"},
    {build::XF(31, 0, 0, 0), "fnmadd"},
    {build::XF(30, 0, 0, 0), "fnmsub"},
    {build::XF(12, 0, 0, 0), "frsp"},
    {build::XF(24, 0, 0, 0), "????"},   // fres is 59-only; 63 XO 24 stays unknown
    {build::XF(26, 0, 0, 0), "frsqrte"},
    {build::XF(0, 0, 0, 0), "fcmpu"},
    {build::XF(32, 0, 0, 0), "fcmpu"},
    // single precision (primary 59)
    {build::XS(21, 0, 0, 0), "fadd"},
    {build::XS(20, 0, 0, 0), "fsub"},
    {build::XS(25, 0, 0, 0), "fmul"},
    {build::XS(18, 0, 0, 0), "fdiv"},
    {build::XS(22, 0, 0, 0), "fsqrt"},
    {build::XS(24, 0, 0, 0), "fres"},
    {build::XS(29, 0, 0, 0), "fmadd"},
    {build::XS(28, 0, 0, 0), "fmsub"},
    // SPR
    {build::SPR(339, 0, 8), "mflr"},    // 0x7C0802A6
    {build::SPR(467, 8, 8), "mtlr"},
    {build::SPR(339, 3, 9), "mfctr"},
    {build::SPR(467, 3, 9), "mtctr"},
    {0x7C0802A6, "mflr"},               // canonical mflr r0
    // CR
    {0x4C000000, "mcrf"},               // mcrf cr0,cr0? primary 19 XO 0
    {0x7C000400, "mcrxr"},   // mcrxr = XO 512
    // misc
    {0x7C0004AC, "sync"},
    {0x4C00012C, "isync"},
    {0x7C0007EC, "dcbz"},               // dcbz = XO 1014 (not 1016)
    {0x7C0006AC, "eieio"},
    {0x44000000, "sc"},                 // sc = primary 17
    {0x0C000000, "tw"},                 // twi = primary 3
    {build::X(4, 31, 31, 31), "tw"},    // tw = X-form XO 4
    {0x4BFFFFFD, "bl"},
};

int main() {
    int fail = 0;
    for (const auto& c : kCases) {
        auto insn = rsr::decode(c.word, 0x10000);
        std::string got = rsr::to_string(insn);
        if (got != c.expect) {
            std::printf("FAIL %08x: expected '%s' got '%s'\n", c.word, c.expect, got.c_str());
            if (++fail >= 40) { std::printf("too many failures, stopping\n"); return 1; }
        }
    }
    if (fail == 0) {
        std::printf("all %zu golden decode tests passed\n", sizeof(kCases) / sizeof(kCases[0]));
        return 0;
    }
    std::printf("%d failures\n", fail);
    return 1;
}
