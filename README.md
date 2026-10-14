# generated9

A development branch of [9front](https://9front.org/), with diskless QEMU boot
and a writable checkout shared between the host and the guest through 9P.
The boot workflow is adapted from [Russ Cox's Plan 9 tree](https://github.com/rsc/plan9).

The working branch is `gen`. The repository preserves the complete history of
the cloned `9front/9front` `front` branch, starting our changes at
`70233ea4a4fe9b905ea8f3a4c4e514a7df5d8b8b` (2026-10-10).

```sh
git clone --branch gen https://github.com/paolomarrone/generated9.git
cd generated9
./boot/getbin
./boot/qemu              # serial console; quit with Ctrl-a x
./boot/qemu -vga         # graphical rio desktop
./boot/test              # native build and shared filesystem smoke test
```

Host requirements, bootstrap details, and kernel development instructions are
in [boot/README.md](boot/README.md). Guest writes modify the checkout directly.
Downloaded binaries, ISO images, and mutable guest state are not committed.

The GitHub repository is an independent copy. Upstream remains
[`9front/9front`](https://github.com/9front/9front), the project's GitHub mirror.
For a new clone, add the upstream remote with:

```sh
git remote add upstream https://github.com/9front/9front.git
git fetch upstream
```

See [CREDITS.md](CREDITS.md) for exact source revisions and attribution, and
[LICENSE](LICENSE) for the license of our additions and the locations of the
original component licenses. Original copyright and license notices are retained.
