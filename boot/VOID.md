# Void Linux in vmx

This boots the unmodified Void Xfce live image with four CPUs, 2 GB RAM,
VirtIO graphics and stereo audio. Its live filesystem is lost at shutdown.
The outer QEMU needs KVM with nested Intel VMX enabled.

Build and boot the current generated9 kernel and install `vmx`; see its
[README](../sys/src/cmd/vmx/README). Accelerated display also needs the
[GPU driver](GPU.md). The bootstrap kernel is too old.

## Host setup

From the checkout root on an X11 Linux host:

```sh
mkdir -p usr/glenda/tmp/void-vmx
cd usr/glenda/tmp/void-vmx
curl -fLO https://repo-default.voidlinux.org/live/20250202/void-live-x86_64-20250202-xfce.iso
echo '40ff378e6cbc74680b30eb0af0003af0abc750b2589f18d4733f9f30bcc79d5b  void-live-x86_64-20250202-xfce.iso' | sha256sum -c -
7z e void-live-x86_64-20250202-xfce.iso boot/vmlinuz boot/initrd
cd ../../../..
SDL_VIDEO_X11_FORCE_EGL=0 ./boot/qemu -vga -m 8G -smp 4 \
    -kernel sys/src/9/pc64/9pc64 -- \
    -vga none -device virtio-vga-gl -display sdl,gl=on
```

The SDL/GLX selection avoids a query-polling defect observed with QEMU
11.1.1 and virglrenderer 1.3.0 under EGL. For the faster EGL path, build
virglrenderer with [virglrenderer-poll.patch](virglrenderer-poll.patch),
which restores the GL context before asynchronous query polling.
The build needs a C compiler, Meson, Ninja, Python/PyYAML, and development
packages for libepoxy, EGL, GBM, libdrm and X11:

```sh
renderer=usr/glenda/tmp/virglrenderer
git clone --depth 1 --branch 1.3.0 \
    https://chromium.googlesource.com/chromiumos/third_party/virglrenderer "$renderer"
git -C "$renderer" apply "$PWD/boot/virglrenderer-poll.patch"
meson setup "$renderer/build" "$renderer" -Dplatforms=egl,glx \
    -Dtests=false -Dvenus=false -Dvideo=false
meson compile -C "$renderer/build"
LD_LIBRARY_PATH="$PWD/$renderer/build/src" \
    ./boot/qemu -vga -m 8G -smp 4 -kernel sys/src/9/pc64/9pc64 -- \
    -vga none -device virtio-vga-gl -display gtk,gl=on
```

`LD_LIBRARY_PATH` selects this renderer for QEMU only.
The regression test is `sys/src/cmd/vmx/test/virgl.c`; see the vmx README.
Source revisions and licenses are in [CREDITS.md](../CREDITS.md).

For PulseAudio or PipeWire's PulseAudio server, append after `--`:

```sh
-audiodev pa,id=audio0 -device intel-hda -device hda-duplex,audiodev=audio0
```

Keep generated9's `/dev/audio` at 44100 Hz. Use `audio/mixfs` in vmx's
namespace to share it with native applications. Without a sound device,
omit `-a /dev/audio` below.

## Guest setup

Inside a generated9 rio terminal:

```rc
cd /tmp/void-vmx
echo type virtio > /dev/vgactl
vmx -P 4 -M 2G -w 1024x768 -g '#∇' -a /dev/audio -n ether0 \
    -d void-live-x86_64-20250202-xfce.iso \
    -m initrd vmlinuz \
    'root=live:CDLABEL=VOID_LIVE init=/sbin/init ro rd.luks=0 rd.md=0 rd.dm=0 rd.live.overlay.overlayfs=1'
```

Click in the window to capture input; Ctrl+Alt releases it.
Xfce logs in as `anon`; both `anon` and `root` use password `voidlinux`.
Open Firefox from Xfce. Xorg, Mesa and PipeWire use the standard VirtIO
drivers; no guest patches or Firefox graphics overrides are needed.

The ISO contains Linux 6.12.11_1, Mesa 24.2.8 and Firefox 134.0.
`glxinfo -B` reports `virgl`, OpenGL 4.0 core and OpenGL ES 3.0;
Firefox's `about:support` reports WebRender. These are guest Mesa
capabilities, separate from native TinyGL's classic GL subset.

Video decoding runs on the guest CPU. Presentation reads pixels back and
copies changed rows to rio. YouTube plays, but can drop frames and fall
behind wall time; smooth playback is not assured. Audio was checked through
ALSA, PipeWire and Firefox, including repeated playback and driver resets.

## Software display

Omit `-g '#∇'` to use the software framebuffer; the outer GPU is then
optional. Log in to the Linux console as `root` and allow LightDM to start
without a DRM seat:

```sh
sed -i 's/#logind-check-graphical=true/logind-check-graphical=false/' /etc/lightdm/lightdm.conf
sv restart lightdm
```

Use Xfce's shutdown action or `sudo poweroff` before closing vmx.
There is no ACPI: the missing-RSDP message is expected, and Linux halts
instead of powering off. Closing vmx releases its CPUs and memory.
The tested Intel host supplies TSC_ADJUST; no clocksource overrides are needed.

The ISO, boot files and logs stay in the ignored temporary directory.
Package licenses are unchanged. Download and checksum:
[Void release directory](https://repo-default.voidlinux.org/live/20250202/).
See also [Void live images](https://docs.voidlinux.org/installation/live-images/).
