#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(dirname "$here")"
build="${KR32OS_BUILD_DIR:-$here/build/linux}"
opt="${KR32OS_OPT:--O1}"
bios_base=0xFFFF0000
kernel_base=0x00020000
user_base=0x00400000

if [[ -z "${KR32:-}" ]]; then
    for candidate in \
        "$root/build/linux-gcc/kr32" \
        "$root/build/linux-clang/kr32" \
        "$here/build/kr32"; do
        if [[ -x "$candidate" ]]; then
            KR32="$candidate"
            break
        fi
    done
fi
if [[ -z "${KR32:-}" ]] && command -v kr32 >/dev/null 2>&1; then
    KR32="$(command -v kr32)"
fi
if [[ -z "${KR32:-}" || ! -x "$KR32" ]]; then
    echo "kr32 was not found; set KR32 or pass a host build path" >&2
    exit 2
fi

mkdir -p "$build"
"$KR32" as "$here/bios/entry.S" --object -o "$build/bios.o"
"$KR32" ld "$build/bios.o" --flat --base "$bios_base" --sym "$build/bios.sym" -o "$build/bios.bin"
"$KR32" ld "$build/bios.o" --base "$bios_base" -o "$build/bios.k32"
"$KR32" cc "$here/kernel/main.c" --source-id 1 $opt -S -o "$build/kernel.s"
"$KR32" as "$build/kernel.s" --object -o "$build/kernel.o"
"$KR32" cc "$here/kernel/console.c" --source-id 2 $opt -S -o "$build/console.s"
"$KR32" as "$build/console.s" --object -o "$build/console.o"
"$KR32" cc "$here/kernel/shell.c" --source-id 3 $opt -S -o "$build/shell.s"
"$KR32" as "$build/shell.s" --object -o "$build/shell.o"
"$KR32" cc "$here/kernel/timer.c" --source-id 4 $opt -S -o "$build/timer.s"
"$KR32" as "$build/timer.s" --object -o "$build/timer.o"
"$KR32" cc "$here/kernel/traps.c" --source-id 5 $opt -S -o "$build/traps.s"
"$KR32" as "$build/traps.s" --object -o "$build/traps.o"
"$KR32" as "$here/kernel/traps-asm.S" --object -o "$build/trap_stubs.o"
"$KR32" cc "$here/kernel/user.c" --source-id 6 $opt -S -o "$build/user.s"
"$KR32" as "$build/user.s" --object -o "$build/user.o"
"$KR32" cc "$here/kernel/memory.c" --source-id 9 $opt -S -o "$build/memory.s"
"$KR32" as "$build/memory.s" --object -o "$build/memory.o"
"$KR32" cc "$here/kernel/process.c" --source-id 8 $opt -S -o "$build/process.s"
"$KR32" as "$build/process.s" --object -o "$build/process.o"
"$KR32" cc "$here/kernel/syscall.c" --source-id 7 $opt -S -o "$build/syscall.s"
"$KR32" as "$build/syscall.s" --object -o "$build/syscall.o"
"$KR32" as "$here/kernel/user-asm.S" --object -o "$build/user_bridge.o"
"$KR32" as "$here/boot/entry.S" --object -o "$build/entry.o"

"$KR32" cc "$here/user/init.c" $opt -S -o "$build/user_init.s"
"$KR32" as "$build/user_init.s" --object -o "$build/user_init.o"
"$KR32" as "$here/libc/crt0.S" --object -o "$build/crt0.o"
"$KR32" as "$here/libc/syscall.S" --object -o "$build/user_syscall.o"
"$KR32" cc "$here/user/fault.c" $opt -S -o "$build/user_fault.s"
"$KR32" as "$build/user_fault.s" --object -o "$build/user_fault.o"
"$KR32" cc "$here/user/wx.c" $opt -S -o "$build/user_wx.s"
"$KR32" as "$build/user_wx.s" --object -o "$build/user_wx.o"
"$KR32" ld "$build/crt0.o" "$build/user_init.o" "$build/user_syscall.o" \
    --flat --base "$user_base" -o "$build/init.bin"
"$KR32" ld "$build/crt0.o" "$build/user_fault.o" "$build/user_syscall.o" \
    --flat --base "$user_base" -o "$build/fault.bin"
"$KR32" ld "$build/crt0.o" "$build/user_wx.o" "$build/user_syscall.o" \
    --flat --base "$user_base" -o "$build/wx.bin"
python3 "$here/tools/mkinitrd.py" "$build/init.bin" "$build/fault.bin" "$build/wx.bin" "$build/initrd.bin" "$build/initrd.s"
"$KR32" as "$build/initrd.s" --object -o "$build/initrd.o"

"$KR32" ld "$build/entry.o" "$build/kernel.o" "$build/console.o" "$build/shell.o" \
    "$build/timer.o" "$build/traps.o" "$build/trap_stubs.o" \
    "$build/user.o" "$build/syscall.o" "$build/process.o" "$build/memory.o" "$build/user_bridge.o" "$build/initrd.o" \
    --flat --base "$kernel_base" --map "$build/kernel.map" --sym "$build/kernel.sym" -o "$build/kernel.bin"
"$KR32" ld "$build/entry.o" "$build/kernel.o" "$build/console.o" "$build/shell.o" \
    "$build/timer.o" "$build/traps.o" "$build/trap_stubs.o" \
    "$build/user.o" "$build/syscall.o" "$build/process.o" "$build/memory.o" "$build/user_bridge.o" "$build/initrd.o" \
    --base "$kernel_base" -o "$build/kernel.k32"
python3 "$here/tools/mkmeta.py" "$build/kernel.bin" "$build/kernel.map" "$build/kernel.meta"

echo "KR32OS M4 built:"
echo "  $build/bios.bin"
echo "  $build/kernel.bin"
