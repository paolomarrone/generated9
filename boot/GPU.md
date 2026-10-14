# Graphics

The amd64 kernel's VirtIO GPU driver exposes VirGL through files.
`libgpu` encodes commands; TinyGL supplies a classic OpenGL subset.
Rendering and vertex projection use the host GPU. Lighting and user clipping
remain on the guest CPU. Completed window images are read back for libdraw;
render textures can be sampled directly on the GPU.

The host needs QEMU with VirGL, virglrenderer, an OpenGL display backend
and access to a DRM render node. The guest uses the Plan 9 C toolchain.
This driver targets QEMU, not physical GPUs.

## Build

Inside generated9:

```rc
cd /sys/src/9/pc64
mk
cd /sys/src/libgpu
mk install
cd /sys/src/libtinygl
mk install
cd /sys/src/cmd/gputest
mk install
cd /sys/src/cmd/glgears
mk install
```

Restart with the new kernel and a VirtIO VGA device:

```sh
./boot/qemu -vga -m 3G -smp 2 -kernel sys/src/9/pc64/9pc64 -- \
  -vga none -device virtio-vga-gl -display gtk,gl=on
```

In a rio window, select VirtIO display transfers and run a demo:

```rc
echo type virtio > /dev/vgactl
teapot
```

`gputest` checks and displays a triangle; `teapot` and `glgears` animate.
Space pauses the animations; `q` exits. See
[teapot(1)](../sys/man/1/teapot) and [glgears(1)](../sys/man/1/glgears)
for frame limits, timing and image output.

## Display

The `virtio` controller presents rio's software screen through scanout zero.
It needs `x8r8g8b8`, a software cursor, no rotation, and dimensions at most
4096 by 4096. `boot/qemu -vga` supplies a suitable initial VESA screen.
Screen updates are asynchronous and combine pending damage. Drawing and
window composition remain in software. Host-window resizing is not automatic.

On a display error, reselect the controller to retry. A failed device needs
a reboot. The 2D path also works with `-device virtio-vga -display gtk`, but
that device has no VirGL contexts. See [gpu(3)](../sys/man/3/gpu).

Keep the host display awake when measuring: DPMS power-off can stall QEMU's
GL presentation. For nested Linux graphics, see the virglrenderer 1.3.0
query-polling issue and setup in [VOID.md](VOID.md).

## Libraries and programs

TinyGL supports geometry, matrices, lighting, display lists, textures and
fixed-function blending, plus color/depth render textures. It is not a
conformant OpenGL implementation; there is no GLSL, stencil, EGL or GLX.
One context per process is supported,
and invalid operations are fatal. See [tinygl(2)](../sys/man/2/tinygl).
The lower-level interfaces are [gpu(2)](../sys/man/2/gpu) and
[gpu(3)](../sys/man/3/gpu); device directories can be rebound in a namespace.

Install raylib, raygui and the model viewer in that order:

```rc
cd /sys/src/libraylib
mk install
cd /sys/src/libraygui
mk install
cd /sys/src/cmd/modelview
mk install
modelview
```

`modelview` shows a torus or loads OBJ, IQM, glTF/GLB, M3D and VOX files,
including embedded animation. Materials retain the classic GL limits.
See [modelview(1)](../sys/man/1/modelview), [raylib(2)](../sys/man/2/raylib)
and [raygui(2)](../sys/man/2/raygui).

For Neverball and Neverputt, fetch assets on the host:

```sh
./boot/getneverball
```

Then build and convert the data inside generated9:

```rc
cd /sys/src/games/neverball
mk install
mk data
games/neverball
games/neverputt
```

Both games use the native graphics, image, font and audio libraries.
See their [README](../sys/src/games/neverball/README) for data and sound setup.

The raylib arcade games need no external assets:

```rc
cd /sys/src/games/raygames
mk install
games/arkanoid
games/asteroids
games/missile
```

See [raygames(1)](../sys/man/1/raygames) for controls and capture options.

## Tests

Device and pixel tests can run without a display window:

```sh
./boot/qemu -m 3G -smp 2 -kernel sys/src/9/pc64/9pc64 -- \
  -device virtio-gpu-gl-pci \
  -display egl-headless,rendernode=/dev/dri/renderD128
```

Choose the intended GPU's render node. Inside generated9:

```rc
cd /sys/src/cmd/gputest
mk test
cd /sys/src/libtinygl
mk test
```

`gputest` checks device lifetime, ownership, transfers, resource limits and
rendered pixels. Run it as the host owner with no other rendering clients:
its exhaustion tests fill the context table. The desktop may remain active.
TinyGL tests cover the supported GL operations and resource reuse.
Raylib and raygui tests need a rio window; see their source READMEs.

## Limits and performance

The driver allows one request in flight. `submit` returns after copying the
commands; `wait` completes them. The original `cmd` file remains synchronous.
TinyGL batches vertices and draws; `glFlush` submits, `glFinish` also waits.
Lighting, pixel readback and software composition still cost guest CPU time.
The display holds the draw lock only while copying pixels into private memory.
Paused demos and idle arcade windows stop rendering.

Read `#∇/stats` before and after a workload to separate display copy time,
draw-lock duration and device waits. See [gpu(3)](../sys/man/3/gpu) for fields.
These counters include all clients; wait time is not GPU execution time.

`teapot -n 300` measures the window path; `teapot -b 240` measures unpaced
rendering and readback. Timings include copies and fence waits, not just GPU
work. Display transfers may finish after the measured libdraw calls return;
include the display process when comparing CPU use. Paced FPS is not a measure
of maximum throughput.

Tested on amd64, QEMU 11.1.1/KVM, Radeon RX 6750 XT and Mesa 26.2.4.
Source revisions and licenses are in [CREDITS.md](../CREDITS.md).
