import {World, move, step} from '/sys/games/lib/gemrun/world.js';
function assert(v) { if (!v) throw Error('game check failed'); }
let w = new World();
assert(Math.abs(Math.hypot(...Object.values(move(1,1)))-1) < 1e-12);
w.player = {x:5.49,z:0};
step(w,{x:1,z:0},0.05);
assert(w.player.x < 5.5);
w.player = {x:-3,z:-2};
step(w,{x:1,z:0},0.05);
assert(w.player.x === -3);
w.player = {x:-4,z:-4};
assert(step(w,{x:0,z:0},0) === 'gem' && w.score === 1);
step(w,{x:0,z:0},0);
assert(w.score === 1);
w.player = {x:0,z:-1};
assert(step(w,{x:0,z:0},0) === 'hit' && w.health === 2);
step(w,{x:0,z:0},0);
assert(w.health === 2);
for (const g of w.gems) {
    w.player = {x:g.x,z:g.z};
    step(w,{x:0,z:0},0);
}
assert(w.won);
w = new World();
w.health=0;
step(w,{x:1,z:0},1);
assert(w.player.x === 0 && w.time === 0);
print('rayjs: game passed');
