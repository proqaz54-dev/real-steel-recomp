#pragma once

#include "ppc32_decode.h"

#include <string>

namespace rsr {

// Emits ARM64 (AArch64) assembly for one decoded PPC32 instruction.
// `label` is a target label for branches (already computed by the driver).
// String is a full line including a comment with the original instruction.
std::string emit_arm64(const Insn& i, const std::string& label);

} // namespace rsr