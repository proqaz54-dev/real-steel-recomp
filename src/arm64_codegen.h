#pragma once
#include <set>

#include "ir.h"
#include "regalloc.h"

#include <string>

namespace rsr {

// Emits ARM64 assembly text for one IR function.
// in_range: predicate for addresses that get local labels (b L_<addr>);
// other targets get a comment. spills use [x19,#-8k] + w16/x16 scratch.
std::string codegen_arm64(const IRFunc& f, const RegAlloc& ra,
                          bool (*in_range)(uint64_t, void*), void* ctx,
                          uint64_t entry_addr = 0,
                          const std::set<uint64_t>* fn_addrs = nullptr);

} // namespace rsr