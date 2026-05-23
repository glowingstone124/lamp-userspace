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

LAMP_LD="${LAMP_LD:-${LAMP_CLANG%/clang}/ld.lld}" bash "${SCRIPT_DIR}/build.sh"

KCONFIG_ALLCONFIG="${MIN_CONFIG}" make -C "${BUSYBOX_DIR}" allnoconfig
while IFS= read -r line; do
  [[ "${line}" == CONFIG_* ]] || continue
  name="${line%%=*}"
  value="${line#*=}"
  LINE="${name}=${value}" NAME="${name}" perl -0pi -e '
    BEGIN { $name = $ENV{"NAME"}; $line = $ENV{"LINE"}; }
    $matched = s/(^|\n)(?:# \Q$name\E is not set|\Q$name\E=.*)(?=\n|$)/$1$line/m;
    END { if (!$matched) { print "\n$line\n"; } }
  ' "${BUSYBOX_DIR}/.config"
done < "${MIN_CONFIG}"
make -C "${BUSYBOX_DIR}" silentoldconfig
make -C "${BUSYBOX_DIR}" \
  CC="${SCRIPT_DIR}/lamp-cc.sh" \
  LD="${LAMP_LD:-${LAMP_CLANG%/clang}/ld.lld}" \
  STRIP="${LAMP_STRIP:-${LAMP_CLANG%/clang}/llvm-strip}" \
  busybox

echo "built: ${BUSYBOX_DIR}/busybox"
