#include <u.h>
#include <libc.h>
#include <keyboard.h>
#include <raylib.h>
#define RAYMATH_STATIC_INLINE
#include <raymath.h>

enum { W=160, H=120 };
static char dir[128];
static char *original;
static int keyboardfd;

static void
windowerrors(void)
{
	if(bind("/dev/zero", "/dev/wctl", MREPL) < 0) sysfatal("bind window control: %r");
	MinimizeWindow();
	if(IsWindowMinimized()) sysfatal("failed minimize changed window state");
	SetWindowState(FLAG_WINDOW_HIDDEN);
	if(IsWindowHidden()) sysfatal("failed hide changed window state");
	if(unmount(nil, "/dev/wctl") < 0) sysfatal("restore window control: %r");
	MinimizeWindow();
	if(!IsWindowMinimized()) sysfatal("minimize window");
	if(bind("/dev/zero", "/dev/wctl", MREPL) < 0) sysfatal("bind window control: %r");
	RestoreWindow();
	ClearWindowState(FLAG_WINDOW_HIDDEN);
	if(!IsWindowMinimized() || !IsWindowHidden()) sysfatal("failed restore changed window state");
	if(unmount(nil, "/dev/wctl") < 0) sysfatal("restore window control: %r");
	RestoreWindow();
	if(IsWindowMinimized() || IsWindowHidden()) sysfatal("restore window");
	print("PASS: window state follows successful control requests\n");
}

static void
characters(void)
{
	Rune input[] = {'A', 0xe8, KF|1, '\t', 0xf900, 0x10400, 0x1f600};
	int expected[] = {'A', 0xe8, 0xf900, 0x10400, 0x1f600};
	char buf[64], *p;
	int i, n, c;

	p = buf;
	for(i=0; i<nelem(input); i++){
		*p++ = 'c';
		p += runetochar(p, &input[i]);
		*p++ = 0;
	}
	if(write(keyboardfd, buf, p-buf) != p-buf) sysfatal("write keyboard: %r");
	n = 0;
	for(i=0; i<100; i++){
		sleep(10);
		PollInputEvents();
		while((c=GetCharPressed()) != 0){
			if(n == nelem(expected) || c != expected[n++]) sysfatal("unexpected character U+%ux", c);
		}
		if(n == nelem(expected)) break;
	}
	if(n != nelem(expected)) sysfatal("missing Unicode characters: %d", n);
	print("PASS: Unicode text input excludes control and function keys\n");
}

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
le32(uchar *p, ulong n)
{
	p[0] = n; p[1] = n>>8; p[2] = n>>16; p[3] = n>>24;
}

static void
savevox(char *name, uchar *data, int size)
{
	uchar *file, *palette;

	file = mallocz(size+12+1024,1);
	if(file == nil) sysfatal("voxel fixture allocation");
	memmove(file,data,size);
	le32(file+16,size+12+1024-20);
	palette = file+size;
	memmove(palette,"RGBA",4);
	le32(palette+4,1024);
	palette[12] = 0x12; palette[13] = 0x34; palette[14] = 0x56; palette[15] = 255;
	if(!SaveFileData(name,file,size+12+1024)) sysfatal("write voxel fixture");
	free(file);
}

/* Minimal authored wire fixtures: one voxel, and one triangle with a root bone. */
static void
loaders(void)
{
	static uchar voxel[] = {
		0x56,0x4f,0x58,0x20,0x96,0x00,0x00,0x00,0x4d,0x41,0x49,0x4e,0x00,0x00,0x00,0x00,
		0x2c,0x00,0x00,0x00,0x53,0x49,0x5a,0x45,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x58,0x59,0x5a,0x49,
		0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
	};
	static uchar bone8[] = {
		0x33,0x44,0x4d,0x4f,0x5a,0x00,0x00,0x00,0x48,0x45,0x41,0x44,0x1d,0x00,0x00,0x00,
		0x00,0x00,0x80,0x3f,0xc0,0xc3,0x3f,0x00,0x74,0x65,0x73,0x74,0x00,0x00,0x00,0x00,
		0x72,0x6f,0x6f,0x74,0x00,0x56,0x52,0x54,0x53,0x18,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x7f,0x00,0x00,0x00,0x00,0x7f,0x00,0x00,0x00,0x00,0x00,0x7f,0x42,0x4f,0x4e,
		0x45,0x0d,0x00,0x00,0x00,0x01,0xff,0x08,0x00,0x03,0x4d,0x45,0x53,0x48,0x0c,0x00,
		0x00,0x00,0x30,0x00,0x01,0x02,0x4f,0x4d,0x44,0x33,
	};
	static uchar bone16[] = {
		0x33,0x44,0x4d,0x4f,0x5c,0x00,0x00,0x00,0x48,0x45,0x41,0x44,0x1d,0x00,0x00,0x00,
		0x00,0x00,0x80,0x3f,0xc0,0xc7,0x3f,0x00,0x74,0x65,0x73,0x74,0x00,0x00,0x00,0x00,
		0x72,0x6f,0x6f,0x74,0x00,0x56,0x52,0x54,0x53,0x18,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x7f,0x00,0x00,0x00,0x00,0x7f,0x00,0x00,0x00,0x00,0x00,0x7f,0x42,0x4f,0x4e,
		0x45,0x0f,0x00,0x00,0x00,0x01,0x00,0xff,0xff,0x08,0x00,0x03,0x4d,0x45,0x53,0x48,
		0x0c,0x00,0x00,0x00,0x30,0x00,0x01,0x02,0x4f,0x4d,0x44,0x33,
	};
	Model model;
	Mesh mesh;
	BoundingBox box;
	float low[3];
	uchar *many;
	int i, j, pass, total;
	float n;

	savevox("one.vox",voxel,sizeof voxel);
	model = LoadModel("one.vox");
	if(!IsModelValid(model) || model.meshCount != 1) sysfatal("load voxel fixture");
	mesh = model.meshes[0];
	if(mesh.vertexCount != 24 || mesh.triangleCount != 12) sysfatal("voxel geometry");
	box = GetModelBoundingBox(model);
	low[0] = box.min.x; low[1] = box.min.y; low[2] = box.min.z;
	if(fabs(box.max.x-box.min.x-.25) > 1e-6 || fabs(box.max.y-box.min.y-.25) > 1e-6
	|| fabs(box.max.z-box.min.z-.25) > 1e-6) sysfatal("voxel bounds");
	for(i=0; i<mesh.vertexCount; i++){
		n = 0;
		for(j=0; j<3; j++){
			if(fabs(mesh.vertices[3*i+j]-low[j]) > 1e-6 && fabs(mesh.vertices[3*i+j]-low[j]-.25) > 1e-6)
				sysfatal("voxel vertex stride");
			n += mesh.normals[3*i+j]*mesh.normals[3*i+j];
		}
		if(n != 1 || mesh.colors[4*i] != 0x12 || mesh.colors[4*i+1] != 0x34
		|| mesh.colors[4*i+2] != 0x56 || mesh.colors[4*i+3] != 255)
			sysfatal("voxel normal/color stride");
	}
	UnloadModel(model);
	/* Disconnected voxels cross the loader's 16-bit mesh index boundary. */
	many = malloc(60+4096*4);
	if(many == nil) sysfatal("allocate voxel fixture");
	memmove(many,voxel,60);
	le32(many+16,40+4096*4);
	le32(many+32,32); le32(many+36,32); le32(many+40,32);
	le32(many+48,4+4096*4); le32(many+56,4096);
	for(i=0; i<4096; i++){
		many[60+4*i] = 2*(i%16);
		many[61+4*i] = 2*(i/16%16);
		many[62+4*i] = 2*(i/256);
		many[63+4*i] = 1;
	}
	savevox("many.vox",many,60+4096*4);
	free(many);
	model = LoadModel("many.vox");
	if(!IsModelValid(model) || model.meshCount != 2) sysfatal("split voxel meshes");
	total = 0;
	for(i=0; i<model.meshCount; i++){
		mesh = model.meshes[i];
		total += mesh.vertexCount;
		for(j=0; j<mesh.triangleCount*3; j++)
			if(mesh.indices[j] >= mesh.vertexCount || mesh.indices[j]/4 != j/6)
				sysfatal("voxel mesh index rebasing");
	}
	if(total != 4096*24) sysfatal("voxel mesh split lost vertices");
	UnloadModel(model);
	for(pass=0; pass<2; pass++){
		if(!SaveFileData("root.m3d",pass ? bone16 : bone8,pass ? sizeof bone16 : sizeof bone8))
			sysfatal("write skeleton fixture");
		model = LoadModel("root.m3d");
		if(!IsModelValid(model) || model.skeleton.boneCount != 2
		|| model.skeleton.bones[0].parent != -1 || model.skeleton.bindPose[0].rotation.w != 1)
			sysfatal("M3D %d-bit root sentinel",pass ? 16 : 8);
		UnloadModel(model);
	}
	print("PASS: VOX layout and mesh splitting, M3D 8/16-bit root bone indices\n");
}

static void
skinning(void)
{
	Model model;
	Mesh *mesh;
	ModelAnimation anim = {0};
	Transform pose[2], *poses[2];
	int i;

	model = LoadModelFromMesh(GenMeshCube(1,1,1));
	mesh = &model.meshes[0];
	mesh->boneCount = model.skeleton.boneCount = 1;
	model.skeleton.bones = calloc(1,sizeof(BoneInfo));
	model.skeleton.bones[0].parent = -1;
	model.skeleton.bindPose = calloc(1,sizeof(Transform));
	model.skeleton.bindPose[0].rotation.w = 1;
	model.skeleton.bindPose[0].scale = (Vector3){1,1,1};
	model.currentPose = calloc(1,sizeof(Transform));
	model.boneMatrices = calloc(1,sizeof(Matrix));
	mesh->boneIndices = calloc(mesh->vertexCount,4);
	mesh->boneWeights = calloc(mesh->vertexCount,4*sizeof(float));
	mesh->animVertices = calloc(mesh->vertexCount,3*sizeof(float));
	mesh->animNormals = calloc(mesh->vertexCount,3*sizeof(float));
	for(i=0; i<mesh->vertexCount; i++) mesh->boneWeights[4*i] = 1;
	pose[0] = pose[1] = model.skeleton.bindPose[0];
	pose[1].translation.x = 2;
	poses[0] = &pose[0]; poses[1] = &pose[1];
	anim.boneCount = 1;
	anim.keyframeCount = 2;
	anim.keyframePoses = poses;
	UpdateModelAnimation(model,anim,.5);
	for(i=0; i<mesh->vertexCount; i++){
		if(fabs(mesh->animVertices[3*i]-mesh->vertices[3*i]-1) > .0001
		|| fabs(mesh->animVertices[3*i+1]-mesh->vertices[3*i+1]) > .0001
		|| fabs(mesh->animVertices[3*i+2]-mesh->vertices[3*i+2]) > .0001)
			sysfatal("classic CPU skinning interpolation");
	}
	UpdateModelAnimation(model,anim,0);
	if(memcmp(mesh->animVertices,mesh->vertices,mesh->vertexCount*3*sizeof(float)))
		sysfatal("classic CPU skinning bind pose");
	UnloadModel(model);
	print("PASS: CPU skinning and animation interpolation without GPU buffers\n");
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
		if(pass == 0){
			loaders();
			skinning();
			windowerrors();
			characters();
		}
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
	char path[32];
	int fd, withaudio, kbd[2];

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
	if(rfork(RFNAMEG) < 0 || pipe(kbd) < 0) sysfatal("keyboard namespace: %r");
	snprint(path, sizeof path, "/fd/%d", kbd[0]);
	if(bind(path, "/dev/kbd", MREPL) < 0) sysfatal("bind test keyboard: %r");
	keyboardfd = kbd[1];
	images();
	files();
	graphics();
	if(withaudio) audio();
	exits(nil);
}
