#include <set>
#include "ppc32_decode.h"
#include "arm64_emit.h"
#include "arm64_codegen.h"
#include "ir.h"
#include "regalloc.h"
#include "xex.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string input;
    std::string output = "out.s";
    // Flat pre-decrypted image mode: --flat file.bin --base 0x.. --entry 0x..
    std::string flat;
    uint64_t flat_base = 0;
    uint64_t flat_entry = 0;
    std::vector<std::pair<uint64_t, uint64_t>> code_ranges;
    long limit_fns = -1;
};

bool parse_hex(const std::string& s, uint64_t& v) {
    if (s.empty()) return false;
    try {
        v = std::stoull(s, nullptr, 16);
    } catch (...) {
        return false;
    }
    return true;
}

bool parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            o.output = argv[++i];
        } else if (a == "--flat" && i + 1 < argc) {
            o.flat = argv[++i];
        } else if (a == "--base" && i + 1 < argc) {
            if (!parse_hex(argv[++i], o.flat_base)) return false;
        } else if (a == "--entry" && i + 1 < argc) {
            if (!parse_hex(argv[++i], o.flat_entry)) return false;
        } else if (a == "--limit-fns" && i + 1 < argc) {
            o.limit_fns = std::atol(argv[++i]);
        } else if (a == "--text" && i + 1 < argc) {
            uint64_t b, e;
            std::string s = argv[++i];
            auto colon = s.find(':');
            if (colon == std::string::npos) return false;
            if (!parse_hex(s.substr(0, colon), b)) return false;
            if (!parse_hex(s.substr(colon + 1), e)) return false;
            if (e <= b) return false;
            o.code_ranges.emplace_back(b, e);
        } else if (a == "-h" || a == "--help") {
            return false;
        } else {
            o.input = a;
        }
    }
    return !o.input.empty() || !o.flat.empty();
}

void print_usage() {
    std::printf("usage: real-steel-recomp default.xex [-o out.s]\n");
    std::printf("       real-steel-recomp --flat image.bin --base 0x82000000 \\\n");
    std::printf("            --entry 0x82088ab8 --text 0x77bc0:0x41ec7c [-o out.s]\n");
    std::printf("  Static recompiler foundation: XEX2 (PPC32) -> ARM64 assembly.\n");
}

uint64_t fetch(const rsr::XexImage& img, const rsr::XexSection& sec, uint64_t addr) {
    uint64_t size = uint64_t(sec.page_size) * sec.page_count;
    if (addr < sec.vaddr || addr >= sec.vaddr + size) return 0;
    uint64_t file = sec.data_offset + (addr - sec.vaddr);
    if (file + 4 > img.data.size()) return 0;
    return rsr::be32(img.data.data() + file);
}

} // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    Options o;
    if (!parse_args(argc, argv, o)) {
        print_usage();
        return 1;
    }

    rsr::XexImage img;
    if (!o.flat.empty()) {
        img = rsr::load_flat(o.flat, o.flat_base, o.flat_entry, o.code_ranges);
    } else {
        img = rsr::load_xex(o.input);
    }
    if (!img.ok) {
        std::fprintf(stderr, "error: %s\n", img.error.c_str());
        return 1;
    }

    const rsr::XexSection* entry_sec = img.section_at(img.entry);
    if (!entry_sec) {
        std::fprintf(stderr, "error: entry point 0x%llx is not inside any section\n",
                     (unsigned long long)img.entry);
        return 1;
    }

    std::printf("xex: base=0x%llx entry=0x%llx image=%llu bytes file=%zu bytes\n",
                (unsigned long long)img.base,
                (unsigned long long)img.entry,
                (unsigned long long)img.image_size,
                img.data.size());
    for (size_t i = 0; i < img.sections.size(); i++)
        std::printf("  section[%zu]: data@0x%llx  vaddr 0x%llx..0x%llx\n",
                    i,
                    (unsigned long long)img.sections[i].data_offset,
                    (unsigned long long)img.sections[i].vaddr,
                    (unsigned long long)(img.sections[i].vaddr +
                                         uint64_t(img.sections[i].page_size) *
                                             img.sections[i].page_count));

    // Pass 1: decode all code sections to collect branch targets + functions.
    std::map<uint64_t, bool> labels;
    std::map<uint64_t, bool> functions;
    std::set<uint64_t> xtargets;
    std::vector<rsr::Insn> insns;
    uint64_t unsupported = 0;
    for (const auto& sec : img.sections) {
        uint64_t size = uint64_t(sec.page_size) * sec.page_count;
        std::map<uint64_t, rsr::Op> ops_here;
        for (uint64_t pc = sec.vaddr; pc + 4 <= sec.vaddr + size; pc += 4) {
            uint32_t w = static_cast<uint32_t>(fetch(img, sec, pc));
            rsr::Insn in = rsr::decode(w, pc);
            insns.push_back(in);
            if (in.op == rsr::Op::UNKNOWN) unsupported++;
            if (in.op == rsr::Op::B || in.op == rsr::Op::BL) {
                uint64_t target = in.aa ? (in.d & 0xFFFFFFFF) : (pc + in.d);
                labels[target] = true;
            } else if (in.op == rsr::Op::BC) {
                uint64_t target = pc + in.d;
                labels[target] = true;
            }
            int64_t ct = rsr::call_target(in);
            if (ct >= 0 && img.section_at(static_cast<uint64_t>(ct)))
                functions[static_cast<uint64_t>(ct)] = true;
            if ((in.op == rsr::Op::B || in.op == rsr::Op::BC) &&
                img.section_at(static_cast<uint64_t>(in.pc + in.d)))
                xtargets.insert(static_cast<uint64_t>(in.pc + in.d));
        }
        (void)ops_here;
    }
    std::sort(insns.begin(), insns.end(),
              [](const rsr::Insn& a, const rsr::Insn& b) { return a.pc < b.pc; });
    std::printf("decoded %zu insns, %zu unsupported, %zu function starts\n",
                insns.size(), unsupported, functions.size());

    FILE* out = std::fopen(o.output.c_str(), "w");
    if (!out) {
        std::fprintf(stderr, "error: cannot open output %s\n", o.output.c_str());
        return 1;
    }

    std::fprintf(out, "// Generated by real-steel-recomp (XEX2/PPC32 -> ARM64)\n");
    std::fprintf(out, "// Source: %s  entry=0x%llx\n",
                 o.flat.empty() ? o.input.c_str() : o.flat.c_str(),
                 (unsigned long long)img.entry);
    std::fprintf(out, ".text\n.align 2\n.globl entry\nentry:\n");
    for (const auto& in : insns) {
        bool is_fn = functions.find(in.pc) != functions.end();
        bool is_tgt = labels.find(in.pc) != labels.end();
        if (is_fn && is_tgt)
            std::fprintf(out, "L_%llx: /* fn */  L_%llx:\n", (unsigned long long)in.pc, (unsigned long long)in.pc);
        else if (is_fn)
            std::fprintf(out, "L_%llx: /* fn */\n", (unsigned long long)in.pc);
        else if (is_tgt)
            std::fprintf(out, "L_%llx:\n", (unsigned long long)in.pc);
        std::string target;
        if (in.op == rsr::Op::B || in.op == rsr::Op::BL) {
            uint64_t t = in.aa ? (in.d & 0xFFFFFFFF) : (in.pc + in.d);
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "L_%llx", (unsigned long long)t);
            if (labels.count(t)) target = lbl;
        } else if (in.op == rsr::Op::BC) {
            uint64_t t = in.pc + in.d;
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "L_%llx", (unsigned long long)t);
            if (labels.count(t)) target = lbl;
        }
        std::fprintf(out, "  /* %08llx: */ %s\n",
                     (unsigned long long)in.pc, rsr::emit_arm64(in, target).c_str());
    }
    std::fclose(out);

    // IR + regalloc + codegen pass: functions = entry + all bl targets.
    std::vector<uint64_t> fns;
    fns.push_back(img.entry);
    for (const auto& kv : functions) fns.push_back(kv.first);
    std::sort(fns.begin(), fns.end());
    fns.erase(std::unique(fns.begin(), fns.end()), fns.end());
    std::vector<uint64_t> starts;
    for (uint64_t a : fns)
        if (img.section_at(a)) starts.push_back(a);
    std::set<uint64_t> starts_set(starts.begin(), starts.end());

    auto in_range_check = [](uint64_t addr, void* ctx) -> bool {
        auto* sections = static_cast<std::vector<std::pair<uint64_t, uint64_t>>*>(ctx);
        for (const auto& s : *sections)
            if (addr >= s.first && addr < s.second) return true;
        return false;
    };
    std::vector<std::pair<uint64_t, uint64_t>> sections;
    for (const auto& sec : img.sections)
        sections.emplace_back(sec.vaddr, sec.vaddr + uint64_t(sec.page_size) * sec.page_count);

    FILE* ir_out = std::fopen((o.output + ".ir").c_str(), "w");
    if (ir_out) {
        std::fprintf(ir_out, "// IR + regalloc + ARM64 codegen for %s\n",
                     o.flat.empty() ? o.input.c_str() : o.flat.c_str());
        size_t total_blocks = 0, total_vregs = 0, spilled = 0, unsup = 0;
        for (size_t i = 0; i < starts.size(); i++) {
            if (o.limit_fns >= 0 && static_cast<long>(i) >= o.limit_fns) break;

            uint64_t end = 0;
            if (i + 1 < starts.size()) {
                end = starts[i + 1];
            } else {
                for (const auto& sec : img.sections) {
                    uint64_t s_end = sec.vaddr + uint64_t(sec.page_size) * sec.page_count;
                    if (s_end > end) end = s_end;
                }
            }
            std::vector<uint64_t> callees;
            rsr::IRFunc f = rsr::build_ir(insns, starts[i], end, callees, xtargets);
            rsr::RegAlloc ra = rsr::linear_scan(f, 16);
            for (int s : ra.slot) if (s >= 0) spilled++;
            for (int p : ra.phys) if (p >= 0) total_vregs++;
            for (const auto& b : f.blocks) {
                total_blocks++;
                for (const auto& ir : b.insns) if (ir.op == rsr::IROp::UNSUP) unsup++;
            }
            std::string ir_text = rsr::ir_to_string(f);
            std::string ra_text = rsr::regalloc_to_string(f, ra);
            std::string asm_text = rsr::codegen_arm64(f, ra, in_range_check, &sections, img.entry, &starts_set);
            std::fprintf(ir_out, "%s%s%s\n", ir_text.c_str(), ra_text.c_str(), asm_text.c_str());
        }
        std::fclose(ir_out);
        std::printf("IR pass: %zu functions, %zu blocks, %zu live vregs, %zu spills, %zu unsupported ir\n",
                    starts.size(), total_blocks, total_vregs, spilled, unsup);
    }
    std::printf("wrote %s (%zu instructions); IR report -> %s.ir\n",
                o.output.c_str(), insns.size(), o.output.c_str());
    return 0;
}