# generated9

> This is not wrong, this is **opinionated**.


This is a fork of [9front](https://9front.org/) where I do invest my about-to-expire LLMs usage quotas.

I only accept LLM-generated code, as long as it makes sense.

## Divergences

I try to stay compatible and up to date with upstream.
Meanwhile, this is where we differ now:

- [Diskless QEMU boot](boot/README.md), with the writable checkout served as
  the guest's root filesystem over 9P. Adapted from
  [Russ Cox's Plan 9 tree](https://github.com/rsc/plan9).
- [Multicore vmx](sys/src/cmd/vmx/README), with xAPIC/x2APIC, improved TSC and
  scheduling, VirGL graphics and VirtIO audio output. Tested with
  [Void Linux, Xfce and Firefox](boot/VOID.md) running inside generated9.
- [Native accelerated 3D](boot/GPU.md): a VirtIO GPU driver with a file
  interface, `libgpu` for VirGL, and TinyGL for a classic OpenGL subset.
  Currently targets QEMU; includes the Utah teapot and `glgears` demos.
- Native [raylib](sys/src/libraylib/README) and
  [raygui](sys/src/libraygui/README), using rio, libdraw and `/dev/audio`,
  plus a [3D model and animation viewer](sys/src/cmd/modelview/README).
- Native [Neverball and Neverputt](sys/src/games/neverball/README), plus
  [Arkanoid, Asteroids and Missile Commander](sys/src/games/raygames/README)
  on raylib. Built with the Plan 9 C toolchain.
- [QuickJS](sys/src/cmd/quickjs/README.Plan9) for JavaScript and
  [Eigenmath](sys/src/cmd/eigenmath/README.Plan9) for symbolic mathematics,
  both built with the Plan 9 toolchain.
- An optional [Go bootstrap and gojq workflow](boot/README.md#go-and-gojq),
  using upstream's existing Plan 9 support without changes to Go or gojq.
- Fixes for amd64 compiler comparisons involving NaN and HDA playback
  underruns, alongside graphics batching and reduced display copying.

## Getting started

```sh
git clone --branch gen https://github.com/paolomarrone/generated9.git
cd generated9
./boot/getbin
./boot/qemu              # serial console; quit with Ctrl-a x
./boot/qemu -vga         # graphical rio desktop
./boot/test              # native build and shared filesystem smoke test
```

See [CREDITS.md](CREDITS.md) for exact source revisions and attribution, and
[LICENSE](LICENSE) for the license of our additions and the locations of the
original component licenses. Original copyright and license notices are retained.
