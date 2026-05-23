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
