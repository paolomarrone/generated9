import * as r from 'raylib';
function assert(v) { if (!v) throw Error('window check failed'); }
function throws(f) {
    let caught = false;
    try { f(); } catch(e) { caught = true; }
    assert(caught);
}
const v = (x,y,z) => ({x,y,z});
const cam = {position:v(3,3,3),target:v(0,0,0),up:v(0,1,0),fovy:45,projection:r.CAMERA_PERSPECTIVE};
throws(() => r.BeginDrawing());
throws(() => r.InitWindow(0,200,'bad'));
r.InitWindow(640,480,'rayjs test');
throws(() => r.InitWindow(100,100,'twice'));
r.SetTargetFPS(0);
throws(() => r.LoadTexture('/does-not-exist.png'));
throws(() => r.LoadModel('/does-not-exist.obj'));
throws(() => r.GenImageChecked(8,8,0,1,r.RED,r.BLUE));
let image = r.GenImageColor(16,16,r.RED);
let texture = r.LoadTextureFromImage(image);
assert(texture.width === 16 && texture.height === 16);
r.UnloadImage(image); r.UnloadImage(image);
throws(() => r.LoadTextureFromImage(image));
throws(() => r.UnloadSound(texture));
const model = r.LoadModel('/sys/games/lib/gemrun/gem.obj');
for (let i=0;i<4;i++) {
    r.BeginDrawing();
    throws(() => r.BeginDrawing());
    throws(() => r.CloseWindow());
    r.ClearBackground(r.DARKBLUE);
    throws(() => r.DrawCircle(0,0,Infinity,r.WHITE));
    r.DrawRectangle(10,10,50,50,r.GREEN);
    r.DrawTextureEx(texture,{x:80,y:10},0,2,r.WHITE);
    r.BeginMode3D(cam);
    throws(() => r.EndDrawing());
    throws(() => r.BeginMode3D(cam));
    r.DrawModel(model,v(0,0,0),1,r.GOLD);
    r.DrawSphere(v(1.2,0,0),0.6,r.MAGENTA);
    r.EndMode3D();
    r.DrawText('JavaScript on generated9',20,400,20,r.WHITE);
    r.EndDrawing();
}
const pixels = r.LoadImageFromScreen();
assert(r.ExportImage(pixels,'/tmp/rayjs-pixels.png'));
assert(!r.ExportImage(pixels,'/does-not-exist/pixels.png'));
r.UnloadImage(pixels);
// Argument getters may unload resources or change drawing state.
r.BeginDrawing();
throws(() => r.DrawTextureEx(texture,{get x(){r.UnloadTexture(texture);return 0;},y:0},0,1,r.WHITE));
r.EndDrawing();
r.UnloadTexture(texture);
r.UnloadModel(model);
r.CloseWindow();
throws(() => r.BeginDrawing());
// Closing a window invalidates its resources, including across a reopen.
r.InitWindow(320,240,'rayjs reopen');
image = r.GenImageColor(8,8,r.BLUE);
texture = r.LoadTextureFromImage(image);
r.CloseWindow();
r.InitWindow(320,240,'rayjs reopen again');
r.BeginDrawing();
throws(() => r.DrawTextureEx(texture,{x:0,y:0},0,1,r.WHITE));
throws(() => r.ClearBackground({get r(){r.EndDrawing();r.CloseWindow();return 0;},g:0,b:0,a:255}));
// Leave image and old texture to the runtime finalizers.
print('rayjs: window passed');
