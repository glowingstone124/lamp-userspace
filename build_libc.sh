#!/usr/bin/env bash
set -e
: "${LAMP_CLANG:?Error: LAMP_CLANG is not defined}"

mkdir -p build-user

CFLAGS="--target=lamp-unknown-unknown -ffreestanding -fno-builtin -fno-stack-protector -O0"
INCLUDES="-Iuser/sysroot/include -Iuser/include -Iuser/lib"

modules="libc_string libc_stdio libc_stdlib libc_syscalls libc_time libc_signal libc_misc"
for m in $modules; do
    "$LAMP_CLANG" $CFLAGS $INCLUDES -c "user/lib/${m}.c" -o "build-user/${m}.o"
    echo "built: build-user/${m}.o"
done
