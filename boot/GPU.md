# Native VirtIO GPU rendering and display

The amd64 kernel has an experimental native VirtIO GPU driver. `gputest`
renders a triangle through VirGL, reads its pixels back, checks the result,
and displays it with libdraw. `teapot` animates the Utah teapot with smooth
normals, perspective, a depth buffer and per-pixel lighting. Both run
directly in generated9, using the same small VirGL encoder in `libgpu`.
`glgears` uses the new native TinyGL library for a subset of classic OpenGL.

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

## Classic OpenGL through GPU files

Build with the native Plan 9 toolchain:

```rc
cd /sys/src/libgpu
mk install
cd /sys/src/libtinygl
mk install
mk test
cd /sys/src/cmd/glgears
mk install
glgears
```

`glgears` adapts Brian Paul's public-domain classic gears. Space pauses,
arrow keys rotate the view, and `q` exits. `glgears -n 300` measures the full
window path; `glgears -b 300` measures rendering and readback without a window;
`glgears -o /tmp/gears.image` saves a frame.

The application uses `<GL/gl.h>` plus four native binding calls from
`<tinygl.h>`: initialize, resize, read pixels and close. TinyGL 0.4.1 supplies
matrix transformations, vertex lighting, primitive assembly and display lists.
The new backend batches clip-space vertices; VirGL clips, interpolates,
rasterizes and depth-tests them. The library opens `clone`, `cmd` and resource
files through `libgpu`. Passing a device directory to `tglinit` allows normal
namespace bindings; nil uses `#∇`. No kernel GL API is needed.
Window presentation stays in the application and uses libdraw.

Vertex data and draws share a buffered VirGL command stream. A full command
buffer submits automatically; explicit `glFlush`, readback and resource
release also submit pending work. Vertex batches use disjoint buffer ranges
until a completed submission makes reuse safe. This reduces synchronous
device round trips without changing the kernel queue or file protocol.

This is a classic GL **subset**, not full or conformant OpenGL. The
backend supports geometry, typed arrays and buffer objects, matrices,
vertex lighting, culling, display lists, depth testing, textures with mipmaps
and four texture units, alpha testing/blending, polygon offset and point sprites.
Six user clip planes are captured in eye space and clipped on the guest CPU;
ordinary view-volume clipping remains on VirGL.
It has no stencil, GLSL, EGL or GLX. Unsupported operations fail
explicitly. There is one current context per process and errors are fatal,
following TinyGL's model. See [tinygl(2)](../sys/man/2/tinygl) for exact limits.

Tests exercise rendered pixels, image orientation, clipping, homogeneous
coordinates, depth ordering, culling, viewport changes, flat provoking colors,
smooth colors, points, lines, array strides, list replacement and ownership,
large convex polygons, lighting and color-material tracking,
invalid operations and repeated target/context recreation. Existing triangle
and teapot tests also exercise the shared `libgpu` encoder.
Texture tests also exceed both the device's resource count and backing budget,
then check reupload and resizing. TinyGL retains CPU images and evicts unused
GPU copies under pressure. The driver now allows 4096 resources and 256 MiB
of backing per context, with at most 64 MiB per resource. Resources can be
shared between contexts through `gpu(3)`'s `attach` operation. This supports
Mesa clients inside `vmx`; see [VOID.md](VOID.md).

Validation on the development amd64 QEMU guest also passed with the GPU bound
at another namespace path and with the window resized while animating.
A 300-frame `glgears -n 300` run at 808×608 measured about 58 fps including
readback and libdraw presentation, with the VirtIO desktop active. The demo's
17 ms event timer limits the window rate to about 59 fps; this is not a GPU
throughput measurement. Pixel readback and desktop composition remain copies.

## Neverball and Neverputt

The native Neverball and Neverputt ports use this classic GL interface,
libdraw, libttf, the system image readers and a PCM mixer over `/dev/audio`.
They preserve the
original physics and level formats. Both games and the map compiler build with
the Plan 9 C toolchain.

Install the pinned original assets on the host with `./boot/getneverball`.
In the guest, build the current kernel and graphics libraries above, then:

```rc
cd /sys/src/games/neverball
mk install
mk data
games/neverball
games/neverputt
```

Boot the newly built kernel before playing. For sound, QEMU also needs an
emulated sound device, for example `intel-hda` and `hda-duplex` connected to
a host audio backend. `mk data` compiles maps and decodes music and effects
once, using native tools. See the [port README](../sys/src/games/neverball/README)
and the [Neverball](../sys/man/1/neverball) and
[Neverputt](../sys/man/1/neverputt) manuals for setup, controls and limitations.

Three small raylib games use the same accelerated path, with no external
assets or audio. After installing `libraylib`:

```rc
cd /sys/src/games/raygames
mk install
games/arkanoid
games/asteroids
games/missile
```

Each game runs in its own rio window. P pauses, R restarts and Escape exits.
The [README](../sys/src/games/raygames/README) and
[raygames(1)](../sys/man/1/raygames) describe the controls and test options.

## raygui and the model viewer

raygui provides immediate-mode controls over the native raylib port.
The model viewer is a small client of both libraries:

```rc
cd /sys/src/libraylib
mk install
cd /sys/src/libraygui
mk install
cd /sys/src/cmd/modelview
mk install
modelview
```

With no file it shows a torus. It also loads OBJ, IQM, glTF/GLB, M3D and VOX
through raylib, including embedded CPU animation. Drag to orbit, use the wheel
to zoom, and use the toolbar for lighting, wireframe and animation controls.
Static views wait for input. `modelview -n 120 -o /tmp/model.png model.glb`
saves a bounded run. See [modelview(1)](../sys/man/1/modelview) and
[raygui(2)](../sys/man/2/raygui). The renderer still has the classic GL limits
above; loading a modern model format does not add shader-based materials.

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
a complete OpenGL or Vulkan implementation, or drive a physical GPU when
booted on bare hardware.
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

## Multiple rendering clients

A comparison against `9e5221bc9` used a four-CPU KVM guest with 4 GiB RAM,
a 1920 by 1080 VirtIO desktop and four non-overlapping 800 by 450 client
windows. Neverputt ran `-h map-paxed/a.sol -n 900`. Arkanoid, Asteroids and
Missile used seed 1 and paused scenes, with `-n 3600` keeping timed redraws
enabled. This measures command batching with all four clients rendering.
Each comparison used the same kernel, assets and scene, with no second
generated9 VM running during the measurement.

| QEMU display | Neverputt alone, before / after | Four rendering clients, before / after |
| --- | ---: | ---: |
| EGL headless, rio desktop | 61.7 / 61.6 fps | 38.6 / 61.5 fps |
| GTK with OpenGL | 61.6 / 61.5 fps | 44.6 / 61.4 fps |

Temporary instrumented native builds measured 31 vertex uploads and 32
command submissions per Neverputt frame before batching, versus 11 command
submissions carrying both data and draws afterward. Framebuffer download
and desktop presentation are additional operations. The vertex data remained
about 2.10 MiB per frame. In the four-client EGL run, paint time fell from
24.17 to 11.51 ms per frame, excluding readback and libdraw presentation;
phase measurements omit the first 60 frames. FPS above covers all 900 frames
and approaches Neverputt's animation timer limit after batching.

Interactive arcade runs also wait for events while paused or game over.
In separate pause checks, frame counters and application user/kernel CPU
times remained unchanged over one-second idle intervals. Resume, restart,
resize while paused and Escape exit were exercised for all three games;
all GPU contexts were released on exit. Fixed-frame runs keep redrawing
so tests and captures terminate without input.
The installed, uninstrumented binaries also completed a simultaneous
600-frame GTK run with all four games active; Neverputt reported 61.7 fps.
The displayed games were inspected and the three arcade captures completed.

These are elapsed measurements of specific scenes, including scheduling,
copies and GPU waits. They do not establish a maximum rendering rate or
guarantee 60 fps in other levels. CPU vertex transforms, pixel readback and
software window composition remain in the path.

## Vertex transport review

A later pass compared the libraries at `015e4cf17` with compact TinyGL
vertex batches. The four-CPU, 4 GiB KVM guest used the same 1920 by 1080
VirtIO desktop, EGL headless display and an 800 by 450 Neverputt window.
Temporary instrumented native builds ran `-h map-paxed/a.sol -n 900`, with
the animation timer removed and simulation advancing by 1/60 second per
frame. Each run omitted 60 warm-up frames. Three before/after pairs ran
alternately, without other rendering clients; the table reports medians.

| Per frame | Before | After |
| --- | ---: | ---: |
| Elapsed time | 6.37 ms | 5.14 ms |
| Vertex payload | 2136.0 KiB | 832.5 KiB |
| Command submissions | 11 | 4 |

This is about 19% less elapsed time, or 24% higher unpaced throughput, for
this scene. Framebuffer download and desktop transfers are additional to
the command submissions counted above. These measurements include guest
CPU work, copies and GPU waits; they are not GPU-only timings or paced
gameplay FPS.

TinyGL now prepares raster state once per `glBegin`, and sends only vertex
attributes used by that batch. All active attributes retain float precision.
The command encoder skips clearing inline payload bytes that it immediately
overwrites, while preserving zeroed headers and padding. The kernel reuses
one lazily allocated 256 KiB command buffer plus its 32-byte header under
the existing lock and completion fence, instead of allocating and clearing
a buffer on every write. Both sides of the comparison used this updated
kernel, so the table isolates the library changes.

Native tests cover device ownership and resource limits, multi-buffer inline
writes, partial words, clipping, texture residency, sparse texture units,
vertex layout changes, lighting, masked clears, readback and context reuse.
The raylib graphics, model, animation and timing tests and raygui interaction
tests also pass. Concurrent command clients exercise the shared staging
buffer. Neverball, modelview with an animated GLB robot, and the three raylib
games completed a simultaneous 1,200-frame run. Separate checks exercised
Neverball gameplay, modelview resizing and device resource reuse after all
clients exited. The file protocol and public library interfaces are unchanged.

The remaining path still transforms vertices on the CPU, reads rendered
pixels back to the guest, and uses software window composition. The display
process holds the draw lock during its synchronous transfer. Removing these
costs would require a larger design change than reducing redundant state
work and unused vertex data.
