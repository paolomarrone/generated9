#include <u.h>
#include <libc.h>
#include <raylib.h>
#include "pinball.h"

enum { Width = 920, Height = 900, Nsounds = 7 };

static Color ink = {9,17,29,255}, bed = {14,30,44,255};
static Color dim = {45,68,83,255}, muted = {126,154,168,255};
static Color white = {226,242,239,255}, teal = {67,225,193,255};
static Color amber = {255,199,92,255}, coral = {255,112,109,255};
static Game game;
static Sound sounds[Nsounds];

static Vector2
vec(V p)
{
	return (Vector2){p.x,p.y};
}

static void
capsule(V a, V b, double r, Color c)
{
	DrawLineEx(vec(a),vec(b),2*r,c);
	DrawCircleV(vec(a),r,c);
	DrawCircleV(vec(b),r,c);
}

static void
centre(char *s, int x, int y, int size, Color c)
{
	DrawText(s,x-MeasureText(s,size)/2,y,size,c);
}

static void
lamp(V p, int on, Color c)
{
	DrawCircleV(vec(p),11,ink);
	DrawCircleV(vec(p),7,on ? c : dim);
	if(on)
		DrawCircle(p.x-2,p.y-2,2,white);
}

static void
board(void)
{
	Rail *r;
	V a, b;
	Color c;
	int i, x, y;

	ClearBackground(ink);
	DrawRectangleRounded((Rectangle){20,24,570,854},.08,12,dim);
	DrawRectangleRounded((Rectangle){27,31,556,840},.07,12,bed);
	for(i = 0; i < 120; i++){
		x = 55+(i*173)%465;
		y = 80+(i*271)%710;
		DrawCircle(x,y,i%7 == 0 ? 1.5 : .7,(Color){54,79,92,255});
	}
	DrawRing((Vector2){290,480},66,68,0,360,80,dim);
	DrawRing((Vector2){290,480},85,86,0,360,80,dim);
	DrawLineEx((Vector2){195,510},(Vector2){385,450},2,dim);
	centre("ORBIT",290,447,40,white);
	centre("9",290,489,40,teal);
	centre("DEEP SPACE PATROL",290,540,14,muted);
	/* The ramp is drawn from the same path that carries the ball. */
	a = ramppos(0);
	for(i = 1; i <= 72; i++){
		b = ramppos(i/72.0);
		capsule(a,b,13,(Color){26,49,62,255});
		a = b;
	}
	a = ramppos(0);
	for(i = 1; i <= 72; i++){
		b = ramppos(i/72.0);
		DrawLineEx(vec(a),vec(b),2,(Color){72,114,128,255});
		a = b;
	}
	centre("ORBIT",460,443,14,teal);
	for(i = 0; i < 3; i++){
		y = 480+i*22;
		DrawLineEx((Vector2){449,y+8},(Vector2){458,y},3,dim);
		DrawLineEx((Vector2){458,y},(Vector2){467,y+8},3,dim);
	}
	DrawTriangle(vec(slings[0][0]),vec(slings[0][1]),vec(slings[0][2]),dim);
	DrawTriangle(vec(slings[1][0]),vec(slings[1][2]),vec(slings[1][1]),dim);
	for(i = 0; i < nrail; i++){
		r = &rails[i];
		capsule((V){r->a.x+3,r->a.y+5},(V){r->b.x+3,r->b.y+5},Railrad+3,ink);
		capsule(r->a,r->b,Railrad+1,muted);
		switch(r->kind){
		case Sling: c = coral; break;
		case Target: c = amber; break;
		case Gate: c = teal; break;
		default: c = dim; break;
		}
		capsule(r->a,r->b,Railrad-1,c);
	}
	for(i = 0; i < Nbumper; i++){
		a = bumpers[i];
		DrawCircle(a.x+4,a.y+7,Bumperrad+7,ink);
		DrawCircleV(vec(a),Bumperrad+5,dim);
		DrawCircleV(vec(a),Bumperrad,muted);
		DrawCircleV(vec(a),Bumperrad-4,coral);
		DrawCircleV(vec(a),Bumperrad-10,bed);
		centre("+",a.x,a.y-13,28,coral);
	}
	for(i = 0; i < Nlane; i++){
		a = lanes[i];
		DrawRing(vec(a),20,22,0,360,24,dim);
	}
	centre("REACTOR",290,393,14,muted);
	centre("RELAYS",108,267,14,amber);
	centre("BALL SAVE",290,792,14,muted);
	centre("DRAIN",290,852,14,dim);
	DrawRectangle(539,784,18,18,muted);
	for(i = 0; i < 8; i++)
		DrawLine(539,805+i*5,557,808+i*5,dim);
	DrawText("GENERATED9 / ARCADE",624,35,16,teal);
	DrawText("ORBIT 9",620,67,44,white);
	DrawText("SPACE PINBALL",624,118,16,muted);
	DrawLine(624,151,886,151,dim);
	DrawText("SCORE",624,176,14,muted);
	DrawText("SESSION BEST",624,262,14,muted);
	DrawText("BALL",624,340,14,muted);
	DrawText("BONUS",770,340,14,muted);
	DrawLine(624,410,886,410,dim);
	DrawText("FLIGHT PLAN",624,434,16,teal);
	DrawLine(624,628,886,628,dim);
	DrawText("FLIGHT CONTROLS",624,710,14,teal);
	DrawText("Z or LEFT      left flipper",624,742,16,muted);
	DrawText("/ or RIGHT    right flipper",624,768,16,muted);
	DrawText("SPACE     hold, release to fire",624,794,16,muted);
	DrawText("UP   nudge      P   pause",624,820,16,muted);
	DrawText("R   new game     Q   quit",624,846,16,muted);
}

static void
drawgame(Game *g, vlong best, int paused, int demo)
{
	static char *orders[] = {"Light all 3 relays", "Hit 5 reactor bumpers", "Shoot the orbit ramp"};
	V a, b;
	char buf[96], *status;
	Color c;
	int i;

	for(i = 0; i < Nbumper; i++)
		if(g->flash[i] > 0){
			a = bumpers[i];
			DrawRing(vec(a),Bumperrad,Bumperrad+7,0,360,32,amber);
			DrawCircleV(vec(a),Bumperrad-10,amber);
		}
	for(i = 0; i < Ntarget; i++)
		lamp((V){109,313+i*52},g->targets & (1<<i),amber);
	for(i = 0; i < Nlane; i++)
		lamp(lanes[i],g->lanes & (1<<i),teal);
	for(i = 0; i < 2; i++){
		a = pivots[i];
		b = fliptip(g,i);
		capsule((V){a.x+3,a.y+6},(V){b.x+3,b.y+6},Fliprad+3,ink);
		capsule(a,b,Fliprad+2,muted);
		capsule(a,b,Fliprad-1,g->tilted ? dim : white);
		capsule(a,b,3,g->tilted ? dim : coral);
		DrawCircleV(vec(a),5,dim);
		if(g->sling[i] > 0)
			capsule(slings[i][2],slings[i][0],4,amber);
	}
	lamp((V){290,770},g->save > 0 && !g->tilted,teal);
	if(g->stage == 2){
		for(i = 0; i < 3; i++){
			DrawLineEx((Vector2){449,488+i*22},(Vector2){458,480+i*22},3,teal);
			DrawLineEx((Vector2){458,480+i*22},(Vector2){467,488+i*22},3,teal);
		}
	}
	if(g->state != Lost && g->state != Over){
		a = g->p;
		DrawCircle(a.x+4,a.y+6,Ballrad+2,(Color){0,0,0,130});
		DrawCircleV(vec(a),Ballrad,muted);
		DrawCircle(a.x-2,a.y-2,Ballrad-2,white);
		DrawCircle(a.x-4,a.y-4,3,WHITE);
	}
	if(g->state == Ready){
		DrawRectangle(540,784+g->charge*18,16,12,amber);
		DrawRectangle(540,720-g->charge*180,16,g->charge*180,teal);
	}
	snprint(buf,sizeof buf,"%09lld",g->score);
	DrawText(buf,622,204,38,amber);
	snprint(buf,sizeof buf,"%09lld",best);
	DrawText(buf,624,288,24,white);
	snprint(buf,sizeof buf,"%d / 3",g->ball);
	DrawText(buf,624,366,27,white);
	snprint(buf,sizeof buf,"%dx",g->multiplier);
	DrawText(buf,770,366,27,teal);
	for(i = 0; i < nelem(orders); i++){
		c = g->stage == i ? teal : g->stage > i ? white : dim;
		lamp((V){635,482+i*44},g->stage >= i,c);
		DrawText(orders[i],655,473+i*44,17,c);
	}
	snprint(buf,sizeof buf,"REACTOR %d/5    MISSIONS %d",g->hits,g->missions);
	DrawText(buf,624,602,14,muted);
	status = "KEEP THE BALL IN ORBIT";
	c = teal;
	if(g->state == Ready) status = "HOLD SPACE TO LAUNCH";
	if(g->state == Ramp) status = "ORBIT TRANSFER";
	if(g->state == Lost) status = "BALL LOST - BONUS ADDED";
	if(g->tilt > .8 && !g->tilted){status = "CAREFUL WITH THE NUDGE"; c = amber;}
	if(g->tilted){status = "TILT - FLIPPERS DISABLED"; c = coral;}
	if(g->state == Over){status = "GAME OVER - R TO RESTART"; c = amber;}
	if(demo) status = "DEMO - R TO PLAY";
	if(paused) status = "PAUSED - P TO RESUME";
	DrawText(status,624,655,16,c);
	if(paused || g->state == Over){
		DrawRectangle(120,424,340,110,(Color){9,17,29,240});
		centre(paused ? "PAUSED" : "GAME OVER",290,443,34,white);
		centre(paused ? "P to resume" : "R for a new flight",290,491,18,teal);
	}
}

static void
initsound(void)
{
	static double freq[Nsounds] = {100,240,880,140,180,400,660};
	short pcm[11025];
	Wave w;
	double t, envelope, phase;
	int i, j;

	InitAudioDevice();
	if(!IsAudioDeviceReady())
		sysfatal("initialize audio");
	for(i = 0; i < Nsounds; i++){
		for(j = 0; j < nelem(pcm); j++){
			t = j/44100.0;
			envelope = exp(-t*(i == 0 ? 60 : 18));
			phase = 2*PI*(freq[i]*t+(i == 3 || i == 5 ? 1100 : -150)*t*t);
			pcm[j] = 7000*envelope*(sin(phase)+.25*sin(phase*2));
		}
		w = (Wave){nelem(pcm),44100,16,1,pcm};
		sounds[i] = LoadSoundFromWave(w);
		if(!IsSoundValid(sounds[i]))
			sysfatal("initialize sound");
	}
}

/* A served ball may still have a moving flipper or spring. */
static int
atrest(Game *g, int keys)
{
	if(g->state == Over)
		return 1;
	if(g->state != Ready || keys != g->keys || g->omega[0] || g->omega[1])
		return 0;
	return keys & Launch ? g->charge == 1 : g->charge == 0;
}

static void
usage(void)
{
	fprint(2,"usage: games/pinball [-ad] [-n frames] [-o image.png]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	RenderTexture background;
	Camera2D camera = {0};
	Image image;
	char *out, *s, *end;
	double now, last, elapsed, accumulator;
	float scale;
	vlong best;
	int audio, demo, limit, paused, keys, pressed, key, i, w, h, waiting;

	out = nil;
	audio = demo = limit = paused = 0;
	ARGBEGIN{
	case 'a': audio = 1; break;
	case 'd': demo = 1; break;
	case 'n':
		s = EARGF(usage());
		limit = strtol(s,&end,10);
		if(s == end || *end || limit <= 0 || limit > 10000000) usage();
		break;
	case 'o': out = EARGF(usage()); break;
	default: usage();
	}ARGEND
	if(argc) usage();
	tableinit();
	newgame(&game);
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(828,810,"Orbit 9 - pinball");
	if(!IsWindowReady())
		sysfatal("initialize game window");
	SetExitKey(KEY_Q);
	SetTargetFPS(60);
	background = LoadRenderTexture(Width,Height);
	if(!IsRenderTextureValid(background))
		sysfatal("initialize table texture");
	BeginTextureMode(background);
	board();
	EndTextureMode();
	if(audio)
		initsound();
	best = 0;
	pressed = 0;
	accumulator = 0;
	waiting = 0;
	last = GetTime();
	while(!WindowShouldClose()){
		now = GetTime();
		elapsed = waiting ? 1.0/Hz : now-last;
		last = now;
		if(elapsed > .1) elapsed = .1;
		while((key = GetKeyPressed()) != 0){
			switch(key){
			case KEY_P: paused = !paused; elapsed = 0; break;
			case KEY_R:
				newgame(&game);
				demo = paused = pressed = 0;
				accumulator = elapsed = 0;
				break;
			case KEY_LEFT: case KEY_Z: pressed |= Left; break;
			case KEY_RIGHT: case KEY_SLASH: pressed |= Right; break;
			case KEY_SPACE: pressed |= Launch; break;
			case KEY_UP: pressed |= Nudge; break;
			}
		}
		if(!demo && !IsWindowFocused() &&
		   ((game.state != Ready && game.state != Over) || game.charge > 0))
			paused = 1;
		keys = 0;
		if(IsKeyDown(KEY_Z) || IsKeyDown(KEY_LEFT)) keys |= Left;
		if(IsKeyDown(KEY_SLASH) || IsKeyDown(KEY_RIGHT)) keys |= Right;
		if(IsKeyDown(KEY_SPACE)) keys |= Launch;
		keys |= pressed;
		if(!paused){
			accumulator += limit ? 1.0/60 : elapsed;
			while(accumulator >= 1.0/Hz){
				if(demo){
					if(game.state == Over) newgame(&game);
					keys = demokeys(&game);
				}
				step(&game,keys);
				pressed = 0;
				accumulator -= 1.0/Hz;
			}
		}else{
			accumulator = 0;
			pressed = 0;
		}
		if(game.score > best) best = game.score;
		if(audio)
			for(i = 0; i < Nsounds; i++)
				if(game.events & (1<<i)) PlaySound(sounds[i]);
		game.events = 0;
		waiting = !limit && (paused || (!demo && !pressed && atrest(&game,keys)));
		if(waiting)
			EnableEventWaiting();
		else
			DisableEventWaiting();
		w = GetScreenWidth();
		h = GetScreenHeight();
		scale = (float)w/Width;
		if((float)h/Height < scale) scale = (float)h/Height;
		camera.offset = (Vector2){(w-Width*scale)/2,(h-Height*scale)/2};
		camera.zoom = scale;
		BeginDrawing();
		ClearBackground(ink);
		BeginMode2D(camera);
		DrawTextureRec(background.texture,(Rectangle){0,0,Width,-Height},(Vector2){0,0},WHITE);
		drawgame(&game,best,paused,demo);
		EndMode2D();
		EndDrawing();
		if(limit && --limit == 0) break;
	}
	if(out != nil){
		image = LoadImageFromScreen();
		if(!ExportImage(image,out))
			sysfatal("save %s",out);
		UnloadImage(image);
	}
	if(audio){
		for(i = 0; i < Nsounds; i++) UnloadSound(sounds[i]);
		CloseAudioDevice();
	}
	UnloadRenderTexture(background);
	CloseWindow();
	exits(nil);
}
