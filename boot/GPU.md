# Native VirtIO GPU rendering and display

The amd64 kernel has an experimental native VirtIO GPU driver. `gputest`
renders a triangle through VirGL, reads its pixels back, checks the result,
and displays it with libdraw. `teapot` animates the Utah teapot with smooth
normals, perspective, a depth buffer and per-pixel lighting. Both run
directly in generated9, using the same small VirGL encoder in `libgpu`.

The host needs QEMU with `virtio-gpu-gl-pci`, virglrenderer, an OpenGL-capable
display backend, and access to a DRM render node. The guest needs no Mesa,
Linux compatibility layer or external compiler. Rendering can use an additional
GPU while rio keeps its VGA display. The optional `virtio` display backend
also presents rio through VirtIO GPU, using the existing software screen
and draw protocol.

Build inside the usual development VM:

```rc
cd /sys/src/9/pc64
mk
cd /sys/src/libgpu
mk install
cd /sys/src/cmd/gputest
mk install
```

Then boot the new kernel with graphics and VirGL:

```sh
./boot/qemu -vga -m 3G -smp 2 -kernel sys/src/9/pc64/9pc64 -- \
  -device virtio-gpu-gl-pci -display gtk,gl=on
```

In a rio window:

```rc
gputest
teapot
```

In `teapot`, space pauses rotation and resizing recreates the render targets.
Type `q` to close either program. QEMU may also show a second, unused display
for the off-screen GPU; select the standard VGA display containing rio.

## Presenting the desktop through VirtIO

Use a single VirtIO VGA device for both the initial VESA screen and GPU
presentation:

```sh
./boot/qemu -vga -m 3G -smp 2 -kernel sys/src/9/pc64/9pc64 -- \
  -vga none -device virtio-vga-gl -display gtk,gl=on
```

After rio starts, select the display backend in a terminal:

```rc
echo type virtio > /dev/vgactl
teapot
```

The initial screen must be a 32-bit `x8r8g8b8` software screen with a
software cursor and no rotation, as configured by `boot/qemu -vga`.
The driver supports scanout zero and screen sizes up to 4096 by 4096.
It recreates the display resource when the screen dimensions change.
There is no automatic host-window resizing or hardware cursor yet.

Screen flushes notify a kernel process, which combines pending damage,
copies the affected rectangle into private backing memory, transfers it
to the host resource and flushes the scanout. Transfers complete before
that backing is reused or freed. The process also handles cursor and kernel
console updates, whose callers cannot always wait for a device command.
Display errors are printed on the kernel console and suspend updates;
selecting the controller again retries, unless the device has failed.

The 2D path also works with `-device virtio-vga -display gtk` without VirGL.
On that device `caps` is empty and opening a 3D context reports that VirGL
is required. The desktop still uses software drawing and composition.
Teapot rendering still reads pixels back for libdraw; this change replaces
the final VGA framebuffer writes with explicit VirtIO transfers.

## Rendering tests

For serial-console tests, use EGL without a graphical window:

```sh
./boot/qemu -m 3G -smp 2 -kernel sys/src/9/pc64/9pc64 -- \
  -device virtio-gpu-gl-pci \
  -display egl-headless,rendernode=/dev/dri/renderD128
```

Choose the render node belonging to the intended host GPU. Inside generated9:

```rc
cd /sys/src/cmd/gputest
mk test
6.gputest -o /tmp/triangle.image
topng /tmp/triangle.image > /tmp/triangle.png
6.teapot -b 240 -o /tmp/teapot.image
```

`mk test` checks capabilities, transfers in both directions, resource bounds
and ownership, stale descriptors, context lifetime, resource and context
exhaustion and reuse, then verifies the rendered triangle and teapot.
The teapot test reverses triangle order to check depth occlusion, checks
lighting and rotation, and recreates the colour and depth targets.
Tests require the host owner and no other rendering clients
because they temporarily fill the context table. The display can remain active.

The interface is documented in `gpu(3)`. It uses one synchronous
queue and copies the completed image back to guest memory for libdraw.
It does not accelerate drawing in existing rio applications, expose
OpenGL/Vulkan, or drive a physical GPU when booted on bare hardware.
The teapot keeps its 5,408 vertices, 9,216 indexed triangles, shaders and
render targets across frames. Only the transformation matrices change.
Window animation is paced at about 60 fps, and paused windows stop rendering.
`teapot -n 300` measures the full window path; `teapot -b 240` measures
unpaced rendering and readback without a window. Both report separate
submission, readback and presentation times after a warm-up frame.
`-n` also submits pending draw commands between pixel loading, composition
and screen flushing to time those stages separately.

## Measurements

On the host below, the 640 by 480 headless run measured 841.6 fps over 240
frames (0.33 ms submission and execution, 0.86 ms readback per frame).
An 800 by 600 rio window measured 57.6 fps over 300 paced frames
(0.73 ms submission, 1.54 ms readback, 4.54 ms libdraw presentation).
These are elapsed guest measurements of this small scene, including fence
waits and copies, not GPU-only timings or a general graphics benchmark.

A later comparison used the same two-CPU VM, a 1920 by 1080 desktop and
`virtio-vga-gl`, first with its legacy VESA framebuffer and then with the
native VirtIO display selected. Each run used `teapot -n 300`:

| Window size | VESA presentation | VirtIO presentation | VESA guest CPU | VirtIO guest CPU |
| --- | ---: | ---: | ---: | ---: |
| 800 by 600 | 6.08 ms/frame | 0.98 ms/frame | 1.99 s | 0.57 s |
| 1280 by 720 | 7.02 ms/frame | 1.76 ms/frame | 2.28 s | 1.06 s |
| Two 800 by 600 windows | 3.35 / 3.36 ms/frame | 1.33 / 2.49 ms/frame | 2.14 s | 1.25 s |

Both paths sustained about 58 fps per window, as limited by the animation
timer. Presentation measures application calls, which return before the
asynchronous VirtIO display work completes. Guest CPU totals include the
applications' user and kernel time plus the `virtiodisplay` process delta
from `/proc/pid/status`, counted once for the pair. They exclude unrelated
guest processes and host CPU/GPU work. This avoids treating work moved into
the display process as a saving. The shared synchronous queue still causes
contention: readback waits increased with two rendering clients.

Keep the host display awake when measuring the graphical path. On this
host, DPMS monitor power-off made QEMU's GL presentation stall for about
one second per update; waking the monitor immediately restored throughput.

Validated on QEMU 11.1.1 with KVM and an AMD Radeon RX 6750 XT using
Mesa 26.2.4: native kernel and application builds, the device and pixel tests,
twenty renders from two concurrent processes, and display in rio using GTK
with OpenGL. In the EGL headless test, QEMU's amdgpu DRM graphics-engine
counter increased during rendering. This confirms hardware execution;
these checks are not a performance benchmark.
QEMU does not propagate all VirGL renderer errors: an invalid opcode was
acknowledged without an error. The tests check the actual data and pixels;
a successful command write alone does not establish correct rendering.
The display backend was also exercised with overlapping windows, pause and
resume, window resizing, concurrent controller reactivation and screen-size
changes, and recovery after rejecting an unsupported pixel format.
With the non-GL `virtio-vga` device, the displayed pixels of a 600 by 424
test image matched all 254,400 expected RGB values, both before and after
moving and redrawing its window. Its capability file was empty and opening
a rendering context failed with the expected VirGL error.

Source revisions and license notices are in
[UPSTREAM](../sys/src/cmd/gputest/UPSTREAM) and [CREDITS.md](../CREDITS.md).
