#include "regalloc.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

namespace rsr {

namespace {

struct Interval {
    int vreg = -1;
    int start = 0, end = 0;
    int freq = 0;
};

void collect(const IRBlock& b, std::set<int>& def, std::set<int>& use) {
    for (const auto& ir : b.insns) {
        if (ir.dst >= 0 && ir.op != IROp::UNSUP) def.insert(ir.dst);
        if (ir.a >= 0 && ir.op != IROp::UNSUP) use.insert(ir.a);
        if (ir.b >= 0 && ir.op != IROp::UNSUP) use.insert(ir.b);
    }
}

} // namespace

RegAlloc linear_scan(const IRFunc& f, int headroom) {
    RegAlloc ra;
    std::set<int> all;
    for (const auto& b : f.blocks)
        for (const auto& ir : b.insns) {
            if (ir.dst >= 0) all.insert(ir.dst);
            if (ir.a >= 0) all.insert(ir.a);
            if (ir.b >= 0) all.insert(ir.b);
        }
    int max_v = all.empty() ? 0 : *all.rbegin() + 1;
    ra.phys.assign(max_v, -1);
    ra.slot.assign(max_v, -1);
    if (f.blocks.empty()) return ra;

    std::vector<int> bstart(f.blocks.size());
    int ic = 0;
    for (size_t i = 0; i < f.blocks.size(); i++) {
        bstart[i] = ic;
        ic += static_cast<int>(f.blocks[i].insns.size()) + 1;
    }

    std::map<uint64_t, int> bindex;
    for (size_t i = 0; i < f.blocks.size(); i++) bindex[f.blocks[i].start] = static_cast<int>(i);

    struct L { std::set<int> def, use, in, out; };
    std::vector<L> l(f.blocks.size());
    for (size_t i = 0; i < f.blocks.size(); i++) collect(f.blocks[i], l[i].def, l[i].use);

    bool changed = true;
    while (changed) {
        changed = false;
        for (int bi = static_cast<int>(f.blocks.size()) - 1; bi >= 0; bi--) {
            std::set<int> succ;
            if (bi + 1 < static_cast<int>(f.blocks.size())) succ.insert(bi + 1);
            for (const auto& ir : f.blocks[bi].insns)
                if ((ir.op == IROp::BR || ir.op == IROp::BR_COND) && bindex.count(ir.label))
                    succ.insert(bindex[ir.label]);
            std::set<int> nout;
            for (int s : succ)
                for (int v : l[s].in) nout.insert(v);
            std::set<int> nin = l[bi].use;
            for (int v : nout)
                if (!l[bi].def.count(v)) nin.insert(v);
            if (nin != l[bi].in) { l[bi].in = nin; changed = true; }
        }
    }

    std::vector<Interval> ivs;
    for (int v : all) {
        int first = -1, last = -1, freq = 0;
        for (size_t bi = 0; bi < f.blocks.size(); bi++) {
            const auto& b = f.blocks[bi];
            bool used = false;
            for (const auto& ir : b.insns) used |= (ir.dst == v || ir.a == v || ir.b == v);
            if (used) {
                if (first < 0) first = bstart[bi];
                last = bstart[bi] + static_cast<int>(b.insns.size()) - 1;
                freq++;
            }
        }
        if (first < 0) continue;
        ivs.push_back(Interval{v, first, last, freq});
    }
    std::sort(ivs.begin(), ivs.end(), [](const Interval& a, const Interval& b) {
        return a.start < b.start;
    });

    std::vector<int> pool;
    for (int r = 0; r <= 29; r++)
        if (r != 16 && r != 17 && r != kGuestSP && r != kGuestCTR && r != kGuestLR)
            pool.push_back(r);  // x16/x17 reserved as scratch

    int next_slot = 1;
    std::vector<Interval> active;
    for (const auto& iv : ivs) {
        for (auto it = active.begin(); it != active.end();)
            if (it->end < iv.start) it = active.erase(it); else ++it;
        std::set<int> used_p;
        for (const auto& a : active) used_p.insert(ra.phys[a.vreg]);
        int phys = -1;
        for (int r : pool)
            if (!used_p.count(r)) { phys = r; break; }
        if (iv.vreg >= 64) continue; // CTR/LR live in host x20/x30, not allocatable
        if (phys >= 0) {
            ra.phys[iv.vreg] = phys;
            active.push_back(iv);
            continue;
        }
        auto victim = std::max_element(active.begin(), active.end(),
            [](const Interval& a, const Interval& b) { return a.end < b.end; });
        if (victim != active.end() && victim->end > iv.end) {
            ra.phys[victim->vreg] = -1;
            ra.slot[victim->vreg] = next_slot++;
            phys = -1;
            for (int r : pool) {
                bool busy = false;
                for (const auto& a : active)
                    if (a.vreg != victim->vreg && ra.phys[a.vreg] == r) { busy = true; break; }
                if (!busy) { phys = r; break; }
            }
            ra.phys[iv.vreg] = phys;
            *victim = iv;
        } else if (next_slot <= headroom) {
            ra.slot[iv.vreg] = next_slot++;
        }
    }
    return ra;
}

std::string regalloc_to_string(const IRFunc& f, const RegAlloc& ra) { (void)f;
    std::string out;
    char buf[96];
    int nphys = 0, nspill = 0;
    for (size_t v = 0; v < ra.phys.size(); v++) {
        if (ra.phys[v] >= 0) nphys++;
        else if (ra.slot[v] >= 0) nspill++;
    }
    std::snprintf(buf, sizeof(buf), "  regalloc: %d phys, %d spilled (of %zu live vregs)\n",
                  nphys, nspill, ra.phys.size());
    out += buf;
    for (size_t v = 0; v < ra.phys.size(); v++) {
        if (ra.phys[v] >= 0)
            std::snprintf(buf, sizeof(buf), "    v%zu -> x%d\n", v, ra.phys[v]);
        else if (ra.slot[v] >= 0)
            std::snprintf(buf, sizeof(buf), "    v%zu -> spill -%d\n", v, ra.slot[v] * 8);
    }
    out += buf;
    return out;
}

} // namespace rsr