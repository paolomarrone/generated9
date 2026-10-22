// Gemrun: a small raylib game for generated9. See the root LICENSE.
import * as r from 'raylib';
import {World, step, move} from './world.js';

let frames = 0, limit = 0, capture = '', audio = false;
for (let i = 1; i < scriptArgs.length; i++) {
    switch (scriptArgs[i]) {
    case '-a': audio = true; break;
    case '-n': limit = Number(scriptArgs[++i]);
        if (!Number.isInteger(limit) || limit < 1) throw Error('invalid frame count');
        break;
    case '-o': capture = scriptArgs[++i];
        if (!capture) throw Error('missing screenshot path');
        break;
    default: throw Error('usage: games/gemrun [-a] [-n frames] [-o screenshot.png]');
    }
}

const rgb = (r, g, b, a = 255) => ({r, g, b, a});
const v = (x, y, z) => ({x, y, z});
const ink = rgb(19, 25, 40), floor = rgb(37, 49, 68), edge = rgb(64, 82, 103);
const cyan = rgb(85, 224, 226), gold = rgb(255, 206, 99), red = rgb(242, 103, 112);
const camera = {position: v(12, 16, 16), target: v(0, 0, 0), up: v(0, 1, 0),
                fovy: 19, projection: r.CAMERA_ORTHOGRAPHIC};
const path = scriptArgs[0].slice(0, scriptArgs[0].lastIndexOf('/') + 1);
let world = new World(), paused = false;

function tone(hz) {
    const samples = new Int16Array(6615);
    for (let i = 0; i < samples.length; i++) {
        const t = i / 44100;
        samples[i] = 7000 * Math.sin(2 * Math.PI * hz * t) * (1 - i / samples.length);
    }
    return r.LoadSoundFromWave({frameCount: samples.length, sampleRate: 44100,
                               sampleSize: 16, channels: 1, data: samples.buffer});
}

r.InitWindow(960, 720, 'Gemrun');
r.SetTargetFPS(60);
const gem = r.LoadModel(path + 'gem.obj');
const image = r.GenImageChecked(32, 32, 8, 8, cyan, edge);
const badge = r.LoadTextureFromImage(image);
r.UnloadImage(image);
let pickup, hit;
if (audio) {
    r.InitAudioDevice();
    pickup = tone(880); hit = tone(180);
}
const start = r.GetTime();
while (!r.WindowShouldClose()) {
    const dt = Math.min(r.GetFrameTime(), 0.05);
    if (r.IsKeyPressed(r.KEY_R)) {world = new World(); paused = false;}
    if (r.IsKeyPressed(r.KEY_P)) paused = !paused;
    if (r.IsKeyPressed(r.KEY_ENTER) && world.won) world = new World(world.level + 1);
    if (!paused && world.health && !world.won) {
        let x = Number(r.IsKeyDown(r.KEY_D) || r.IsKeyDown(r.KEY_RIGHT)) -
                Number(r.IsKeyDown(r.KEY_A) || r.IsKeyDown(r.KEY_LEFT));
        let z = Number(r.IsKeyDown(r.KEY_S) || r.IsKeyDown(r.KEY_DOWN)) -
                Number(r.IsKeyDown(r.KEY_W) || r.IsKeyDown(r.KEY_UP));
        // Align the two movement axes with the isometric camera.
        const event = step(world, move(x, z), dt);
        if (audio && event) r.PlaySound(event === 'hit' ? hit : pickup);
    }
    const width = r.GetScreenWidth(), height = r.GetScreenHeight();
    camera.fovy = 19 * Math.max(1, height / width);
    r.BeginDrawing();
    r.ClearBackground(ink);
    r.BeginMode3D(camera);
    r.DrawCube(v(0, -0.4, 0), 12, 0.6, 12, edge);
    r.DrawPlane(v(0, -0.09, 0), {x: 11.8, y: 11.8}, floor);
    for (let i = -5; i <= 5; i++) {
        r.DrawCube(v(i, -0.07, 0), 0.018, 0.01, 11.6, edge);
        r.DrawCube(v(0, -0.07, i), 11.6, 0.01, 0.018, edge);
    }
    for (const b of world.blocks) {
        r.DrawCube(v(b.x, 0.45, b.z), 1.2, 1, 1.2, edge);
        r.DrawCubeWires(v(b.x, 0.45, b.z), 1.2, 1, 1.2, cyan);
    }
    for (const g of world.gems) {
        if (g.taken) continue;
        const y = 0.65 + 0.15 * Math.sin(world.time * 3 + g.x + g.z);
        r.DrawModel(gem, v(g.x, y, g.z), 0.55, gold);
    }
    for (const e of world.enemies) {
        r.DrawSphere(v(e.x, 0.32, e.z), 0.34, red);
        r.DrawCube(v(e.x, 0.72, e.z), 0.12, 0.12, 0.12, r.WHITE);
    }
    const player = world.player;
    if (world.health && (world.invulnerable <= 0 || Math.floor(world.time * 12) % 2)) {
        r.DrawSphere(v(player.x, 0.32, player.z), 0.32, cyan);
        r.DrawSphere(v(player.x, 0.57, player.z), 0.12, r.WHITE);
    }
    r.EndMode3D();
    r.DrawTextureEx(badge, {x: 24, y: 24}, 0, 1, r.WHITE);
    r.DrawText('GEMRUN', 68, 23, 30, cyan);
    r.DrawText(`LEVEL ${world.level}    GEMS ${world.score} / ${world.gems.length}`, 24, 70, 20, gold);
    r.DrawText(`LIVES ${world.health}`, Math.max(24, width - 130), 30, 20, red);
    r.DrawText('WASD / arrows: move    P: pause    R: restart', 24, height - 54, 16, r.LIGHTGRAY);
    r.DrawText('Collect the gold gems. Avoid the red sentinels.', 24, height - 30, 14, cyan);
    r.DrawFPS(Math.max(24, width - 110), height - 30);
    let message = paused ? 'PAUSED' : !world.health ? 'CAUGHT - R TO RESTART' :
                  world.won ? 'ALL GEMS FOUND - ENTER FOR NEXT LEVEL' : '';
    if (message) {
        r.DrawRectangle(16, Math.floor(height / 2 - 36), width - 32, 72, rgb(19,25,40,235));
        r.DrawText(message, 30, Math.floor(height / 2 - 10), width < 700 ? 16 : 24, gold);
    }
    if (!limit && (paused || !world.health || world.won)) r.EnableEventWaiting();
    else r.DisableEventWaiting();
    r.EndDrawing();
    if (++frames === limit) break;
}
if (limit) print('Gemrun:', frames, 'frames in', (r.GetTime() - start).toFixed(3), 'seconds');
if (capture) {
    const image = r.LoadImageFromScreen();
    try { if (!r.ExportImage(image, capture)) throw Error('cannot write ' + capture); }
    finally { r.UnloadImage(image); }
}
if (audio) r.CloseAudioDevice();
r.CloseWindow();
