#!/usr/bin/env bash
set -e
: "${LAMP_CLANG:?Error: LAMP_CLANG is not defined}"

mkdir -p build-user

LAMP_OPT_LEVEL="${LAMP_OPT_LEVEL:--O0}"
case "${LAMP_OPT_LEVEL}" in
    -O|-O0|-O1|-O2|-O3|-Os|-Oz|-Ofast) ;;
    *)
        echo "error: LAMP_OPT_LEVEL must be a supported Clang -O flag" >&2
        exit 1
        ;;
esac

CFLAGS="--target=lamp-unknown-unknown -ffreestanding -fno-builtin -fno-stack-protector ${LAMP_OPT_LEVEL}"
INCLUDES="-Iuser/sysroot/include -Iuser/include -Iuser/lib"

modules="libc_string libc_stdio libc_stdlib libc_syscalls libc_time libc_signal libc_regex libc_misc"
for m in $modules; do
    "$LAMP_CLANG" $CFLAGS $INCLUDES -c "user/lib/${m}.c" -o "build-user/${m}.o"
    echo "built: build-user/${m}.o"
done
