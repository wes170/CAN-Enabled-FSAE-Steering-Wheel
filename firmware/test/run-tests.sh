#!/bin/sh
# Host-side firmware tests. No hardware, no cross-compiler needed.
#
# These cover the logic that can be proven at a desk: clock-tree arithmetic
# (enforced by static assertions, so a violation fails the BUILD rather than
# the run), CAN bit timing, the boot-option-byte predicate, and the Haltech
# protocol maths. Register writes are compiled out via FIRMWARE_HOST_BUILD.
#
# Both board personalities are built, because a change that only compiles for
# one of them is a change that breaks the other board silently.
set -e
cd "$(dirname "$0")/.."
BASE="-std=c11 -Wall -Wextra -Werror -Iinclude -include test/host_stubs.h"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

for BOARD in BOARD_WHEEL BOARD_DASH; do
    echo "== compile check: -D$BOARD =="
    for SRC in src/*.c; do
        gcc $BASE -D$BOARD -c "$SRC" -o "$OUT/$(basename $SRC .c)_$BOARD.o"
    done
    echo "   ok"
done

echo
echo "== system_init tests (wheel personality) =="
gcc $BASE -DBOARD_WHEEL test/test_system_init.c src/system_init.c -o "$OUT/t_sys"
"$OUT/t_sys"

echo
echo "== haltech_can tests =="
gcc $BASE -DBOARD_WHEEL test/test_haltech_can.c src/haltech_can.c -o "$OUT/t_can"
"$OUT/t_can"

echo
echo "== encoder tests =="
gcc $BASE -DBOARD_WHEEL test/test_encoder.c src/encoder.c -o "$OUT/t_enc"
"$OUT/t_enc"

echo
echo "All host tests passed."
