import * as r from 'raylib';
function assert(v) { if (!v) throw Error('audio check failed'); }
function throws(f) { let caught=false; try {f();} catch(e) {caught=true;} assert(caught); }
const pcm = new Int16Array(4410);
for (let i=0;i<pcm.length;i++) pcm[i] = 2000*Math.sin(2*Math.PI*440*i/44100);
const wave = {frameCount:pcm.length,sampleRate:44100,sampleSize:16,channels:1,data:pcm.buffer};
throws(() => r.LoadSoundFromWave(wave));
r.InitAudioDevice();
assert(r.IsAudioDeviceReady());
throws(() => r.InitAudioDevice());
throws(() => r.LoadSoundFromWave({...wave,frameCount:10}));
let sound = r.LoadSoundFromWave(wave);
r.PlaySound(sound);
const start = r.GetTime();
while (r.IsSoundPlaying(sound) && r.GetTime() - start < 2) {}
assert(!r.IsSoundPlaying(sound));
r.SetSoundVolume(sound,0.5);
r.StopSound(sound);
assert(!r.IsSoundPlaying(sound));
r.UnloadSound(sound); r.UnloadSound(sound);
throws(() => r.PlaySound(sound));
sound = r.LoadSoundFromWave(wave);
r.CloseAudioDevice();
throws(() => r.PlaySound(sound));
r.InitAudioDevice();
sound = r.LoadSoundFromWave(wave);
r.PlaySound(sound);
// Leave a live sound and device to automatic cleanup.
print('rayjs: audio passed');
