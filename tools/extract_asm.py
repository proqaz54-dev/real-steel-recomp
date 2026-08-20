#!/usr/bin/env python3
import re
import sys

KEEP = set(
    "mov movz movk add sub mul madd ldr ldrb ldrh str strb strh eor and orr ror "
    "lsl lsr asr sxtb sxth sxtw uxtb uxth clz neg cmp b. b bl cbz cbnz tbnz tst "
    "mvn bic ccmp cset ret br adr nop svc brk isb dmb msr mrs hlt csdb".split()
)

def main():
    src = sys.argv[1]
    dst = sys.argv[2]
    lines = []
    for ln in open(src).read().splitlines():
        t = ln.strip()
        if not t or "=" in t:
            continue
        if t.startswith("IR fn") or t.startswith("bb ") or t.startswith("regalloc"):
            continue
        if "-> 0x" in t and "  " not in t.split("->")[0]:
            continue
        m = re.match(r"^(\S+)", t)
        if (m and m.group(1) in KEEP) or re.match(r"^(L_[0-9a-f]+|\.Lbb)", t):
            lines.append(ln.rstrip())
        elif t.startswith((".globl", "entry:", "// unsupported", "// call ->", "// unresolved")):
            lines.append(ln.rstrip())
    open(dst, "w").write("\n".join(lines) + "\n")
    print(f"asm lines: {len(lines)}")

if __name__ == "__main__":
    main()