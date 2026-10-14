# Void Linux inside generated9

This runs the official Void Xfce live system inside generated9's `vmx`.
The outer QEMU runs generated9; `vmx` runs Linux with four virtual CPUs,
2 GB of RAM, virtio disk/network devices and a 1024×768 VirtIO GPU display.
The live filesystem is temporary: changes disappear when it stops.

Build the generated9 kernel and install the monitor first if needed; see
[/sys/src/cmd/vmx/README](../sys/src/cmd/vmx/README). The bootstrap kernel
in `amd64/9pc64` predates these changes. Accelerated graphics also needs
the current `gpu(3)` driver, including resource sharing, and an OpenGL-capable
QEMU host; see [GPU.md](GPU.md).

On the Linux host, from the checkout root:

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

This uses SDL with GLX on an X11 host. With QEMU 11.1.1 and virglrenderer
1.3.0, the EGL path returned corrupt timestamp queries when polling after a
context switch. This was reproduced through native `gpu(3)` too, without
vmx. Selecting GLX passed the query regression without an injected library
or a host patch; SDL's default EGL path and GTK both failed. See
`sys/src/cmd/vmx/test/virgl.c`. This backend selection is specific to the
tested outer host, not a requirement of the vmx transport.
The relevant paths are QEMU's
[`virtio_gpu_fence_poll`](https://github.com/qemu/qemu/blob/v11.1.1/hw/display/virtio-gpu-virgl.c)
and virglrenderer's
[`vrend_renderer_poll`](https://chromium.googlesource.com/chromiumos/third_party/virglrenderer/+/refs/tags/1.3.0/src/vrend/vrend_renderer.c).

EGL's asynchronous fence path is faster on this host. To use it with
virglrenderer 1.3.0, [virglrenderer-poll.patch](virglrenderer-poll.patch)
resets the current GL context before polling queries, as the synchronous
path already does. Build the library locally on Linux with a C compiler,
Meson, Ninja, Python with PyYAML, and the libepoxy, EGL, GBM, libdrm and X11
development packages:

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

This replaces the library only for this QEMU process; the installed host
packages are unchanged. No host library is linked into native generated9
programs. The patch retains virglrenderer's MIT license and exact revision.

For audio on an outer Linux host running PulseAudio or PipeWire's PulseAudio
server, append these options after `--` in either QEMU command above:

```sh
-audiodev pa,id=audio0 -device intel-hda -device hda-duplex,audiodev=audio0
```

The generated9 HDA driver provides `/dev/audio`. Keep its default 44100 Hz
rate. The nested guest needs no audio configuration; its desktop audio
server discovers the VirtIO sound card. Build and boot the current generated9
kernel too: the HDA driver detects playback underruns even when an interrupt
arrives after DMA has passed the last queued sample. Previously this could
look like an almost full ring and block a write for over a second. Omit `-a /dev/audio` below if the
outer VM has no sound device. Other native programs can share the output
through `audio/mixfs` in the monitor's namespace.

Inside a rio terminal:

```rc
cd /tmp/void-vmx
echo type virtio > /dev/vgactl
vmx -P 4 -M 2G -w 1024x768 -g '#∇' -a /dev/audio -n ether0 \
    -d void-live-x86_64-20250202-xfce.iso \
    -m initrd vmlinuz \
    'root=live:CDLABEL=VOID_LIVE init=/sbin/init ro rd.luks=0 rd.md=0 rd.dm=0 rd.live.overlay.overlayfs=1'
```

Click inside the `vmx` window to capture input. Ctrl+Alt releases it.
LightDM starts automatically and logs into Xfce as `anon`, password
`voidlinux`. Root uses the same password. Xorg selects `virtio_gpu` and
initializes glamor; Mesa reports the `virgl` renderer with direct rendering.
Open Firefox from Xfce. No graphics preferences need to be forced.

The tested combination is Linux 6.12.11_1, Mesa 24.2.8 and Firefox 134.0 from
this ISO. `glxinfo -B` reports accelerated rendering, OpenGL 4.0 core and
OpenGL ES 3.0. Firefox's `about:support` reports WebRender, adapter `virgl`
and hardware compositing available. These are the guest driver's reported
capabilities, not an OpenGL conformance claim for generated9's native TinyGL.

Rendering reaches the outer host GPU through `vmx` and `gpu(3)`. Video
decoding still runs on the guest CPU. Audio uses VirtIO Sound and the host
`/dev/audio`; the unmodified ISO supplies `virtio_snd` and PipeWire. Presentation
reads pixels back and copies changed rows into the rio window. Resolution, codec
and the cost of nested CPU virtualization still affect video playback.

Audio was checked through ALSA, PipeWire and Firefox, with the stereo output
captured from the outer QEMU stream. Repeated playback and idle transitions,
and detaching and reattaching the guest sound driver, were exercised. The
VirtIO sound regression in `mk test` also covers malformed requests, host
open failures and reset with a full monitor notification channel.

With the corrected host renderer, a local 854x480 H.264 video at 30 fps
presented 550 frames in a 20.2-second interval after startup (about 27 fps),
with 56 dropped frames in that interval. This is one Firefox playback sample,
not a general throughput guarantee. The timestamp regression passed all
300 reused queries; native driver and TinyGL pixel tests also passed.

YouTube's Big Buck Bunny played in a normally launched Firefox, with its
player selecting 480p AV1 and later VP9. Playback continued for several
minutes, but dropped many frames and sometimes fell behind wall time.
Smooth YouTube playback remains an open performance issue; the local H.264
measurement does not establish that result.

For the original software framebuffer path, omit `-g '#∇'`; the outer GPU
is then optional. Log in to the Linux console as `root` and run:

```sh
sed -i 's/#logind-check-graphical=true/logind-check-graphical=false/' /etc/lightdm/lightdm.conf
sv restart lightdm
```

Without a DRM device, LightDM waits for a graphical seat reported by logind.
This setting lets Xorg start with its software `fbdev` driver.
Use Xfce's shutdown action or `sudo poweroff` before closing the VM.
Without ACPI, Linux halts instead of turning the virtual machine off.

The tested image contains Linux 6.12.11_1. Four CPUs (`0-3`), Xfce/Xorg,
keyboard input, PS/2 mouse events, DHCP, DNS and an HTTPS download were checked.
Four concurrent SHA-256 jobs, each pinned to a different CPU and hashing
64 MiB of zeroes, matched the result calculated on the Linux host.

The tested Intel host provides TSC_ADJUST; the guest selects `tsc` without
clocksource overrides. ACPI is absent, so its missing-RSDP message is expected.
Closing the monitor releases its virtual CPUs and guest memory. Two consecutive
boots to Xfce were checked in the same 6 GB outer VM. After closing the first
guest, allocated user memory dropped from about 2063 MiB to 8 MiB, and `#X`
contained only `clone`.

The ISO is unmodified Void software with its original package licenses.
It, the extracted boot files and local logs remain in the ignored temporary
directory. The download and checksum come from Void's
[release directory](https://repo-default.voidlinux.org/live/20250202/);
see also the [Void live image documentation](https://docs.voidlinux.org/installation/live-images/).
