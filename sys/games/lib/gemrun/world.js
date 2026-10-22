// Game rules are independent of the display and clock. See the root LICENSE.
export class World {
    constructor(level = 1) {
        this.level = level;
        this.time = this.score = this.invulnerable = 0;
        this.health = 3;
        this.won = false;
        this.player = {x: 0, z: 4.8};
        this.blocks = [{x:-2,z:-2}, {x:2,z:-2}, {x:-2,z:2}, {x:2,z:2}];
        this.gems = [[-4,-4],[0,-4],[4,-4],[-4,0],[0,0],[4,0],[-4,4],[4,4]]
            .map(([x,z]) => ({x,z,taken:false}));
        this.enemies = [{x:-4,z:-1}, {x:4,z:1}, {x:0,z:-4}];
    }
}

export function move(x, z) {
    const scale = Math.hypot(x, z) || 1;
    return {x:(x + z) / (scale * Math.SQRT2), z:(z - x) / (scale * Math.SQRT2)};
}

export function step(w, direction, dt) {
    if (!w.health || w.won) return '';
    w.time += dt;
    w.invulnerable = Math.max(0, w.invulnerable - dt);
    const p = w.player;
    function blocked(x, z) {
        return Math.abs(x) > 5.5 || Math.abs(z) > 5.5 ||
            w.blocks.some(b => Math.abs(x - b.x) < 0.9 && Math.abs(z - b.z) < 0.9);
    }
    const x = p.x + direction.x * 4.5 * dt;
    if (!blocked(x, p.z)) p.x = x;
    const z = p.z + direction.z * 4.5 * dt;
    if (!blocked(p.x, z)) p.z = z;
    const t = w.time * (0.65 + 0.1 * Math.min(w.level, 12));
    w.enemies[0] = {x:4.7 * Math.sin(t), z:-1};
    w.enemies[1] = {x:4.7 * Math.sin(t + Math.PI), z:1};
    w.enemies[2] = {x:0, z:4.5 * Math.sin(t * 0.7 - Math.PI / 2)};
    let event = '';
    if (!w.invulnerable && w.enemies.some(e => Math.hypot(p.x-e.x,p.z-e.z) < 0.66)) {
        w.health--;
        w.invulnerable = 1.8;
        event = 'hit';
    }
    if (w.health) for (const g of w.gems) {
        if (!g.taken && Math.hypot(p.x-g.x,p.z-g.z) < 0.65) {
            g.taken = true;
            w.score++;
            if (!event) event = 'gem';
        }
    }
    w.won = w.score === w.gems.length;
    return event;
}
