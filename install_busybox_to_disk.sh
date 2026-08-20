#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DISK_IMG="${REPO_ROOT}/disk.img"
ROOTFS_IMG=""
BUSYBOX_ELF=""
DEBUGFS_BIN=""
EXT4_LBA_BASE=2048

usage() {
  cat <<'EOF'
Usage:
  bash user/install_busybox_to_disk.sh --input <busybox-elf> [--disk <path> | --rootfs <path>]

Installs:
  /bin/busybox
  /bin/sh -> /bin/busybox

Notes:
  - The BusyBox binary must already be a static ELF for the LAMP user ABI.
  - Requires debugfs from e2fsprogs. Homebrew paths are auto-detected on macOS.
EOF
}

find_debugfs() {
  if command -v debugfs >/dev/null 2>&1; then
    DEBUGFS_BIN="$(command -v debugfs)"
    return
  fi
  for candidate in \
    /opt/homebrew/opt/e2fsprogs/sbin/debugfs \
    /usr/local/opt/e2fsprogs/sbin/debugfs \
    /opt/homebrew/sbin/debugfs \
    /usr/local/sbin/debugfs; do
    if [[ -x "${candidate}" ]]; then
      DEBUGFS_BIN="${candidate}"
      return
    fi
  done
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --disk)
      DISK_IMG="$2"
      shift 2
      ;;
    --rootfs)
      ROOTFS_IMG="$2"
      shift 2
      ;;
    --input)
      BUSYBOX_ELF="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${BUSYBOX_ELF}" ]]; then
  echo "error: --input is required" >&2
  usage
  exit 1
fi
if [[ "${BUSYBOX_ELF}" != /* ]]; then
  BUSYBOX_ELF="${REPO_ROOT}/${BUSYBOX_ELF}"
fi
if [[ ! -f "${BUSYBOX_ELF}" ]]; then
  echo "error: BusyBox ELF not found: ${BUSYBOX_ELF}" >&2
  exit 1
fi

find_debugfs
if [[ -z "${DEBUGFS_BIN}" ]]; then
  echo "error: debugfs not found. Please install e2fsprogs." >&2
  exit 1
fi
TMP_DIR=""
FS_IMG=""
cleanup() {
  if [[ -n "${TMP_DIR}" && -d "${TMP_DIR}" ]]; then
    rm -rf "${TMP_DIR}"
  fi
}
trap cleanup EXIT

if [[ -n "${ROOTFS_IMG}" ]]; then
  FS_IMG="${ROOTFS_IMG}"
else
  if [[ ! -f "${DISK_IMG}" ]]; then
    echo "error: disk image not found: ${DISK_IMG}" >&2
    exit 1
  fi
  TMP_DIR="$(mktemp -d)"
  FS_IMG="${TMP_DIR}/rootfs.ext4"
  dd if="${DISK_IMG}" of="${FS_IMG}" bs=512 skip="${EXT4_LBA_BASE}" status=none
fi

ensure_parent_dirs() {
  local full_path="$1"
  local parent="${full_path%/*}"
  local cur=""
  local rest
  local next

  if [[ -z "${parent}" || "${parent}" == "/" ]]; then
    return
  fi

  rest="${parent#/}"
  while [[ -n "${rest}" ]]; do
    next="${rest%%/*}"
    if [[ "${rest}" == *"/"* ]]; then
      rest="${rest#*/}"
    else
      rest=""
    fi
    cur="${cur}/${next}"
    if ! "${DEBUGFS_BIN}" -R "stat ${cur}" "${FS_IMG}" >/dev/null 2>&1; then
      "${DEBUGFS_BIN}" -w -R "mkdir ${cur}" "${FS_IMG}" >/dev/null
    fi
  done
}

ensure_parent_dirs /bin/busybox
for stale in /bin/hello /bin/echo /bin/vfork_exec /bin/cat /bin/pipe_exec /bin/pwd /bin/ls /bin/free /bin/uptime /bin/wget /bin/nc /bin/nslookup /bin/ping; do
  "${DEBUGFS_BIN}" -w -R "rm ${stale}" "${FS_IMG}" >/dev/null 2>&1 || true
done
"${DEBUGFS_BIN}" -w -R "rm /bin/busybox" "${FS_IMG}" >/dev/null 2>&1 || true
"${DEBUGFS_BIN}" -w -R "write ${BUSYBOX_ELF} /bin/busybox" "${FS_IMG}" >/dev/null
for applet in \
  sh basename cat clear cmp cp cut date dd diff dirname du echo env expr false find \
  free head kill ls md5sum mkdir mv nc nl nproc nslookup paste ping printenv \
  printf ps pwd readlink rm rmdir sha1sum sha256sum sha3sum sha512sum sleep sort \
  strings tail tee test tr true tty uname uptime wc wget which xargs yes; do
  "${DEBUGFS_BIN}" -w -R "rm /bin/${applet}" "${FS_IMG}" >/dev/null 2>&1 || true
  "${DEBUGFS_BIN}" -w -R "symlink /bin/${applet} /bin/busybox" "${FS_IMG}" >/dev/null
done

if [[ -z "${ROOTFS_IMG}" ]]; then
  dd if="${FS_IMG}" of="${DISK_IMG}" bs=512 seek="${EXT4_LBA_BASE}" conv=notrunc status=none
fi

echo "installed BusyBox shell: /bin/busybox and /bin/sh -> /bin/busybox"
"${DEBUGFS_BIN}" -R "ls -l /bin" "${FS_IMG}"
