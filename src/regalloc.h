#pragma once

#include "ir.h"

#include <vector>

namespace rsr {

// Guest r31 lives in host x19, CTR in x20. Allocatable: x0..x18, x21..x29.
constexpr int kGuestSP = 19;
constexpr int kGuestCTR = 20;
constexpr int kGuestLR = 30;

struct RegAlloc {
    std::vector<int> phys; // vreg -> host reg, -1 if spilled
    std::vector<int> slot; // vreg -> stack slot (8B, off [x19,#-8k])
};

// Linear-scan over the function's live intervals. Spilled vregs get
// slots below the guest frame (headroom slots max).
RegAlloc linear_scan(const IRFunc& f, int headroom);

// "vN -> xN" or "vN -> spill -8k" (for reporting/debug).
std::string regalloc_to_string(const IRFunc& f, const RegAlloc& ra);

} // namespace rsr