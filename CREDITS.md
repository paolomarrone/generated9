# Credits and source provenance

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
