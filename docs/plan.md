# Roadmap: Real Steel (Xbox 360) static recompiler

Target: `default.xex` from the Xbox 360 disc (`realsteel` title id varies by region).
Guest: PowerPC32 (Xenon). Host: ARM64 (Android/Termux, later Vulkan).

## Phase 1 — Tooling foundation (current)

- [x] XEX2 loader: magic, headers, section table, entry, base address (`src/xex.*`)
- [x] PPC32 decoder: integer ALU, load/store (D/DS/X), branches, mflr/mtlr/ctr,
      cmp, nop, sync/isync — `src/ppc32_decode.*`
- [x] ARM64 text emitter with demo register map — `src/arm64_emit.*`
- [x] CLI that dumps a disassembled/emitted listing — `src/main.cpp`
- [ ] Verify section layout against a real dump (alignments/relocations)
- [ ] XEX1 parsing (rarely needed; most retail XEX2)

## Phase 2 — Full coverage & analysis

- [ ] Decode every PPC32 opcode incl. FPU (loads/stores, fpscr), paired singles
- [ ] Lvar/string reach, function boundary discovery from `bl` targets
- [ ] Call graph + import table (XAM kernel imports, thunk stubs)
- [ ] Data refs analysis (address constants -> pointers into image/data sections)

## Phase 3 — Static translation

- [ ] IR or direct PPC32->ARM64 lowering, full instruction selection
- [ ] Register allocation + Xenon ABI mapping (r1 SP, r3..r10 args, r11 volatile)
- [ ] Prologue/epilogue helpers; exception/RC semantics (CR flags)
- [ ] Emit relocatable ARM64 + C shims per guest function

## Phase 4 — Runtime

- [ ] Guest memory: direct-mapped 32-bit with bounds checks (Android 64-bit ptr)
- [ ] XAM/kernel syscall stubs (memory, threads, files, crypto)
- [ ] Indirect call handling (function pointer stub table / JIT glue)
- [ ] Threading model (guest threads on host threads, TLS)

## Phase 5 — Graphics & media

- [ ] Xenos (D3D9-ish) -> Vulkan/GLES wrapper
- [ ] Shader translator / VLT -> SPIR-V/slang
- [ ] Audio (XMA) + input
- [ ] Asset loading from game disc/images + boot flow

## Phase 6 — Validation & perf

- [ ] Run Real Steel in Xenia for reference output; diff behavior in headless tests
- [ ] Boot to title, then gameplay; FPS goals on mid-range ARM SoC
- [ ] LTO, hot-path syscall ring, cache-friendly code layout, SIMD where possible

## Reference material

- Xenia (github.com/xenia-project/xenia) — XEX, Xenos, kernel behavior
- IBM PowerPC Processor v2.05 ISA — Xenon opcode reference
- Xbox 360 System Software / Xam docs (community wiki)