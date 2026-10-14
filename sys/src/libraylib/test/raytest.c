#include <u.h>
#include <libc.h>
#include <raylib.h>
#define RAYMATH_STATIC_INLINE
#include <raymath.h>

enum { W=160, H=120 };
static char dir[128];
static char *original;

static void
cleanup(void)
{
	FilePathList files;
	int i;

	chdir(original);
	files = LoadDirectoryFiles(dir);
	for(i=0; i<files.count; i++) remove(files.paths[i]);
	UnloadDirectoryFiles(files);
	remove(dir);
}

static void
images(void)
{
	Image image, decoded, reduced;
	Color *p;
	char *names[] = {"image.png", "image.bmp", "image.qoi"};
	int i, x, y;

	if(sizeof(Color) != 4 || sizeof(Vector3) != 12)
		sysfatal("packed color/vector layout");
	image = GenImageChecked(32,24,8,8,RED,BLUE);
	p = image.data;
	for(y=0; y<24; y++) for(x=0; x<32; x++)
		if(!ColorIsEqual(p[y*32+x], (x/8+y/8)%2 ? BLUE : RED))
			sysfatal("generated image pixel %d,%d",x,y);
	for(i=0; i<nelem(names); i++){
		if(!ExportImage(image,names[i])) sysfatal("export %s",names[i]);
		decoded = LoadImage(names[i]);
		ImageFormat(&decoded,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		if(decoded.width != 32 || decoded.height != 24 || memcmp(decoded.data,image.data,32*24*4))
			sysfatal("image roundtrip %s",names[i]);
		UnloadImage(decoded);
	}
	reduced = ImageCopy(image);
	ImageResize(&reduced,16,12);
	if(reduced.width != 16 || reduced.height != 12) sysfatal("image resize");
	UnloadImage(reduced);
	UnloadImage(image);
	print("PASS: packed components, procedural images, PNG/BMP/QOI roundtrips and resize\n");
}

static void
files(void)
{
	FilePathList list;
	char *text;
	int n;

	if(!SaveFileText("text","hello\n")) sysfatal("save text");
	if(FileRename("text","renamed") != 0) sysfatal("rename file");
	text = LoadFileText("renamed");
	if(text == nil || strcmp(text,"hello\n")) sysfatal("load text");
	UnloadFileText(text);
	if(!FileExists("renamed") || !IsPathFile("renamed") || !DirectoryExists("."))
		sysfatal("file queries");
	list = LoadDirectoryFilesEx(".",".png;.bmp;.qoi",false);
	n = list.count;
	UnloadDirectoryFiles(list);
	if(n != 3) sysfatal("directory filtering: %d entries",n);
	if(GetApplicationDirectory()[0] != '/') sysfatal("application directory");
	print("PASS: native file queries, directory filtering and application path\n");
}

static void
graphics(void)
{
	Image image, readback;
	Texture texture;
	Color patch[4] = {GREEN,GREEN,GREEN,GREEN};
	Camera3D camera = {{4,3,4},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
	Model model;
	int i, pass;

	for(pass=0; pass<2; pass++){
		InitWindow(W,H,"raylib tests");
		if(!IsWindowReady()) sysfatal("init window");
		image = GenImageChecked(16,16,8,8,RED,BLUE);
		texture = LoadTextureFromImage(image);
		UnloadImage(image);
		UpdateTextureRec(texture,(Rectangle){0,0,2,2},patch);
		readback = LoadImageFromTexture(texture);
		if(!ColorIsEqual(GetImageColor(readback,0,0),GREEN)
		|| !ColorIsEqual(GetImageColor(readback,8,0),BLUE)) sysfatal("texture update/readback");
		UnloadImage(readback);
		model = LoadModelFromMesh(GenMeshSphere(1,8,8));
		if(!IsModelValid(model) || !IsMaterialValid(model.materials[0]))
			sysfatal("classic model/material validity");
		for(i=0; i<3; i++){
			BeginDrawing();
			ClearBackground(BLACK);
			BeginMode3D(camera);
			DrawModel(model,(Vector3){0,0,0},1,WHITE);
			EndMode3D();
			BeginScissorMode(4,8,8,8);
			DrawRectangle(0,0,W,H,RED);
			EndScissorMode();
			EndDrawing();
		}
		readback = LoadImageFromScreen();
		if(!ColorIsEqual(GetImageColor(readback,6,10),RED)
		|| !ColorIsEqual(GetImageColor(readback,2,10),BLACK)) sysfatal("screen scissor/readback");
		UnloadImage(readback);
		UnloadModel(model);
		UnloadTexture(texture);
		CloseWindow();
	}
	print("PASS: model rendering, dynamic textures, screen capture and window/context recreation\n");
}

static void
audio(void)
{
	short pcm[22050];
	Wave wave = {22050,22050,16,1,pcm}, decoded;
	Sound sound;
	Music music;
	float paused;
	int i;

	for(i=0; i<nelem(pcm); i++) pcm[i] = 6000*sin(2*PI*440*i/22050);
	if(!ExportWave(wave,"tone.wav")) sysfatal("export wave");
	decoded = LoadWave("tone.wav");
	if(decoded.frameCount != 22050 || decoded.sampleRate != 22050 || decoded.sampleSize != 16
	|| memcmp(decoded.data,pcm,sizeof pcm)) sysfatal("WAV roundtrip");
	InitAudioDevice();
	if(!IsAudioDeviceReady()) sysfatal("audio device unavailable");
	sound = LoadSoundFromWave(decoded);
	UnloadWave(decoded);
	if(!IsSoundValid(sound)) sysfatal("sound conversion");
	PlaySound(sound);
	for(i=0; IsSoundPlaying(sound) && i<300; i++) sleep(10);
	if(i == 300) sysfatal("sound did not finish");
	UnloadSound(sound);
	music = LoadMusicStream("tone.wav");
	if(!IsMusicValid(music)) sysfatal("WAV stream");
	PlayMusicStream(music);
	for(i=0; i<300; i++){
		UpdateMusicStream(music);
		sleep(10);
		if(GetMusicTimePlayed(music) > 0) break;
	}
	if(i == 300) sysfatal("stream did not advance");
	PauseMusicStream(music);
	paused = GetMusicTimePlayed(music);
	sleep(50);
	if(IsMusicStreamPlaying(music) || GetMusicTimePlayed(music) != paused)
		sysfatal("paused stream advanced");
	ResumeMusicStream(music);
	if(!IsMusicStreamPlaying(music)) sysfatal("resume stream");
	StopMusicStream(music);
	UnloadMusicStream(music);
	CloseAudioDevice();
	InitAudioDevice();
	if(!IsAudioDeviceReady()) sysfatal("reopen audio device");
	CloseAudioDevice();
	print("PASS: WAV roundtrip, mono resampling, sound, stream pause/resume and device recreation\n");
}

void
main(int argc, char **argv)
{
	int fd, withaudio;

	withaudio = 0;
	ARGBEGIN{
	case 'a': withaudio = 1; break;
	default: sysfatal("usage: raytest [-a]");
	}ARGEND
	if(argc) sysfatal("usage: raytest [-a]");
	original = strdup(GetWorkingDirectory());
	snprint(dir,sizeof dir,"/tmp/raytest.%d",getpid());
	if(access(dir,AEXIST) == 0) sysfatal("test directory already exists");
	fd = create(dir,OREAD,DMDIR|0700);
	if(fd < 0) sysfatal("create test directory: %r");
	close(fd);
	atexit(cleanup);
	if(!ChangeDirectory(dir)) sysfatal("change directory");
	SetTraceLogLevel(LOG_WARNING);
	images();
	files();
	graphics();
	if(withaudio) audio();
	exits(nil);
}
