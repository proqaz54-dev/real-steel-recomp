#!/bin/sh
# Usage: build_object.sh <image.ir> <out.o>
set -e
IR="$1"
OBJ="$2"
DIR="$(cd "$(dirname "$0")" && pwd)"
python3 "$DIR/extract_asm.py" "$IR" "${OBJ%.o}.s"
clang -c -target aarch64-linux-android24 -x assembler "${OBJ%.o}.s" -o "$OBJ"
clang -shared -fPIC -target aarch64-linux-android24 "$DIR/runtime.c" "$OBJ" -o "${OBJ%.o}.so"
echo "OK obj=$(stat -c%s "$OBJ") so=$(stat -c%s "${OBJ%.o}.so")"
echo "undefined: $(nm -u "$OBJ" | wc -l)"