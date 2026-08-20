#include "arm64_emit.h"

#include <cstdio>
#include <string>

namespace rsr {

// Interim register map (final ABI TBD):
//   PPC r0..r30 -> X0..X30   (32-bit ops use W0..W30)
//   PPC r31 (stack pointer) -> X19
//   PPC lr  -> host LR (X30)
//   PPC ctr -> X20
//   PPC fr0..fr31 -> d0..d31 / s0..s31
namespace {

std::string xr(int r) {
    if (r == 31) return "x19"; // guest stack pointer
    return "x" + std::to_string(r);
}

std::string wr(int r) {
    if (r == 31) return "w19"; // guest stack pointer (low half)
    return "w" + std::to_string(r);
}

std::string dr(int fr) { return "d" + std::to_string(fr); }
std::string sr(int fr) { return "s" + std::to_string(fr); }

} // namespace

std::string emit_arm64(const Insn& i, const std::string& label) {
    std::string src = to_string(i);
    std::string out;

    auto line = [&](const std::string& body) {
        out = "  " + body;
        while (out.size() < 48) out += ' ';
        out += "// " + src;
        return out;
    };

    switch (i.op) {
    case Op::NOP: return line("nop");

    case Op::ADD: return line("add " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::ADDC: return line("add " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb) + "  // TODO: carry flag");
    case Op::ADDE: return line("add " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb) + "  // TODO: carry in/out");
    case Op::ADDZE: return line("add " + wr(i.rd) + ", " + wr(i.ra) + ", xzr // TODO: carry");
    case Op::ADDME: return line("add " + wr(i.rd) + ", " + wr(i.ra) + ", xzr // TODO: carry");
    case Op::ADDI:
        if (i.ra == 0) return line("mov " + wr(i.rt) + ", #" + std::to_string(i.d));
        return line("add " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.d));
    case Op::ADDIS:
        return line("movz " + wr(i.rt) + ", #" + std::to_string(i.d & 0xFFFF) + ", lsl #16");
    case Op::ADDIC: return line("add " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.d) + " // TODO: carry");
    case Op::SUBF: return line("sub " + wr(i.rd) + ", " + wr(i.rb) + ", " + wr(i.ra));
    case Op::SUBFC:
    case Op::SUBFE:
    case Op::SUBFZE:
    case Op::SUBFME:
        return line("sub " + wr(i.rd) + ", " + wr(i.rb) + ", " + wr(i.ra) + "  // TODO: carry semantics");
    case Op::SUBFIC: return line("sub " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.d) + " // TODO: carry");
    case Op::NEG: return line("neg " + wr(i.rd) + ", " + wr(i.ra));
    case Op::MULLW: return line("mul " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::MULLI: return line("mul " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.d));
    case Op::MULHW: return line("smull " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb) + " // high 32 result");
    case Op::MULHWU: return line("umull " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb) + " // high 32 result");
    case Op::DIVW: return line("sdiv " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::DIVWU: return line("udiv " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));

    case Op::AND:  return line("and " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::ANDC: return line("bic " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::ANDI: return line("and " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.uimm));
    case Op::ANDIS: return line("and " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.uimm) + ", lsl #16");
    case Op::OR:   return line("orr " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::ORC:  return line("orn " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::ORI:  return line("orr " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.uimm));
    case Op::ORIS: return line("orr " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.uimm) + ", lsl #16");
    case Op::XOR:  return line("eor " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));
    case Op::XORI: return line("eor " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.uimm));
    case Op::XORIS: return line("eor " + wr(i.rt) + ", " + wr(i.ra) + ", #" + std::to_string(i.uimm) + ", lsl #16");
    case Op::NOR:  return line("orn " + wr(i.rd) + ", " + wr(i.rb) + ", " + wr(i.ra) + " // TODO: NOR");
    case Op::NAND: return line("bic " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb) + " // TODO: NAND");
    case Op::EQV:  return line("eon " + wr(i.rd) + ", " + wr(i.ra) + ", " + wr(i.rb));

    case Op::SLW: return line("lsl " + wr(i.rd) + ", " + wr(i.rs) + ", " + wr(i.rb));
    case Op::SLD: return line("lsl " + xr(i.rd) + ", " + xr(i.rs) + ", " + xr(i.rb));
    case Op::SRW: return line("lsr " + wr(i.rd) + ", " + wr(i.rs) + ", " + wr(i.rb));
    case Op::SRD: return line("lsr " + xr(i.rd) + ", " + xr(i.rs) + ", " + xr(i.rb));
    case Op::SRAW: return line("asr " + wr(i.rd) + ", " + wr(i.rs) + ", " + wr(i.rb));
    case Op::SRAWI: return line("asr " + wr(i.rd) + ", " + wr(i.rs) + ", #" + std::to_string(i.sh));
    case Op::RLWINM: return line("// TODO rlwinm");
    case Op::RLWIMI: return line("// TODO rlwimi");
    case Op::RLWNM: return line("// TODO rlwnm");
    case Op::CNTLZW: return line("clz " + wr(i.rd) + ", " + wr(i.rs));
    case Op::CNTLZD: return line("clz " + xr(i.rd) + ", " + xr(i.rs));
    case Op::POPCNTB: return line("// TODO popcntb");
    case Op::EXTSB: return line("sxtb " + wr(i.rd) + ", " + wr(i.rs));
    case Op::EXTSH: return line("sxth " + wr(i.rd) + ", " + wr(i.rs));
    case Op::EXTSW: return line("sxtw " + xr(i.rd) + ", " + wr(i.rs));

    case Op::B:
    case Op::BL:
        if (label.empty()) return line(i.op == Op::BL ? "bl" : "b");
        return line(std::string(i.op == Op::BL ? "bl " : "b ") + label);
    case Op::BC:
        if (label.empty()) return line(std::string("b.") + cond_name(i));
        return line(std::string("b.") + cond_name(i) + " " + label);
    case Op::BCLR:
        if (i.bo == 20 && i.bi == 0) return line("ret");
        return line("// TODO bclr(bclrl)");
    case Op::BCTR:
        return line("br x20 // ctr -> x20");

    case Op::LWZ:
        if (i.ra == 0) return line("ldr " + wr(i.rt) + ", [xzr, #" + std::to_string(i.d) + "]");
        return line("ldr " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STW: return line("str " + wr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LWZU: return line("ldr " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::STWU: return line("str " + wr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LBZ: return line("ldrb " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LBZU: return line("ldrb " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::STB: return line("strb " + wr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STBU: return line("strb " + wr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LHZ: return line("ldrh " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LHZU: return line("ldrh " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LHA: return line("ldrsh " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LHAU: return line("ldrsh " + wr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::STH: return line("strh " + wr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STHU: return line("strh " + wr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LWBRX:
        return line("ldr " + wr(i.rt) + ", [" + xr(i.ra) + ", " + xr(i.rb) + ", lsl #2] // TODO: byte reverse");
    case Op::LHBRX:
        return line("ldrh " + wr(i.rt) + ", [" + xr(i.ra) + ", " + xr(i.rb) + ", lsl #1] // TODO: byte reverse");
    case Op::STWBRX:
        return line("str " + wr(i.rs) + ", [" + xr(i.ra) + ", " + xr(i.rb) + ", lsl #2] // TODO: byte reverse");
    case Op::LD: return line("ldr " + xr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LDU: return line("ldr " + xr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LWA: return line("ldrsw " + xr(i.rt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STD: return line("str " + xr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STDU: return line("str " + xr(i.rs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LMW: return line("// TODO lmw");
    case Op::STMW: return line("// TODO stmw");

    case Op::LFS: return line("ldr " + sr(i.frt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LFSU: return line("ldr " + sr(i.frt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::LFD: return line("ldr " + dr(i.frt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::LFDU: return line("ldr " + dr(i.frt) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::STFS: return line("str " + sr(i.frs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STFSU: return line("str " + sr(i.frs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");
    case Op::STFD: return line("str " + dr(i.frs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]");
    case Op::STFDU: return line("str " + dr(i.frs) + ", [" + xr(i.ra) + ", #" + std::to_string(i.d) + "]!");

    case Op::FADD: return line("fadd " + dr(i.frd) + ", " + dr(i.fra) + ", " + dr(i.frb));
    case Op::FSUB: return line("fsub " + dr(i.frd) + ", " + dr(i.fra) + ", " + dr(i.frb));
    case Op::FMUL: return line("fmul " + dr(i.frd) + ", " + dr(i.fra) + ", " + dr(i.frb));
    case Op::FDIV: return line("fdiv " + dr(i.frd) + ", " + dr(i.fra) + ", " + dr(i.frb));
    case Op::FSQRT: return line("fsqrt " + dr(i.frd) + ", " + dr(i.frb));
    case Op::FABS: return line("fabs " + dr(i.frd) + ", " + dr(i.frb));
    case Op::FNEG: return line("fneg " + dr(i.frd) + ", " + dr(i.frb));
    case Op::FMR: return line("fmov " + dr(i.frd) + ", " + dr(i.frb));
    case Op::FRSP: return line("// TODO frsp (single-round)");
    case Op::FRES: return line("// TODO fres");
    case Op::FCMPU:
        return line("fcmp " + dr(i.fra) + ", " + dr(i.frb) + " // TODO: CR" + std::to_string(i.crfd) + " update");
    case Op::FMADD: return line("fmadd " + dr(i.frd) + ", " + dr(i.fra) + ", " + dr(i.frb) + ", " + dr(i.frd));
    case Op::FMSUB: return line("// TODO fmsub");
    case Op::FNMADD: return line("// TODO fnmadd");
    case Op::FNMSUB: return line("// TODO fnmsub");
    case Op::MFFS: return line("// TODO mffs (fpscr)");
    case Op::MTFSF: return line("// TODO mtfsf (fpscr)");
    case Op::PS_ADD:
    case Op::PS_SUB:
    case Op::PS_MUL:
    case Op::PS_DIV:
    case Op::PS_ABS:
    case Op::PS_NEG:
    case Op::PS_MR:
    case Op::PS_MADD:
    case Op::PSQ_L:
    case Op::PSQ_ST:
        return line("// TODO paired-single: " + src);

    case Op::MFLR: return line("mov " + xr(i.rd) + ", x30");
    case Op::MTLR: return line("mov x30, " + xr(i.rs));
    case Op::MFCTR: return line("mov " + xr(i.rd) + ", x20");
    case Op::MTCTR: return line("mov x20, " + xr(i.rs));
    case Op::MFSPR:
    case Op::MTSPR:
        return line("// TODO " + std::string(i.op == Op::MFSPR ? "mfspr" : "mtspr") + "(spr=" + std::to_string(i.spr) + ")");

    case Op::CMPW: return line("cmp " + wr(i.ra) + ", " + wr(i.rb));
    case Op::CMPLW: return line("cmp " + wr(i.ra) + ", " + wr(i.rb) + " // unsigned");
    case Op::CMPI: return line("cmp " + wr(i.ra) + ", #" + std::to_string(i.d));
    case Op::CMPLI: return line("cmp " + wr(i.ra) + ", #" + std::to_string(i.uimm) + " // unsigned");

    case Op::MFCR: return line("// TODO mfcr");
    case Op::MTCRF: return line("// TODO mtcrf");
    case Op::MCRF: return line("// TODO mcrf");

    case Op::SYNC: return line("dmb ish");
    case Op::ISYNC: return line("isb");
    case Op::TRAP: return line("brk #0xff");
    case Op::SC: return line("svc #0 // XAM/kernel call stub");

    default:
        return "  .byte 0x00                     // UNSUPPORTED: " + src + " (raw " +
               [&]() {
                   char r[16];
                   std::snprintf(r, sizeof(r), "%08x", i.raw);
                   return std::string(r);
               }() + ")";
    }
}

} // namespace rsr