#include <u.h>
#include <libc.h>
#include <raylib.h>
#define RAYMATH_STATIC_INLINE
#include <raymath.h>
#include <raygui.h>
#include <rlgl.h>
#include <GL/gl.h>

static Model model;
static ModelAnimation *anims;
static int nanims, anim, triangles;
static float frame, yaw, pitch, distance;
static Vector3 center;
static float scale;
static char path[1024], message[256];

/* LoadModel owns its meshes; the caller owns the material textures. */
static void
freemodel(Model model)
{
	Texture t;
	int i, j, k, l;

	for(i=0; i<model.materialCount; i++)
		for(j=0; j<=MATERIAL_MAP_BRDF; j++){
			t = model.materials[i].maps[j].texture;
			if(t.id == 0 || t.id == rlGetTextureIdDefault()) continue;
			/* A texture can be referenced by several maps and materials. */
			for(k=i; k<model.materialCount; k++)
				for(l=0; l<=MATERIAL_MAP_BRDF; l++)
					if(model.materials[k].maps[l].texture.id == t.id)
						model.materials[k].maps[l].texture.id = 0;
			UnloadTexture(t);
		}
	UnloadModel(model);
}

static void
unload(void)
{
	UnloadModelAnimations(anims,nanims);
	anims = nil;
	nanims = anim = 0;
	freemodel(model);
	memset(&model,0,sizeof model);
}

static void
resetview(void)
{
	yaw = .7;
	pitch = .35;
	distance = 5;
}

static int
load(char *name)
{
	Model next;
	BoundingBox box;
	Vector3 size, midpoint;
	float extent;
	int i;

	if(name[0]){
		if(!FileExists(name)){
			snprint(message,sizeof message,"Cannot open %s",name);
			return 0;
		}
		if(!IsFileExtension(name,".obj;.iqm;.gltf;.glb;.m3d;.vox")){
			snprint(message,sizeof message,"Unsupported model format");
			return 0;
		}
		next = LoadModel(name);
	}else
		next = LoadModelFromMesh(GenMeshTorus(.4,1,24,48));
	if(!IsModelValid(next)){
		freemodel(next);
		snprint(message,sizeof message,"Cannot load model");
		return 0;
	}
	box = GetModelBoundingBox(next);
	midpoint = Vector3Scale(Vector3Add(box.min,box.max),.5);
	size = Vector3Subtract(box.max,box.min);
	extent = fmax(size.x,fmax(size.y,size.z));
	if(!(extent > 0) || !isfinite(extent) || !isfinite(midpoint.x)
	|| !isfinite(midpoint.y) || !isfinite(midpoint.z)){
		freemodel(next);
		snprint(message,sizeof message,"Model has no finite bounds");
		return 0;
	}
	unload();
	model = next;
	center = midpoint;
	scale = 2/extent;
	triangles = 0;
	for(i=0; i<model.meshCount; i++) triangles += model.meshes[i].triangleCount;
	if(name[0] && IsFileExtension(name,".iqm;.gltf;.glb;.m3d"))
		anims = LoadModelAnimations(name,&nanims);
	frame = 0;
	resetview();
	snprint(message,sizeof message,"%d meshes, %d triangles, %d animations",
		model.meshCount,triangles,nanims);
	return 1;
}

static void
lighting(void)
{
	float pos[4] = {-3,5,4,0};
	float ambient[4] = {.25,.25,.25,1};
	float diffuse[4] = {.8,.8,.8,1};

	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
	glLightfv(GL_LIGHT0,GL_POSITION,pos);
	glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
}

static void
snapshot(char *name)
{
	Image image;
	int ok;

	image = LoadImageFromScreen();
	ok = ExportImage(image,name);
	UnloadImage(image);
	if(!ok) sysfatal("save image %s",name);
}

static void
usage(void)
{
	fprint(2,"usage: modelview [-n frames] [-o screenshot.png] [model]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Camera camera = {{0},{0},{0,1,0},45,CAMERA_PERSPECTIVE};
	Vector2 mouse, delta;
	Vector3 offset;
	bool wire, grid, light, rotate, play, editing;
	float dt, speed, width;
	char *shot;
	int limit, frames, w, h, open;

	limit = frames = 0;
	shot = nil;
	ARGBEGIN{
	case 'n': limit = atoi(EARGF(usage())); if(limit < 1) usage(); break;
	case 'o': shot = EARGF(usage()); break;
	default: usage();
	}ARGEND
	if(argc > 1 || (argc && strlen(argv[0]) >= sizeof path)) usage();
	if(argc) strcpy(path,argv[0]);
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(900,650,"modelview");
	SetTargetFPS(60);
	SetExitKey(KEY_NULL);
	GuiSetStyle(DEFAULT,TEXT_SIZE,16);
	if(!load(path)) sysfatal("%s",message);
	wire = rotate = editing = false;
	grid = light = play = true;
	speed = 1;
	while(!WindowShouldClose()){
		dt = fmin(GetFrameTime(),.1);
		w = GetScreenWidth();
		h = GetScreenHeight();
		mouse = GetMousePosition();
		if(IsKeyPressed(KEY_ESCAPE)){
			if(editing) editing = false;
			else break;
		}
		if(!editing){
			if(IsKeyPressed(KEY_R)) resetview();
			if(IsKeyPressed(KEY_SPACE)) rotate = !rotate;
		}
		if(!editing && mouse.y > 112 && mouse.y < h-28){
			if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
				delta = GetMouseDelta();
				yaw -= delta.x*.01;
				pitch = Clamp(pitch+delta.y*.01,-1.5,1.5);
			}
			distance = Clamp(distance*pow(.85,GetMouseWheelMove()),1,30);
		}
		BeginDrawing();
		ClearBackground((Color){225,230,235,255});
		GuiPanel((Rectangle){0,0,w,112},nil);
		if(w >= 560){
			width = w-200;
			open = GuiTextBox((Rectangle){10,10,width,26},path,sizeof path,editing);
			if(open){
				if(editing && IsKeyPressed(KEY_ENTER)) load(path);
				editing = !editing;
			}
			if(GuiButton((Rectangle){width+20,10,75,26},"Open")){
				load(path);
				editing = false;
			}
			if(GuiButton((Rectangle){width+105,10,75,26},"Reset")) resetview();
			GuiCheckBox((Rectangle){10,47,18,18},"Grid",&grid);
			GuiCheckBox((Rectangle){90,47,18,18},"Wire",&wire);
			GuiCheckBox((Rectangle){175,47,18,18},"Light",&light);
			GuiCheckBox((Rectangle){265,47,18,18},"Rotate",&rotate);
			GuiSlider((Rectangle){400,47,120,18},"Speed",nil,&speed,0,3);
			if(nanims){
				GuiCheckBox((Rectangle){10,82,18,18},"Animate",&play);
				if(GuiButton((Rectangle){125,77,60,26},"Next")){
					anim = (anim+1)%nanims;
					frame = 0;
				}
				GuiLabel((Rectangle){195,77,w-205,26},TextFormat("%d/%d: %s",anim+1,nanims,anims[anim].name));
			}else GuiLabel((Rectangle){10,77,w-20,26},"Drag: orbit   wheel: zoom   Space: rotate   R: reset");
		}else GuiLabel((Rectangle){10,10,w-20,80},"Enlarge window to show controls");
		if(rotate) yaw += dt*speed*.5;
		if(nanims && IsModelAnimationValid(model,anims[anim]) && anims[anim].keyframeCount > 0){
			if(play) frame = fmod(frame+dt*speed*60,anims[anim].keyframeCount);
			UpdateModelAnimation(model,anims[anim],frame);
		}
		camera.position = (Vector3){distance*cos(pitch)*sin(yaw),distance*sin(pitch),distance*cos(pitch)*cos(yaw)};
		offset = Vector3Scale(center,-scale);
		BeginScissorMode(0,112,w,h > 140 ? h-140 : 0);
		BeginMode3D(camera);
		if(grid) DrawGrid(10,1);
		if(light && !wire) lighting();
		if(wire) DrawModelWires(model,offset,scale,DARKBLUE);
		else DrawModel(model,offset,scale,WHITE);
		glDisable(GL_LIGHTING);
		glDisable(GL_COLOR_MATERIAL);
		EndMode3D();
		EndScissorMode();
		GuiStatusBar((Rectangle){0,h-28,w,28},message);
		if(shot && frames+1 == limit) snapshot(shot);
		if(!limit && !editing && !(rotate && speed > 0) && !(nanims && play && speed > 0)) EnableEventWaiting();
		else DisableEventWaiting();
		EndDrawing();
		if(++frames == limit) break;
	}
	if(shot && !limit) snapshot(shot);
	unload();
	CloseWindow();
	exits(nil);
}
