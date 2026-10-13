# Native VirGL rendering

The amd64 kernel has an experimental native VirtIO GPU driver. `gputest`
renders a triangle through VirGL, reads its pixels back, checks the result,
and displays it with libdraw. `teapot` animates the Utah teapot with smooth
normals, perspective, a depth buffer and per-pixel lighting. Both run
directly in generated9, using the same small VirGL encoder.

The host needs QEMU with `virtio-gpu-gl-pci`, virglrenderer, an OpenGL-capable
display backend, and access to a DRM render node. The guest needs no Mesa,
Linux compatibility layer or external compiler. The GPU is an additional
rendering device; rio keeps its existing VGA display.

Build inside the usual development VM:

```rc
cd /sys/src/9/pc64
mk
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
Tests require the host owner and no other GPU clients
because they temporarily fill the context table.

The initial interface is documented in `gpu(3)`. It uses one synchronous
queue and copies the completed image back to guest memory for libdraw.
It does not accelerate existing rio applications automatically, expose
OpenGL/Vulkan, or drive a physical GPU when booted on bare hardware.
The teapot keeps its 5,408 vertices, 9,216 indexed triangles, shaders and
render targets across frames. Only the transformation matrices change.
Window animation is paced at about 60 fps, and paused windows stop rendering.
`teapot -n 300` measures the full window path; `teapot -b 240` measures
unpaced rendering and readback without a window. Both report separate
submission, readback and presentation times after a warm-up frame.

On the host below, the 640 by 480 headless run measured 841.6 fps over 240
frames (0.33 ms submission and execution, 0.86 ms readback per frame).
An 800 by 600 rio window measured 57.6 fps over 300 paced frames
(0.73 ms submission, 1.54 ms readback, 4.54 ms libdraw presentation).
These are elapsed guest measurements of this small scene, including fence
waits and copies, not GPU-only timings or a general graphics benchmark.

Validated on QEMU 11.1.1 with KVM and an AMD Radeon RX 6750 XT using
Mesa 26.2.4: native kernel and application builds, the device and pixel tests,
twenty renders from two concurrent processes, and display in rio using GTK
with OpenGL. In the EGL headless test, QEMU's amdgpu DRM graphics-engine
counter increased during rendering. This confirms hardware execution;
these checks are not a performance benchmark.
QEMU does not propagate all VirGL renderer errors: an invalid opcode was
acknowledged without an error. The tests check the actual data and pixels;
a successful command write alone does not establish correct rendering.
Booting with the non-GL `virtio-gpu-pci` device was also checked: opening
the driver reports the missing VirGL feature and the guest remains usable.

Source revisions and license notices are in
[UPSTREAM](../sys/src/cmd/gputest/UPSTREAM) and [CREDITS.md](../CREDITS.md).
