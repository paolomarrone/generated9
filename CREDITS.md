# Credits and source provenance

## VirtIO GPU and VirGL

The native GPU transport implements the [VirtIO 1.2 GPU protocol](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html).
Its wire layouts also follow the BSD-licensed `virtio_gpu.h` by Dave Airlie
and Gerd Hoffmann, Copyright Red Hat, Inc. 2013-2014, from Linux v6.12,
revision `adc218676eef25575469234709c2d87185ca223a`. The original notice is
preserved in [devgpu.c](sys/src/9/pc/devgpu.c). The transport reuses 9front's
VirtIO register access and follows its existing PCI and split queue idioms.

The native triangle and command encoder adapt the MIT-licensed tests and
protocol definitions in virglrenderer, Copyright Red Hat 2014-2015,
revision `4d8bd3c9954a9599e0de70a2e432c761da80398d`.
The original [license](sys/src/cmd/gputest/LICENSE) and exact source paths in
[UPSTREAM](sys/src/cmd/gputest/UPSTREAM) accompany the example.
New generated9 driver, device tests and integration code use the root
[LICENSE](LICENSE); the original notices continue to apply to adapted portions.

The Utah teapot demo uses Martin Newell's model as distributed by
[freeglut](https://github.com/freeglut/freeglut), revision
`69ac8824f003dd813ceb14b11fb4efaa38cb4aa9`. The data and native patch evaluator
retain the freeglut and Pawel W. Olszta permission notices in
[teapot.h](sys/src/cmd/gputest/teapot.h) and [mesh.c](sys/src/cmd/gputest/mesh.c).
The local [UPSTREAM](sys/src/cmd/gputest/UPSTREAM) records the exact source
files and the data's provenance through Philip Schneider and Juhana Kouhia.

## Eigenmath

`sys/src/cmd/eigenmath` imports [Eigenmath](https://github.com/georgeweigt/eigenmath),
by George Weigt, at revision `751f8d59dbc303fe527d84de948ce6035adc386c`,
under its preserved [BSD-2-Clause license](sys/src/cmd/eigenmath/LICENSE).
Its incomplete beta function by Lewis Van Winkle retains the original
copyright and zlib license in `src/eval_incbeta.c`.

The native Plan 9 build uses selected routines from musl 1.2.5, with source
and adaptation details in [UPSTREAM](sys/src/cmd/eigenmath/UPSTREAM).
Their [COPYRIGHT](sys/src/cmd/eigenmath/math/COPYRIGHT), including the MIT
license, and the Sun notices in individual files are preserved.
New generated9 integration code uses the [root license](LICENSE).

## QuickJS

`sys/src/cmd/quickjs` imports [QuickJS 2026-06-04](https://bellard.org/quickjs/),
by Fabrice Bellard and Charlie Gordon, under its preserved
[MIT license](sys/src/cmd/quickjs/LICENSE). The archive SHA-256 is
`b376e839b322978313d929fd20663b11ba58b75df5a46c126dd19ea2fa70ad2a`.
The native APE build and compiler adaptations are documented in
[UPSTREAM](sys/src/cmd/quickjs/UPSTREAM).

[Anand Tamariya's Plan 9 port](https://gitlab.com/atamariya/qjs/-/tree/plan9),
revision `e494138ed9f9e611fac8bb67e593a7f3c720df64`, informed the value
constructors, closure layout and heap-frame approach. Its compatibility and
graphical/browser layers were not imported.

The accompanying double-precision math routines come from
[musl 1.2.5](https://musl.libc.org/releases.html), archive SHA-256
`a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4`.
They retain musl's [COPYRIGHT](sys/src/cmd/quickjs/math/COPYRIGHT), including
the MIT license, and the original Arm, Sun and FreeBSD notices in individual
files. The local adaptations preserve the numerical algorithms and supply
the floating-point behavior needed by JavaScript. APE itself is unchanged.

## Plan 9 and 9front

generated9 derives from Plan 9 from Bell Labs and the work of the 9front
authors. The base checkout is the `front` branch of
[`9front/9front`](https://github.com/9front/9front), at
[`70233ea4a4fe9b905ea8f3a4c4e514a7df5d8b8b`](https://github.com/9front/9front/commit/70233ea4a4fe9b905ea8f3a4c4e514a7df5d8b8b).
The original Git history, authorship, and per-file notices are preserved.

The upstream [licensing notice](lib/legal/NOTICE) and [MIT license](lib/legal/mit)
remain unchanged, including the documented exceptions for fonts, Ghostscript,
and separately licensed components. Local runtime configuration is seeded from
9front's existing `sys/lib/dist` templates.

The amd64 compiler's handling of negated floating-point comparisons follows
Richard Miller's `5c-nan-cmp` design, integrated into 9front by cinap_lenrek in
commit [`0d7e3fd4a27a8494d8e513b43cac12b7d9b9dea6`](https://github.com/9front/9front/commit/0d7e3fd4a27a8494d8e513b43cac12b7d9b9dea6).
The adaptation uses the same `BTRUE` flag, with amd64-specific comparison and
branch selection. The original code remains under the upstream licenses above;
new generated9 additions use the [root license](LICENSE).

## vmx

The multicore work extends 9front's existing vmx monitor and kernel driver,
introduced by aiju in commits
[`389ba6bdd9885f7570d8f87ea14b3eba48c000f6`](https://github.com/9front/9front/commit/389ba6bdd9885f7570d8f87ea14b3eba48c000f6)
and [`789311ad540ba30820e51768f7bf029d32939bb6`](https://github.com/9front/9front/commit/789311ad540ba30820e51768f7bf029d32939bb6).
Their authorship, subsequent upstream history and licenses are preserved.
The new multiprocessor support, APIC emulation and tests use the
[root license](LICENSE). Architecture references and validation are recorded
in [sys/src/cmd/vmx/README](sys/src/cmd/vmx/README).

## Russ Cox's diskless boot workflow

`boot/qemu` adapts the workflow from
[`rsc/plan9/boot/qemu`](https://github.com/rsc/plan9/blob/13bd0c334646e584ddecc1b7b106c0183b164273/boot/qemu),
revision `13bd0c334646e584ddecc1b7b106c0183b164273`: QEMU guest forwarding starts
u9fs to serve the host checkout as the guest's root filesystem.
Russ Cox is credited for this integration. The adapted script uses 9front's
multiboot support and adds configuration, headless operation, and validation.

The repository-level MIT license accompanying that source is preserved
verbatim in [boot/LICENSE.rsc](boot/LICENSE.rsc), with its original Plan 9
Foundation copyright notice. The other new boot helpers were written for
generated9; their license is in [LICENSE](LICENSE).

The current [9legacy boot scripts](https://github.com/0intro/9legacy/tree/main/boot)
were also consulted as a reference for bootstrap preparation and QEMU options.
They were not imported into this tree.

## u9fs

The 34 original files under `sys/src/cmd/unix/u9fs` were imported byte for byte
from [rsc/plan9 at the same pinned revision](https://github.com/rsc/plan9/tree/13bd0c334646e584ddecc1b7b106c0183b164273/sys/src/cmd/unix/u9fs).
The local `UPSTREAM` file records provenance. The original license, reproduced
below for the supporting documentation, remains in
[sys/src/cmd/unix/u9fs/LICENSE](sys/src/cmd/unix/u9fs/LICENSE).

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

`boot/getbin` retrieves the official `9front-11952.amd64.iso.gz` image and checks
its SHA-256 against the [9front release announcement](https://9front.org/releases/2026/08/02/0/).
The downloaded ISO and extracted runtime are not part of our Git commits.
Their original distribution licenses continue to apply.

## Optional Go environment

`boot/getgo` downloads the unmodified
[`go1.27.2.src.tar.gz`](https://go.dev/dl/#go1.27.2) release from the Go project
and invokes its `src/bootstrap.bash` for `plan9/amd64`. The Go Authors retain
their original copyrights. The installed `/sys/lib/go` tree includes the
upstream BSD 3-clause `LICENSE`, `PATENTS`, and component-specific notices.
The pinned archive SHA-256 is
`03495da2ba64894d40f5c4992e49454fa78b50690604ff92b6afff5081b76e62`.
The downloaded source and generated toolchain are not committed to this tree.

The optional `/sys/lib/tls/ca.pem` is the unmodified
[2026-09-25 Mozilla CA bundle](https://curl.se/ca/cacert-2026-09-25.pem)
converted and distributed by the curl project. Its SHA-256 is
`a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.
It retains the original header and source reference, and is licensed under
[Mozilla Public License 2.0](https://www.mozilla.org/MPL/2.0/), as documented
by [curl's CA extraction service](https://curl.se/docs/caextract.html).
The bootstrap preserves an existing CA bundle.

The native build example uses itchyny's
[`gojq` v0.12.19](https://github.com/itchyny/gojq/tree/b7ebffbfc038677520df0bae4c8c2d877f88ffea),
commit `b7ebffbfc038677520df0bae4c8c2d877f88ffea`, under its original MIT license.
No source modifications are applied. Its `plan9/amd64` binary links these
modules in addition to Go's standard library:

| Module | Version | License |
| --- | --- | --- |
| github.com/clipperhouse/stringish | v0.1.1 | MIT; `utf8` retains the Go Authors' BSD 3-clause notice |
| github.com/clipperhouse/uax29/v2 | v2.3.0 | MIT |
| github.com/itchyny/go-yaml | v0.0.0-20251001235044-fca9a0999f15 | Apache 2.0; libyaml-derived files retain MIT notices |
| github.com/itchyny/timefmt-go | v0.1.8 | MIT |
| github.com/mattn/go-isatty | v0.0.20 | MIT |
| github.com/mattn/go-runewidth | v0.0.19 | MIT |

The upstream tests also use `github.com/google/go-cmp` v0.7.0, under its
BSD 3-clause license. `golang.org/x/sys` v0.38.0 is declared by gojq but is
not linked into this Plan 9 binary. Module versions and checksums are recorded
by `go version -m`; the recipe in [boot/README.md](boot/README.md) saves that
output and archives the downloaded GitHub sources with all original license
and notice files under `/usr/glenda/lib/gojq`. These local build artifacts
are ignored by Git. Preserve their notices when distributing the binary.
