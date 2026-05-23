#!/usr/bin/env bash
set -euo pipefail

: "${LAMP_CLANG:?Error: LAMP_CLANG is not defined}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUSYBOX_DIR="${REPO_ROOT}/busybox-1.37.0"
MIN_CONFIG="${SCRIPT_DIR}/busybox_lamp_min.config"

if [[ ! -d "${BUSYBOX_DIR}" ]]; then
  echo "error: BusyBox source directory not found: ${BUSYBOX_DIR}" >&2
  exit 1
fi

mkdir -p "${REPO_ROOT}/build-user"
LAMP_LD="${LAMP_LD:-${LAMP_CLANG%/clang}/ld.lld}"

"${LAMP_CLANG}" --target=lamp-unknown-unknown \
  -ffreestanding -fno-builtin -fno-stack-protector -O0 \
  -I"${SCRIPT_DIR}/include" -c "${SCRIPT_DIR}/crt/start.c" \
  -o "${REPO_ROOT}/build-user/start.o"

"${LAMP_CLANG}" --target=lamp-unknown-unknown \
  -ffreestanding -fno-builtin -fno-stack-protector -O0 \
  -I"${SCRIPT_DIR}/include" -c "${SCRIPT_DIR}/lib/libsys.c" \
  -o "${REPO_ROOT}/build-user/libsys.o"

"${LAMP_CLANG}" --target=lamp-unknown-unknown \
  -ffreestanding -fno-builtin -fno-stack-protector -O0 \
  -I"${SCRIPT_DIR}/sysroot/include" -I"${SCRIPT_DIR}/include" \
  -c "${SCRIPT_DIR}/lib/libc_compat.c" \
  -o "${REPO_ROOT}/build-user/libc_compat.o"

"${LAMP_CLANG}" --target=lamp-unknown-unknown \
  -ffreestanding -fno-builtin -fno-stack-protector -O0 \
  -I"${SCRIPT_DIR}/include" -c "${SCRIPT_DIR}/lib/compiler_rt_divmod.c" \
  -o "${REPO_ROOT}/build-user/compiler_rt_divmod.o"

KCONFIG_ALLCONFIG="${MIN_CONFIG}" make -C "${BUSYBOX_DIR}" allnoconfig
while IFS= read -r line; do
  if [[ "${line}" == CONFIG_*=* ]]; then
    name="${line%%=*}"
  elif [[ "${line}" =~ ^#\ (CONFIG_[A-Za-z0-9_]+)\ is\ not\ set$ ]]; then
    name="${BASH_REMATCH[1]}"
  else
    continue
  fi
  LINE="${line}" NAME="${name}" perl -0pi -e '
    BEGIN { $name = $ENV{"NAME"}; $line = $ENV{"LINE"}; }
    $matched = s/(^|\n)(?:# \Q$name\E is not set|\Q$name\E=.*)(?=\n|$)/$1$line/m;
    END { if (!$matched) { print "\n$line\n"; } }
  ' "${BUSYBOX_DIR}/.config"
done < "${MIN_CONFIG}"
make -C "${BUSYBOX_DIR}" silentoldconfig
make -C "${BUSYBOX_DIR}" \
  CC="${SCRIPT_DIR}/lamp-cc.sh" \
  LD="${LAMP_LD}" \
  STRIP="${LAMP_STRIP:-${LAMP_CLANG%/clang}/llvm-strip}" \
  busybox

echo "built: ${BUSYBOX_DIR}/busybox"
