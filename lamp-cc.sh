#!/usr/bin/env bash
set -euo pipefail

: "${LAMP_CLANG:?Error: LAMP_CLANG is not defined}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LAMP_LD="${LAMP_LD:-${LAMP_CLANG%/clang}/ld.lld}"

# Optimization levels pass through to the compiler; callers that need the
# conservative baseline can still request -O0 explicitly.
is_compile=0
for arg in "$@"; do
  case "${arg}" in
    -c|-E|-S)
      is_compile=1
      ;;
  esac
done

if [[ "${is_compile}" -eq 1 ]]; then
  exec "${LAMP_CLANG}" --target=lamp-unknown-unknown -fno-omit-frame-pointer "$@"
fi

out=""
ld_args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -o)
      out="$2"
      shift 2
      ;;
    -Wl,*)
      IFS=',' read -r -a parts <<< "${1#-Wl,}"
      for part in "${parts[@]}"; do
        ld_args+=("${part}")
      done
      shift
      ;;
    *.o|*.a)
      ld_args+=("$1")
      shift
      ;;
    -l*)
      shift
      ;;
    -L*)
      shift
      ;;
    *)
      shift
      ;;
  esac
done

if [[ -z "${out}" ]]; then
  echo "lamp-cc: missing -o for link" >&2
  exit 1
fi

exec "${LAMP_LD}" \
  -T "${REPO_ROOT}/user/linker.ld" \
  -e _start \
  -o "${out}" \
  "${REPO_ROOT}/build-user/start.o" \
  "${REPO_ROOT}/build-user/libsys.o" \
  "${REPO_ROOT}/build-user/libc_string.o" \
  "${REPO_ROOT}/build-user/libc_stdio.o" \
  "${REPO_ROOT}/build-user/libc_stdlib.o" \
  "${REPO_ROOT}/build-user/libc_syscalls.o" \
  "${REPO_ROOT}/build-user/libc_time.o" \
  "${REPO_ROOT}/build-user/libc_signal.o" \
  "${REPO_ROOT}/build-user/libc_regex.o" \
  "${REPO_ROOT}/build-user/libc_misc.o" \
  "${REPO_ROOT}/build-user/compiler_rt_divmod.o" \
  "${REPO_ROOT}/build-user/compiler_rt_softfloat.o" \
  "${ld_args[@]}"
