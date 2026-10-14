# Boot from the checkout

The guest's writable root is this checkout, served over 9P by u9fs.
Host edits and guest builds affect the same files. The workflow is adapted
from [Russ Cox's Plan 9 tree](https://github.com/rsc/plan9).

The Linux host needs Bash, QEMU (`qemu-system-x86_64` with user networking),
a C compiler, make, Python 3, curl and 7-Zip (`7z` or `7zz`). Root access
and host network configuration are not required.

```sh
./boot/getbin            # install the amd64 bootstrap runtime
./boot/qemu              # serial console; quit with Ctrl-a x
./boot/qemu -vga         # rio desktop and serial console
./boot/test              # boot, native C build and file sharing test
```

`getbin` verifies and extracts the pinned `9front-11952.amd64.iso.gz`.
It also accepts a local amd64 `.iso` or `.iso.gz`. It replaces binaries in
`amd64/bin`, `amd64/lib`, `amd64/init`, `amd64/9pc64` and `acme/bin/amd64`,
including local rebuilds, but preserves source and headers. These files
and the download cache are ignored by Git. The bootstrap predates our changes.

`boot/mkdirs` creates missing directories and seeds missing configuration
from `sys/lib/dist`. The initial Glenda profile comes from `boot/profile`;
edit `usr/glenda/lib/profile` to customize it. Existing files are preserved.

## Build and boot

QEMU loads the multiboot kernel and `boot/plan9.ini` directly. No guest disk
image is needed. Console and display settings come from `boot/qemu`.

```sh
./boot/qemu -m 4G -smp 4
./boot/qemu -tcg                 # without KVM
./boot/test --tcg
./boot/qemu -kernel sys/src/9/pc64/9pc64
./boot/qemu -ini boot/my-plan9.ini
```

The defaults are 2 GB and two CPUs. KVM is used when `/dev/kvm` is accessible;
`host,migratable=off` exposes invariant TSC where available. Arguments after
`--` go to QEMU. Relative kernel and ini paths start at the checkout root.
The VBE and missing-framebuffer messages during text boot are harmless.

Build the kernel inside generated9:

```rc
cd /sys/src/9/pc64
mk
```

Restart QEMU with `-kernel sys/src/9/pc64/9pc64` to use it. To rebuild and
install userland, run `cd /sys/src` and `mk install` inside generated9.

## Native programs

QuickJS and Eigenmath need the amd64 compiler's NaN comparison fix.
Rebuild `6c` before building either port, then clean any old port objects:

```rc
cd /sys/src/cmd/6c
mk
mk test
mk install
```

Build instructions:

- [QuickJS](../sys/src/cmd/quickjs/README.Plan9): JavaScript, APE and a PPM ray tracer.
- [Eigenmath](../sys/src/cmd/eigenmath/README.Plan9): symbolic mathematics.
- [GPU.md](GPU.md): native graphics, libraries and games.
- [VOID.md](VOID.md): Void Linux with Xfce inside `vmx`.

## Go and gojq

The optional bootstrap needs bzip2, Python 3.12 or later and Go 1.24.6 or
later on the Linux host:

```sh
./boot/getgo
./boot/qemu -m 4G
```

`getgo` verifies the Go 1.27.2 source archive and runs `src/bootstrap.bash`
for `plan9/amd64`. It installs sources, tools and licenses in `/sys/lib/go`.
Existing installations are preserved; move one aside to rebuild it.
If absent, `/sys/lib/tls/ca.pem` is seeded with curl's pinned Mozilla bundle.
Downloads and installed files are ignored by Git.

Inside generated9:

```rc
. /root/boot/goenv
go version
go install github.com/itchyny/gojq/cmd/gojq@v0.12.19
echo '{"system":"generated9","cpus":2}' | gojq -c .
```

`/root/boot` is the checkout's boot directory; `/boot` holds kernel bootstrap
programs. `goenv` binds Go into `/bin` and installs programs in
`$home/bin/$cputype`. It uses `GOTOOLCHAIN=local`; cgo is unavailable.
Go and gojq are unmodified. HTTPS and module checksum verification stay enabled.

Go's cache locks require exclusive-use files, which u9fs lacks. `goenv`
therefore mounts ramfs at `/mnt/go` for module, build and temporary files.
Repeated sourcing reuses it. Caches consume guest RAM and are lost at shutdown;
source projects and installed programs remain in the checkout.

Before shutdown, save gojq's sources and license notices outside ramfs:

```rc
mkdir -p $home/lib/gojq
go version -m $GOBIN/gojq > $home/lib/gojq/buildinfo
@{cd $GOMODCACHE && tar cf $home/lib/gojq/modules.tar github.com}
```

The archive includes all GitHub modules downloaded in that session.
Keep their `LICENSE` and `NOTICE` files with redistributed binaries.
See [CREDITS.md](../CREDITS.md) for versions and licenses.

With Go 1.27.2, gojq's `TestCliRun/stream_option_with_unterminated_input`
fails on both Plan 9 and Linux because the expected JSON error text differs.
The remaining gojq tests and Go's `lockedfile` tests pass; run the latter
with `TMPDIR=/mnt/go/tmp`.

## Shared filesystem

QEMU starts `boot/u9fs` for guest address `10.0.2.1:564`; no host port listens.
The export is unauthenticated and writable as the invoking host user.
Guest writes can change the entire checkout, including Git metadata.
Coordinate concurrent builds when sharing it between VMs.

The server log is `boot/cache/u9fs.log`. Guest `/tmp` is bound to
`/usr/glenda/tmp`, visible on the host as `usr/glenda/tmp`.
Source revisions and permission notices are in [CREDITS.md](../CREDITS.md).
