/* generated9 ray tracer; see the repository root LICENSE. */
import * as std from "std";

function add(a, b) {
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function sub(a, b) {
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function scale(a, k) {
    return [a[0] * k, a[1] * k, a[2] * k];
}

function dot(a, b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

function unit(a) {
    return scale(a, 1 / Math.sqrt(dot(a, a)));
}

function mix(a, b, k) {
    return add(scale(a, 1 - k), scale(b, k));
}

function reflect(d, n) {
    return sub(d, scale(n, 2 * dot(d, n)));
}

const spheres = [
    { p: [-1.45, 0, 0], r: 1, color: [0.8, 0.13, 0.07], mirror: 0.18 },
    { p: [0.35, -0.3, 1], r: 0.7, color: [0.04, 0.55, 0.48], mirror: 0.3 },
    { p: [1.4, 0, -1.1], r: 1, color: [0.7, 0.65, 0.5], mirror: 0.75 },
];
const floor = { mirror: 0.15 };
const light = [-3, 6, 4];
const epsilon = 0.0001;

/* Directions are unit vectors; limit also bounds shadow rays. */
function hit(o, d, limit = Infinity) {
    let t = limit, shape = null;
    for (const s of spheres) {
        const v = sub(o, s.p);
        const b = dot(v, d);
        const disc = b * b - dot(v, v) + s.r * s.r;
        if (disc < 0)
            continue;
        const root = Math.sqrt(disc);
        let near = -b - root;
        if (near <= epsilon)
            near = -b + root;
        if (near > epsilon && near < t) {
            t = near;
            shape = s;
        }
    }
    if (Math.abs(d[1]) > 1e-8) {
        const near = (-1 - o[1]) / d[1];
        if (near > epsilon && near < t) {
            t = near;
            shape = floor;
        }
    }
    if (shape === null)
        return null;
    const p = add(o, scale(d, t));
    if (shape === floor) {
        const odd = (Math.floor(p[0]) + Math.floor(p[2])) & 1;
        return { p, n: [0, 1, 0], mirror: floor.mirror,
                 color: odd ? [0.12, 0.16, 0.2] : [0.75, 0.72, 0.65] };
    }
    return { p, n: unit(sub(p, shape.p)),
             color: shape.color, mirror: shape.mirror };
}

function trace(o, d, depth) {
    const h = hit(o, d);
    if (h === null)
        return mix([0.7, 0.8, 0.95], [0.12, 0.25, 0.45], (d[1] + 1) / 2);

    /* Start secondary rays above the surface to avoid self-intersection. */
    const start = add(h.p, scale(h.n, epsilon));
    const tolight = sub(light, start);
    const distance = Math.sqrt(dot(tolight, tolight));
    const l = scale(tolight, 1 / distance);
    const diffuse = Math.max(0, dot(h.n, l));
    let color = scale(h.color, 0.12);
    if (diffuse > 0 && hit(start, l, distance) === null) {
        color = add(color, scale(h.color, 0.88 * diffuse));
        const specular = Math.pow(Math.max(0, dot(reflect(scale(l, -1), h.n),
                                                 scale(d, -1))), 64);
        color = add(color, scale([1, 1, 1], 0.4 * specular));
    }
    if (depth > 0 && h.mirror > 0)
        color = mix(color, trace(start, reflect(d, h.n), depth - 1), h.mirror);
    return color;
}

let width = 480, height = 300;
const args = scriptArgs.slice(1);
if (args.length === 2) {
    width = Number(args[0]);
    height = Number(args[1]);
}
if ((args.length !== 0 && args.length !== 2) ||
    !Number.isSafeInteger(width) || width <= 0 ||
    !Number.isSafeInteger(height) || height <= 0) {
    std.err.puts("usage: qjs raytrace.js [width height] (positive integers)\n");
    std.exit(1);
}

const eye = [0, 1.1, 6];
const forward = unit([0, -0.16, -1]);
const up = [0, -forward[2], forward[1]];
const row = new Uint8Array(width * 3);
std.out.puts(`P6\n${width} ${height}\n255\n`);
for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
        let color = [0, 0, 0];
        /* Four fixed samples per pixel keep edges smooth and runs repeatable. */
        for (let sy = 0; sy < 2; sy++) {
            for (let sx = 0; sx < 2; sx++) {
                const u = (2 * (x + (sx + 0.5) / 2) - width) / height;
                const v = 1 - 2 * (y + (sy + 0.5) / 2) / height;
                const d = unit(add(scale(forward, 1.8), add([u, 0, 0], scale(up, v))));
                color = add(color, trace(eye, d, 3));
            }
        }
        for (let c = 0; c < 3; c++) {
            const linear = Math.max(0, Math.min(1, color[c] / 4));
            const srgb = linear <= 0.0031308 ? 12.92 * linear :
                         1.055 * Math.pow(linear, 1 / 2.4) - 0.055;
            row[3 * x + c] = Math.round(255 * srgb);
        }
    }
    if (std.out.write(row.buffer, 0, row.length) !== row.length)
        throw Error("writing PPM output");
}
std.out.flush();
if (std.out.error())
    throw Error("flushing PPM output");
