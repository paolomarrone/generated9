# Void Linux inside generated9

This runs the official Void Xfce live system inside generated9's `vmx`.
The outer QEMU runs generated9; `vmx` runs Linux with four virtual CPUs,
2 GB of RAM, virtio disk/network devices and a 1024×768 framebuffer.
The live filesystem is temporary: changes disappear when it stops.

Build the generated9 kernel and install the monitor first if needed; see
[/sys/src/cmd/vmx/README](../sys/src/cmd/vmx/README). The bootstrap kernel
in `amd64/9pc64` predates the multiprocessor changes.

On the Linux host, from the checkout root:

```sh
mkdir -p usr/glenda/tmp/void-vmx
cd usr/glenda/tmp/void-vmx
curl -fLO https://repo-default.voidlinux.org/live/20250202/void-live-x86_64-20250202-xfce.iso
echo '40ff378e6cbc74680b30eb0af0003af0abc750b2589f18d4733f9f30bcc79d5b  void-live-x86_64-20250202-xfce.iso' | sha256sum -c -
7z e void-live-x86_64-20250202-xfce.iso boot/vmlinuz boot/initrd
cd ../../../..
./boot/qemu -vga -m 6G -smp 4 -kernel sys/src/9/pc64/9pc64
```

Inside a rio terminal:

```rc
cd /tmp/void-vmx
vmx -P 4 -M 2G -w 1024x768 -n ether0 \
    -d void-live-x86_64-20250202-xfce.iso \
    -m initrd vmlinuz \
    'root=live:CDLABEL=VOID_LIVE init=/sbin/init ro rd.luks=0 rd.md=0 rd.dm=0 rd.live.overlay.overlayfs=1'
```

Click inside the `vmx` window to capture input. Ctrl+Alt releases it.
Log in to the Linux console as `root`, password `voidlinux`, then run:

```sh
sed -i 's/#logind-check-graphical=true/logind-check-graphical=false/' /etc/lightdm/lightdm.conf
sv restart lightdm
```

LightDM normally waits for a graphical seat reported by logind. With this
setting it starts Xorg, which finds the VESA framebuffer through its `fbdev`
driver. The live image then logs into Xfce as `anon`, also with password
`voidlinux`. No separate Xorg configuration is needed for this image.
Use Xfce's shutdown action or `sudo poweroff` before closing the VM.
Without ACPI, Linux halts instead of turning the virtual machine off.

The tested image contains Linux 6.12.11_1. Four CPUs (`0-3`), Xfce/Xorg,
keyboard input, PS/2 mouse events, DHCP, DNS and an HTTPS download were checked.
Four concurrent SHA-256 jobs, each pinned to a different CPU and hashing
64 MiB of zeroes, matched the result calculated on the Linux host.

Graphics use software rendering at a fixed resolution. Audio is not provided.
The tested Intel host provides TSC_ADJUST; the guest selects `tsc` without
clocksource overrides. ACPI is absent, so its missing-RSDP message is expected.
Closing the monitor releases its virtual CPUs and guest memory. Two consecutive
boots to Xfce were checked in the same 6 GB outer VM. After closing the first
guest, allocated user memory dropped from about 2063 MiB to 8 MiB, and `#X`
contained only `clone`. Keep at least four CPUs in the outer VM: oversubscribing
them currently causes long stalls with the TSC clock enabled.

The ISO is unmodified Void software with its original package licenses.
It, the extracted boot files and local logs remain in the ignored temporary
directory. The download and checksum come from Void's
[release directory](https://repo-default.voidlinux.org/live/20250202/);
see also the [Void live image documentation](https://docs.voidlinux.org/installation/live-images/).
