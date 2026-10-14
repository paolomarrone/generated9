# Credits and source provenance

New generated9 code uses the root [MIT license](LICENSE).
Imported and adapted code retains the authorship and licenses listed below.

## Plan 9 and 9front

Based on Plan 9 from Bell Labs and [9front](https://github.com/9front/9front),
`front` at `70233ea4a4fe9b905ea8f3a4c4e514a7df5d8b8b`.
Upstream history and notices are preserved; see [NOTICE](lib/legal/NOTICE)
and [MIT](lib/legal/mit). Local configuration starts from `sys/lib/dist`.

The amd64 NaN comparison fix follows Richard Miller's `5c-nan-cmp` design,
integrated by cinap_lenrek in
[`0d7e3fd4a27a8494d8e513b43cac12b7d9b9dea6`](https://github.com/9front/9front/commit/0d7e3fd4a27a8494d8e513b43cac12b7d9b9dea6).
It uses the same `BTRUE` flag with amd64 comparison and branch selection.

## Russ Cox's diskless boot workflow

`boot/qemu` adapts [rsc/plan9](https://github.com/rsc/plan9/tree/13bd0c334646e584ddecc1b7b106c0183b164273/boot)
at `13bd0c334646e584ddecc1b7b106c0183b164273`: QEMU starts u9fs to serve the
checkout as the guest root. [LICENSE.rsc](boot/LICENSE.rsc) preserves the
source tree's MIT notice and Plan 9 Foundation copyright.
The [9legacy boot scripts](https://github.com/0intro/9legacy/tree/main/boot)
were consulted but not imported.

## u9fs

The 34 original files in [u9fs](sys/src/cmd/unix/u9fs) were imported unchanged
from that same rsc/plan9 revision. See its [UPSTREAM](sys/src/cmd/unix/u9fs/UPSTREAM)
and [LICENSE](sys/src/cmd/unix/u9fs/LICENSE). The required permission notice:

```text
The authors of this software are Bob Flandrena, Ken Thompson,
Rob Pike, and Russ Cox.

		Copyright (c) 1992-2002 by Lucent Technologies.

Permission to use, copy, modify, and distribute this software for any
purpose without fee is hereby granted, provided that this entire notice
is included in all copies of any software which is or includes a copy
or modification of this software and in all copies of the supporting
documentation for such software.

THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
```

## Bootstrap distribution

`boot/getbin` verifies and extracts `9front-11952.amd64.iso.gz`, using the
SHA-256 in the [release announcement](https://9front.org/releases/2026/08/02/0/).
The ISO and runtime are not committed; their distribution licenses apply.

## VirtIO GPU and VirGL

The driver implements [VirtIO 1.2, section 5.7](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html).
Wire layouts also follow Linux v6.12 `virtio_gpu.h`, revision
`adc218676eef25575469234709c2d87185ca223a`, by Dave Airlie and Gerd Hoffmann.
The Red Hat 2013–2014 copyright and BSD notice remain in
[devgpu.c](sys/src/9/pc/devgpu.c). PCI and queue handling follow 9front.

The command encoder and triangle adapt virglrenderer tests and definitions
at `4d8bd3c9954a9599e0de70a2e432c761da80398d`, copyright Red Hat 2014–2015,
under MIT. Source paths and notices:
[gputest UPSTREAM](sys/src/cmd/gputest/UPSTREAM),
[gputest LICENSE](sys/src/cmd/gputest/LICENSE),
[libgpu UPSTREAM](sys/src/libgpu/UPSTREAM), [libgpu LICENSE](sys/src/libgpu/LICENSE).

The Utah teapot uses Martin Newell's model from
[freeglut](https://github.com/freeglut/freeglut) at
`69ac8824f003dd813ceb14b11fb4efaa38cb4aa9`.
[teapot.h](sys/src/cmd/gputest/teapot.h) and [mesh.c](sys/src/cmd/gputest/mesh.c)
retain freeglut and Pawel W. Olszta's notices. The local UPSTREAM records
the data's provenance through Philip Schneider and Juhana Kouhia.

## TinyGL and classic gears

[TinyGL 0.4.1](https://bellard.org/TinyGL/) by Fabrice Bellard supplies the
API, matrix, lighting and display-list code. Archive SHA-256:
`b314f019a778d583a3778cb14bcc2a57626a8f866cbed0693705bb1bb785b16f`.
See [LICENSE](sys/src/libtinygl/LICENSE) and [UPSTREAM](sys/src/libtinygl/UPSTREAM).
The header retains Mesa's constants attribution; the VirGL backend retains
the Red Hat MIT notice above.

`glgears` adapts Brian Paul's public-domain `examples/gears.c` from that
archive. Attribution is in [glgears.c](sys/src/cmd/glgears/glgears.c);
port details are in [UPSTREAM](sys/src/cmd/glgears/UPSTREAM).

## vmx

The multicore work extends aiju's 9front monitor and driver, introduced in
[`389ba6bdd9885f7570d8f87ea14b3eba48c000f6`](https://github.com/9front/9front/commit/389ba6bdd9885f7570d8f87ea14b3eba48c000f6)
and [`789311ad540ba30820e51768f7bf029d32939bb6`](https://github.com/9front/9front/commit/789311ad540ba30820e51768f7bf029d32939bb6).
The [README](sys/src/cmd/vmx/README) records architecture references and tests.

The GPU bridge follows the definitions and notices in
[UPSTREAM.gpu](sys/src/cmd/vmx/UPSTREAM.gpu) and [LICENSE.gpu](sys/src/cmd/vmx/LICENSE.gpu).
The new sound device implements VirtIO 1.2 section 5.14; no sound driver
or emulator source was imported.

[virglrenderer-poll.patch](boot/virglrenderer-poll.patch) corrects host query
polling in virglrenderer 1.3.0 at `ca50e008863837e094747a69974dde3ae148aeaa`.
It preserves the Red Hat 2014 copyright and MIT notice. This library runs
only in the outer QEMU; build instructions are in [VOID.md](boot/VOID.md).

## QuickJS

[QuickJS 2026-06-04](https://bellard.org/quickjs/) by Fabrice Bellard and
Charlie Gordon, under [MIT](sys/src/cmd/quickjs/LICENSE). Archive SHA-256:
`b376e839b322978313d929fd20663b11ba58b75df5a46c126dd19ea2fa70ad2a`.
[UPSTREAM](sys/src/cmd/quickjs/UPSTREAM) records the APE/native compiler port.

[Anand Tamariya's Plan 9 port](https://gitlab.com/atamariya/qjs/-/tree/plan9)
at `e494138ed9f9e611fac8bb67e593a7f3c720df64` informed value constructors,
closure layout and heap frames. Its compatibility and graphics layers
were not imported.

Math routines come from [musl 1.2.5](https://musl.libc.org/releases.html),
archive SHA-256 `a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4`.
[COPYRIGHT](sys/src/cmd/quickjs/math/COPYRIGHT) and individual files retain
the MIT, Arm, Sun and FreeBSD notices. APE itself is unchanged.

## Eigenmath

[Eigenmath](https://github.com/georgeweigt/eigenmath) by George Weigt,
at `751f8d59dbc303fe527d84de948ce6035adc386c`, under
[BSD-2-Clause](sys/src/cmd/eigenmath/LICENSE).
Lewis Van Winkle's incomplete beta function retains its zlib notice.
Selected musl 1.2.5 routines retain [COPYRIGHT](sys/src/cmd/eigenmath/math/COPYRIGHT)
and Sun notices. Sources and adaptations: [UPSTREAM](sys/src/cmd/eigenmath/UPSTREAM).

## Optional Go environment

`boot/getgo` bootstraps unmodified [Go 1.27.2](https://go.dev/dl/#go1.27.2)
for `plan9/amd64`. Archive SHA-256:
`03495da2ba64894d40f5c4992e49454fa78b50690604ff92b6afff5081b76e62`.
The installed `/sys/lib/go` retains the Go Authors' BSD-3-Clause `LICENSE`,
`PATENTS` and component notices. Sources and binaries are not committed.

The optional CA file is curl's unmodified
[2026-09-25 Mozilla bundle](https://curl.se/ca/cacert-2026-09-25.pem), under
[MPL 2.0](https://www.mozilla.org/MPL/2.0/), with its header preserved.
SHA-256: `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.
An existing bundle is preserved; see [curl's source notes](https://curl.se/docs/caextract.html).

The native example uses itchyny's unmodified
[gojq v0.12.19](https://github.com/itchyny/gojq/tree/b7ebffbfc038677520df0bae4c8c2d877f88ffea),
commit `b7ebffbfc038677520df0bae4c8c2d877f88ffea`, under MIT.
Its `plan9/amd64` binary links these modules alongside the Go standard library:

| Module | Version | License |
| --- | --- | --- |
| github.com/clipperhouse/stringish | v0.1.1 | MIT; `utf8` retains the Go Authors' BSD-3-Clause notice |
| github.com/clipperhouse/uax29/v2 | v2.3.0 | MIT |
| github.com/itchyny/go-yaml | v0.0.0-20251001235044-fca9a0999f15 | Apache 2.0; libyaml-derived files retain MIT notices |
| github.com/itchyny/timefmt-go | v0.1.8 | MIT |
| github.com/mattn/go-isatty | v0.0.20 | MIT |
| github.com/mattn/go-runewidth | v0.0.19 | MIT |

Tests use `github.com/google/go-cmp` v0.7.0 (BSD-3-Clause).
`golang.org/x/sys` v0.38.0 is declared but not linked on Plan 9.
The [build recipe](boot/README.md#go-and-gojq) saves `go version -m` and
archives sources and notices under `/usr/glenda/lib/gojq`, outside the
volatile cache. Preserve those notices when redistributing binaries.

## Neverball and Neverputt

[Neverball 1.6.0](https://neverball.org/neverball-1.6.0.tar.gz) by Robert Kooima
and contributors supplies both games and shared C modules, under GPL-2.0-or-later.
See [UPSTREAM](sys/src/games/neverball/UPSTREAM),
[LICENSE.md](sys/src/games/neverball/LICENSE.md) and
[license-GPL-2.0.txt](sys/src/games/neverball/license-GPL-2.0.txt).

`boot/getneverball` (`getneverputt` is an alias) installs assets separately,
including all `doc/legal` notices and the DejaVu/WenQuanYi font notices.
Downloads, compiled maps and decoded audio are not committed.
Native image, font and audio libraries retain their original licenses.

## raylib

[raylib 6.0](https://github.com/raysan5/raylib/tree/dbc56a87da87d973a9c5baa4e7438a9d20121d28)
at `dbc56a87da87d973a9c5baa4e7438a9d20121d28`, by Ramon Santamaria and contributors,
under [zlib/libpng](sys/src/libraylib/LICENSE). The source digest and port
changes are in [UPSTREAM](sys/src/libraylib/UPSTREAM).
Bundled codecs and utilities retain their notices in
[external](sys/src/libraylib/external).

## raylib games

Arkanoid, Asteroids and Missile Commander come from
[raylib-games](https://github.com/raysan5/raylib-games/tree/318688c759eaf391b36b9f893355c038fcf56419/classics)
at `318688c759eaf391b36b9f893355c038fcf56419`.
Arkanoid and Missile Commander are by Marc Palau and Ramon Santamaria;
Asteroids is by Ian Eito, Albert Martos and Ramon Santamaria.
The zlib/libpng [LICENSE](sys/src/games/raygames/LICENSE) and source headers
are preserved; [UPSTREAM](sys/src/games/raygames/UPSTREAM) records digests
and adaptations. No additional assets are bundled.

## raygui and model viewer

[raygui 5.0](https://github.com/raysan5/raygui/tree/020a61bebcbe288b4414de3416e219ef40af847a)
at `020a61bebcbe288b4414de3416e219ef40af847a`, by Ramon Santamaria and contributors,
retains its [zlib license](sys/src/libraygui/LICENSE) and header notices.
[UPSTREAM](sys/src/libraygui/UPSTREAM) records native changes and the
color-picker fix adapted from `9cb5cfa73290ba920d071f57c1d06a4daee22f52`.
The binding and modelview client are new code; no model assets are bundled.
