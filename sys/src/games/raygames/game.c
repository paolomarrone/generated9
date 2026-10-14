#include "game.h"

static void
usage(void)
{
	fprint(2,"usage: %s [-n frames] [-s seed] [-o image.png]\n",argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	Camera2D camera = {0};
	Image image;
	char *out, *arg, *end;
	ulong seed;
	int limit, frames, w, h, x, y;
	float scale;

	out = nil;
	limit = frames = 0;
	seed = time(nil);
	ARGBEGIN{
	case 'n':
		limit = strtol(EARGF(usage()),&end,10);
		if(limit <= 0 || *end) usage();
		break;
	case 's':
		arg = EARGF(usage());
		seed = strtoul(arg,&end,0);
		if(end == arg || *end) usage();
		break;
	case 'o': out = EARGF(usage()); break;
	default: usage();
	}ARGEND
	if(argc) usage();
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(screenWidth,screenHeight,gameTitle);
	if(!IsWindowReady()) sysfatal("initialize game window");
	SetRandomSeed(seed);
	SetTargetFPS(60);
	InitGame();
	while(!WindowShouldClose()){
		w = GetScreenWidth();
		h = GetScreenHeight();
		scale = fminf((float)w/screenWidth,(float)h/screenHeight);
		x = (w-screenWidth*scale)/2;
		y = (h-screenHeight*scale)/2;
		camera.offset = (Vector2){x,y};
		camera.zoom = scale;
		SetMouseOffset(-x,-y);
		SetMouseScale(1/scale,1/scale);
		if(IsKeyPressed(KEY_R)) InitGame();
		if(UpdateGame() || limit != 0) DisableEventWaiting();
		else EnableEventWaiting();
		BeginDrawing();
		ClearBackground(BLACK);
		BeginScissorMode(x,y,screenWidth*scale,screenHeight*scale);
		BeginMode2D(camera);
		DrawRectangle(0,0,screenWidth,screenHeight,RAYWHITE);
		DrawGame();
		EndMode2D();
		EndScissorMode();
		EndDrawing();
		if(++frames == limit) break;
	}
	if(out != nil){
		image = LoadImageFromScreen();
		if(!ExportImage(image,out)) sysfatal("save game image %s",out);
		UnloadImage(image);
	}
	CloseWindow();
	exits(nil);
}
