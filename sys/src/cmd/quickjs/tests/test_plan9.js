/* generated9 integration tests; see the repository LICENSE. */
import * as std from "std";
import * as os from "os";

function assert(actual, expected) {
    if (arguments.length == 1)
        expected = true;
    if (!Object.is(actual, expected))
        throw Error(`got ${actual}, expected ${expected}`);
}

function test_numbers() {
    assert(NaN === NaN, false);
    assert(NaN == 0, false);
    assert(NaN < 0, false);
    assert(NaN <= 0, false);
    assert(NaN > 0, false);
    assert(NaN >= 0, false);
    assert(NaN !== NaN);
    assert(Number.MIN_VALUE, 2 ** -1074);
    assert(Number.NaN, NaN);
    assert(Number.POSITIVE_INFINITY, Infinity);
    assert(Number.NEGATIVE_INFINITY, -Infinity);
    assert(1 / -0, -Infinity);
    assert(-(0.5 * 0), -0);
    assert(Math.atanh(-0), -0);
    assert(Math.tanh(-0), -0);
    assert(Math.ceil(-0.5), -0);
    assert(Math.round(-0.5), -0);
    assert((-0) ** 3, -0);
    assert(Math.atan2(-Number.MIN_VALUE, Number.MAX_VALUE), -0);
    assert(Math.expm1(1e-20), 1e-20);
    assert(Math.log1p(1e-20), 1e-20);
    assert(Math.trunc(-0.5), -0);
    assert(Math.cbrt(-8), -2);
    assert(Math.acosh(1), 0);
    assert(Math.acosh(0), NaN);
    assert(Math.asinh(-0), -0);
    assert(Math.atanh(1), Infinity);
    assert(Math.atanh(2), NaN);
    assert(Math.atanh(Infinity), NaN);
    assert(Math.tanh(NaN), NaN);
    assert(Math.hypot(NaN, 0), NaN);
    assert(Math.hypot(NaN, Infinity), Infinity);
    assert(Math.hypot(Infinity, Infinity), Infinity);
    assert(Math.acos(0), Math.PI / 2);
    assert(Math.acos(-1), Math.PI);
    assert(Math.asin(1), Math.PI / 2);
    assert(Math.atan(Infinity), Math.PI / 2);
    assert((-2) ** 0.5, NaN);
    assert(0 ** -1, Infinity);
    assert(Math.exp(-744) > 0);
    assert(Math.log(-1), NaN);
    assert(Math.log(Number.MIN_VALUE) < -744);
    assert(Math.asin(2), NaN);
    assert(Math.acos(2), NaN);
    assert(Math.sqrt(-1), NaN);
    assert(Math.sin(Infinity), NaN);
    assert(Math.cos(Infinity), NaN);
    assert(Math.atan2(0, -0), Math.PI);
    assert(Math.abs(Math.sin(1e100) - -0.3806377310050287) < 1e-15);
}

function test_files() {
    const dir = "/tmp/quickjs-" + os.getpid();
    const path = dir + "/data";
    assert(os.mkdir(dir, 0o700), 0);
    try {
        const data = { greeting: "ciao, città — 世界", answer: 42 };
        const f = std.open(path, "w");
        assert(f !== null);
        f.puts(JSON.stringify(data));
        assert(f.close(), 0);
        assert(JSON.parse(std.loadFile(path)).greeting, data.greeting);
        assert(os.readdir(dir)[0].includes("data"));
        assert(os.realpath(path)[0], path);
        assert(os.utimes(path, 10000, 10000), 0);
        const [st, err] = os.stat(path);
        assert(err, 0);
        assert(st.mtime, 10000);
        assert(st.mode & os.S_IFMT, os.S_IFREG);

        const fd = os.open(path, os.O_RDWR | os.O_TRUNC);
        assert(fd >= 0);
        try {
            const a = new Uint8Array([0, 1, 127, 128, 255]);
            const b = new Uint8Array(a.length);
            assert(os.write(fd, a.buffer, 0, a.length), a.length);
            assert(os.seek(fd, 0n, std.SEEK_SET), 0n);
            assert(os.read(fd, b.buffer, 0, b.length), b.length);
            assert(b.toString(), a.toString());
        } finally {
            os.close(fd);
        }
    } finally {
        os.remove(path);
        os.remove(dir);
    }
}

function test_processes() {
    const name = "QUICKJS_PLAN9_TEST";
    std.setenv(name, "first");
    assert(std.getenv(name), "first");
    std.setenv(name, "second");
    assert(std.getenviron()[name], "second");
    assert(os.exec(["/bin/rc", "-c", "exit 7"]), 7);

    const [r, w] = os.pipe();
    const pid = os.exec(["/bin/rc", "-c", "echo $" + name], {
        stdout: w,
        block: false,
    });
    assert(pid > 0);
    os.close(w);
    const f = std.fdopen(r, "r");
    assert(f.getline(), "second");
    assert(f.getline(), null);
    f.close();
    const [waited, status] = os.waitpid(pid, 0);
    assert(waited, pid);
    assert(status, 0);
    std.unsetenv(name);
    assert(std.getenv(name), undefined);
}

async function test_events() {
    for (const fd of [-1, 96]) {
        let rejected = false;
        try { os.setReadHandler(fd, () => {}); }
        catch (e) { rejected = e instanceof RangeError; }
        assert(rejected);
    }
    const timeout = os.setTimeout(() => std.exit(1), 5000);
    try {
        await Promise.all(Array.from({ length: 8 }, (_, i) => new Promise(resolve => {
            const [r, w] = os.pipe();
            os.setReadHandler(r, () => {
                const data = new Uint32Array(1);
                const n = os.read(r, data.buffer, 0, 4);
                os.setReadHandler(r, null);
                os.close(r);
                assert(n, 4);
                assert(data[0], i);
                resolve();
            });
            os.setWriteHandler(w, () => {
                const data = new Uint32Array([i]);
                assert(os.write(w, data.buffer, 0, 4), 4);
                os.setWriteHandler(w, null);
                os.close(w);
            });
        })));
        const start = Date.now();
        await os.sleepAsync(20);
        assert(Date.now() >= start + 20);
        await new Promise(resolve => {
            const [r, w] = os.pipe();
            os.setReadHandler(r, () => {
                const data = new Uint8Array(1);
                assert(os.read(r, data.buffer, 0, 1), 0);
                os.setReadHandler(r, null);
                os.close(r);
                resolve();
            });
            os.close(w);
        });
        await new Promise(resolve => {
            const [r, w] = os.pipe();
            os.setReadHandler(r, () => {
                os.setReadHandler(r, null);
                resolve();
            });
            os.close(r);
            os.close(w);
        });
    } finally {
        os.clearTimeout(timeout);
    }
}

assert(os.platform, "plan9");
test_numbers();
test_files();
test_processes();
await test_events();
print("Plan 9 integration tests passed");
