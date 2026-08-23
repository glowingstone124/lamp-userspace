# Lamp Userspace

This repository now targets BusyBox as the primary userland.

Keep the ABI headers, crt, libc compatibility layer, sysroot headers, linker script,
and BusyBox helper scripts here. The older standalone smoke binaries were removed so
the init path can focus on `/bin/sh -> /bin/busybox`.

Build BusyBox:

```bash
bash user/build_busybox.sh
```

Install BusyBox into `disk.img`:

```bash
bash user/install_busybox_to_disk.sh --input busybox-1.37.0/busybox
```

The installer writes `/bin/busybox` and creates `/bin/sh -> /bin/busybox`.
Kernel init starts `/bin/sh` directly after boot and respawns it if the shell exits.

Signal delivery regression source lives at `tests/signal_delivery.c`. After the
runtime objects have been built, compile it with `lamp-cc.sh`; the test covers a
blocked pending signal, delivery after unmask, handler return through
`sigreturn`, and repeated delivery.

Networking applet notes, including the current `wget` statusbar setting and NAT
test flow, are documented in `../docs/networking.md`.

## CoreMark

CoreMark is kept as the top-level `../coremark` submodule so the EEMBC benchmark
sources stay unmodified.  Build the LAMP port, then install the resulting static
ELF into the ext4 guest rootfs:

```bash
export LAMP_CLANG=/path/to/lamp-llvm/bin/clang
bash user/build_coremark.sh
bash user/install_coremark_to_disk.sh --input build-user/coremark/coremark
```

Boot the guest and run `/bin/coremark`.  It defaults to CoreMark's 2 KiB
performance parameters and calibrates its iteration count to a run of at least
ten seconds.  To use an explicit number of iterations, pass the standard seeds
and iteration count at runtime:

```sh
/bin/coremark 0 0 0x66 1000
```

The LAMP port uses its paced monotonic clock and CoreMark's integer-time mode,
because the guest ISA provides binary32 rather than double-precision floating
point.  A valid run still prints the standard CRC validation lines; use a run of
at least ten seconds for reportable results as required by EEMBC.

CoreMark builds at `-O2` by default.  Set `COREMARK_OPT_LEVEL=-O0` (or another
supported Clang optimization level) only when comparing compiler behavior.
