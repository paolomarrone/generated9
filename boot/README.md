# 9front from the checkout

This adapts Russ Cox's [diskless QEMU workflow](https://github.com/rsc/plan9)
to 9front: the guest's writable root is this Git checkout, exported by u9fs
over 9P. Host edits are visible in the guest, and guest builds write back here.

On a Linux host, install QEMU (`qemu-system-x86_64` with user networking), a C
compiler, make, Python 3, curl, and 7-Zip (`7z` or `7zz`). No root access or
host network configuration is required.

```sh
./boot/getbin            # download and extract the amd64 bootstrap runtime
./boot/qemu              # serial console; quit with Ctrl-a x
./boot/qemu -vga         # graphical rio desktop, plus serial console
./boot/test              # boot, native C build, and file sharing smoke test
```

`boot/getbin` uses the pinned `9front-11952.amd64.iso.gz` image and checks the
SHA-256 published in the [release announcement](https://9front.org/releases/2026/08/02/0/).
Alternatively,
pass a local 9front amd64 `.iso` or `.iso.gz`. It installs `amd64/bin`,
`amd64/lib`, `amd64/init`, `amd64/9pc64`, and `acme/bin/amd64`; it does not
replace source files or headers. Running it again replaces those bootstrap
binaries, including locally rebuilt ones. These runtime files and the download
cache are ignored by Git. The ISO is a bootstrap baseline, not a build of the
current checkout.

`boot/mkdirs` creates directories absent from Git and seeds missing local
configuration from `sys/lib/dist`. It preserves existing files. The initial
Glenda profile is copied from `boot/profile`; customize `usr/glenda/lib/profile`
afterward. It starts an interactive rc in text mode and the distribution's rio
profile in graphics mode.

The stock amd64 kernel already supports multiboot, so QEMU loads it directly
and passes `boot/plan9.ini` as its first module. We do not need the Bell Labs
PXE disk, TFTP, or a guest disk image. Change that ini file for the next boot;
display and console settings are supplied by `boot/qemu`.
QEMU's `multiboot knows VBE` message and the `no frame buffer` message during
a text-only boot are harmless; the graphics mode initializes its screen later.

```sh
./boot/qemu -m 4G -smp 4
./boot/qemu -tcg                 # software emulation, without /dev/kvm
./boot/test --tcg
./boot/qemu -kernel sys/src/9/pc64/9pc64
./boot/qemu -ini boot/my-plan9.ini
```

KVM is selected when `/dev/kvm` is readable and writable. Arguments following
`--` go directly to QEMU. Kernel and ini paths are relative to the checkout
(absolute paths also work), regardless of the launch directory.

Inside 9front, build the kernel with:

```rc
cd /sys/src/9/pc64
mk
```

Then quit QEMU and use the `-kernel` command above to boot that result. To build
the userland, run `cd /sys/src` followed by `mk install` inside 9front.

QEMU invokes `boot/u9fs` for its guest-only address `10.0.2.1:564`; there is no
9P listening port on the host. Like Russ's setup, this is an unauthenticated,
writable development root mapped to the invoking host user. Guest changes
affect this checkout, including its Git metadata. Multiple VMs can use it, but
concurrent builds into the same output files need coordination.
The server log is `boot/cache/u9fs.log`. In the guest, `/tmp` is bound to
`/usr/glenda/tmp`, so its files appear under `usr/glenda/tmp` on the host.

The u9fs source in `sys/src/cmd/unix/u9fs` was imported unchanged from
[`rsc/plan9` at `13bd0c334646e584ddecc1b7b106c0183b164273`](https://github.com/rsc/plan9/tree/13bd0c334646e584ddecc1b7b106c0183b164273/sys/src/cmd/unix/u9fs).
Its original authors and license are preserved in that directory's `LICENSE`.
Complete source attribution and the u9fs permission notice are also collected
in [CREDITS.md](../CREDITS.md). The upstream MIT notice for Russ's boot script
is in [LICENSE.rsc](LICENSE.rsc); our additions use the [root license](../LICENSE).
