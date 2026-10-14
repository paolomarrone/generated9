#include <u.h>
#include <libc.h>
#include <raylib.h>
#include <raymath.h>

static void
usage(void)
{
	fprint(2, "usage: games/raydemo [-a] [-n frames]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Camera3D camera = {{5,4,5},{0,1,0},{0,1,0},45,CAMERA_PERSPECTIVE};
	Vector3 axis = {0,1,0};
	Image image;
	Texture texture;
	Model model;
	Sound sound = {0};
	Wave wave = {0};
	short pcm[11025];
	float angle, radius, dt;
	int audio, paused, frames, limit, i;

	audio = paused = frames = limit = 0;
	ARGBEGIN{
	case 'a': audio = 1; break;
	case 'n': limit = atoi(EARGF(usage())); if(limit <= 0) usage(); break;
	default: usage();
	}ARGEND
	if(argc) usage();
	InitWindow(800,600,"raylib on generated9");
	if(!IsWindowReady()) sysfatal("initialize raylib window");
	SetTargetFPS(60);
	image = GenImageChecked(128,128,16,16,SKYBLUE,RAYWHITE);
	texture = LoadTextureFromImage(image);
	UnloadImage(image);
	model = LoadModelFromMesh(GenMeshCube(2,2,2));
	model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	if(audio){
		InitAudioDevice();
		audio = IsAudioDeviceReady();
		if(audio){
			for(i=0; i<nelem(pcm); i++)
				pcm[i] = 6000*sin(2*PI*440*i/44100)*(1-(float)i/nelem(pcm));
			wave = (Wave){nelem(pcm),44100,16,1,pcm};
			sound = LoadSoundFromWave(wave);
		}
	}
	angle = 0;
	radius = 6;
	while(!WindowShouldClose()){
		dt = GetFrameTime();
		if(IsKeyPressed(KEY_SPACE)){
			paused = !paused;
			if(audio) PlaySound(sound);
		}
		if(!paused) angle += dt*.5;
		if(IsKeyDown(KEY_LEFT)) angle -= dt;
		if(IsKeyDown(KEY_RIGHT)) angle += dt;
		radius = Clamp(radius-GetMouseWheelMove()*.5,3,12);
		camera.position = Vector3RotateByAxisAngle((Vector3){radius,4,0},axis,angle);
		BeginDrawing();
		ClearBackground(RAYWHITE);
		BeginMode3D(camera);
		DrawGrid(10,1);
		DrawModel(model,(Vector3){0,1,0},1,WHITE);
		EndMode3D();
		if(GetScreenWidth() > 40){
			BeginScissorMode(20,20,GetScreenWidth()-40,100);
			DrawRectangle(20,20,GetScreenWidth()-40,100,DARKBLUE);
			DrawText("raylib / TinyGL / VirGL",35,35,24,WHITE);
			DrawText("Space: pause   arrows: orbit   wheel: zoom",35,78,18,SKYBLUE);
			EndScissorMode();
		}
		DrawFPS(20,GetScreenHeight()-35);
		EndDrawing();
		if(++frames == limit) break;
	}
	if(audio){
		UnloadSound(sound);
		CloseAudioDevice();
	}
	UnloadModel(model);
	UnloadTexture(texture);
	CloseWindow();
	exits(nil);
}
