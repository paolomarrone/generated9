#include "rayjs.h"
#include <raylib.h>
#include <rlgl.h>

enum { Rimage, Rtexture, Rmodel, Rsound };
typedef struct Resource Resource;
struct Resource {
	int kind, live;
	union {
		Image image;
		Texture texture;
		Model model;
		Sound sound;
	} u;
	Resource *next, **prev;
};
static Resource *resources;
static JSClassID resourceid;
/* 0: no window, 1: idle, 2: drawing, 3: drawing in 3D. */
static int drawing;

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
release(Resource *r)
{
	if(!r->live)
		return;
	switch(r->kind){
	case Rimage: UnloadImage(r->u.image); break;
	case Rtexture: UnloadTexture(r->u.texture); break;
	case Rmodel: freemodel(r->u.model); break;
	case Rsound: UnloadSound(r->u.sound); break;
	}
	r->live = 0;
}

static void
finalize(JSRuntime *rt, JSValue v)
{
	Resource *r;

	USED(rt);
	r = JS_GetOpaque(v, resourceid);
	if(r == nil)
		return;
	release(r);
	*r->prev = r->next;
	if(r->next != nil)
		r->next->prev = r->prev;
	free(r);
}

static void
closeaudio(void)
{
	Resource *r;

	for(r = resources; r != nil; r = r->next)
		if(r->kind == Rsound)
			release(r);
	if(IsAudioDeviceReady())
		CloseAudioDevice();
}

static void
closewindow(void)
{
	Resource *r;

	if(drawing == 0)
		return;
	DisableEventWaiting();
	if(drawing == 3)
		EndMode3D();
	if(drawing >= 2)
		EndDrawing();
	for(r = resources; r != nil; r = r->next)
		if(r->kind == Rtexture || r->kind == Rmodel)
			release(r);
	CloseWindow();
	drawing = 0;
}

void
rayclose(void)
{
	closeaudio();
	closewindow();
}

static int
need(JSContext *ctx, int mode)
{
	if(drawing != 0 && (mode == 0 || drawing == mode))
		return 0;
	JS_ThrowTypeError(ctx, "invalid drawing state");
	return -1;
}

static int
number(JSContext *ctx, JSValueConst v, double *d)
{
	if(!JS_IsNumber(v) || JS_ToFloat64(ctx, d, v) < 0 || !isfinite(*d) || fabs(*d) > 3.402823466e38){
		JS_ThrowTypeError(ctx, "expected a finite number representable as float");
		return -1;
	}
	return 0;
}

static int
integer(JSContext *ctx, JSValueConst v, int *n, int lo, int hi)
{
	double d;

	if(number(ctx, v, &d) < 0)
		return -1;
	if(d < lo || d > hi || floor(d) != d){
		JS_ThrowRangeError(ctx, "expected an integer in %d..%d", lo, hi);
		return -1;
	}
	*n = d;
	return 0;
}

static int
fields(JSContext *ctx, JSValueConst v, char **names, double *d, int n)
{
	JSValue x;
	int i, ok;

	for(i = 0; i < n; i++){
		x = JS_GetPropertyStr(ctx, v, names[i]);
		if(JS_IsException(x))
			return -1;
		ok = number(ctx, x, &d[i]);
		JS_FreeValue(ctx, x);
		if(ok < 0)
			return -1;
	}
	return 0;
}

static int
vector(JSContext *ctx, JSValueConst v, Vector3 *p)
{
	static char *names[] = {"x", "y", "z"};
	double d[3];

	if(fields(ctx, v, names, d, 3) < 0)
		return -1;
	*p = (Vector3){d[0],d[1],d[2]};
	return 0;
}

static int
color(JSContext *ctx, JSValueConst v, Color *c)
{
	static char *names[] = {"r", "g", "b", "a"};
	double d[4];
	int i;

	if(fields(ctx, v, names, d, 4) < 0)
		return -1;
	for(i = 0; i < 4; i++)
		if(d[i] < 0 || d[i] > 255 || floor(d[i]) != d[i]){
			JS_ThrowRangeError(ctx, "color components must be integers in 0..255");
			return -1;
		}
	*c = (Color){d[0],d[1],d[2],d[3]};
	return 0;
}

static int
camera(JSContext *ctx, JSValueConst v, Camera3D *c)
{
	static char *names[] = {"position", "target", "up"};
	Vector3 p[3];
	JSValue x;
	double fov;
	int i, ok;

	for(i = 0; i < 3; i++){
		x = JS_GetPropertyStr(ctx, v, names[i]);
		if(JS_IsException(x)) return -1;
		ok = vector(ctx, x, &p[i]);
		JS_FreeValue(ctx, x);
		if(ok < 0) return -1;
	}
	x = JS_GetPropertyStr(ctx, v, "fovy");
	if(JS_IsException(x)) return -1;
	ok = number(ctx, x, &fov);
	JS_FreeValue(ctx, x);
	if(ok < 0) return -1;
	x = JS_GetPropertyStr(ctx, v, "projection");
	if(JS_IsException(x)) return -1;
	ok = integer(ctx, x, &c->projection, CAMERA_PERSPECTIVE, CAMERA_ORTHOGRAPHIC);
	JS_FreeValue(ctx, x);
	if(ok < 0) return -1;
	if(fov <= 0 || (c->projection == CAMERA_PERSPECTIVE && fov >= 180)){
		JS_ThrowRangeError(ctx, "invalid camera field of view");
		return -1;
	}
	c->position = p[0]; c->target = p[1]; c->up = p[2]; c->fovy = fov;
	return 0;
}

static Resource *
resource(JSContext *ctx, JSValueConst v, int kind, int live)
{
	Resource *r;

	r = JS_GetOpaque2(ctx, v, resourceid);
	if(r == nil)
		return nil;
	if(r->kind != kind || (live && !r->live)){
		JS_ThrowTypeError(ctx, "wrong resource type or resource already unloaded");
		return nil;
	}
	return r;
}

static JSValue
newresource(JSContext *ctx, Resource *value)
{
	JSValue v;
	Resource *r;
	int w, h;

	v = JS_NewObjectClass(ctx, resourceid);
	if(JS_IsException(v)){
		release(value);
		return v;
	}
	r = malloc(sizeof *r);
	if(r == nil){
		JS_FreeValue(ctx, v);
		release(value);
		return JS_ThrowOutOfMemory(ctx);
	}
	*r = *value;
	r->next = resources;
	r->prev = &resources;
	if(resources != nil)
		resources->prev = &r->next;
	resources = r;
	JS_SetOpaque(v, r);
	w = h = 0;
	if(r->kind == Rtexture){w = r->u.texture.width; h = r->u.texture.height;}
	if(r->kind == Rimage){w = r->u.image.width; h = r->u.image.height;}
	if(w && (JS_DefinePropertyValueStr(ctx, v, "width", JS_NewInt32(ctx,w), JS_PROP_ENUMERABLE) < 0 ||
	    JS_DefinePropertyValueStr(ctx, v, "height", JS_NewInt32(ctx,h), JS_PROP_ENUMERABLE) < 0)){
		JS_FreeValue(ctx, v);
		return JS_EXCEPTION;
	}
	return v;
}

/* The function table and switch describe the supported raylib subset. */
enum {
	FInitWindow, FCloseWindow, FWindowShouldClose, FIsWindowReady,
	FEnableEventWaiting, FDisableEventWaiting, FSetTargetFPS, FGetFrameTime, FGetTime, FGetFPS, FGetScreenWidth, FGetScreenHeight,
	FBeginDrawing, FEndDrawing, FClearBackground, FBeginMode3D, FEndMode3D,
	FIsKeyDown, FIsKeyPressed, FIsKeyReleased, FIsMouseButtonDown, FIsMouseButtonPressed,
	FGetMouseX, FGetMouseY, FGetMouseWheelMove,
	FDrawText, FDrawFPS, FDrawRectangle, FDrawCircle, FDrawLine,
	FDrawCube, FDrawCubeWires, FDrawSphere, FDrawPlane, FDrawGrid,
	FLoadImageFromScreen, FExportImage, FLoadImage, FGenImageColor, FGenImageChecked, FUnloadImage,
	FLoadTexture, FLoadTextureFromImage, FUnloadTexture, FDrawTextureEx,
	FLoadModel, FUnloadModel, FDrawModel,
	FInitAudioDevice, FCloseAudioDevice, FIsAudioDeviceReady,
	FLoadSound, FLoadSoundFromWave, FUnloadSound, FPlaySound, FStopSound, FIsSoundPlaying,
	FSetSoundVolume,
};

static JSValue
call(JSContext *ctx, JSValueConst self, int argc, JSValueConst *a, int op)
{
	Resource value, *r;
	const char *s;
	JSValue data;
	Vector3 p;
	Camera3D cam;
	Color c, c2;
	Wave wave;
	double d[5];
	size_t len;
	int n[4], i, ok;
	static char *xy[] = {"x", "y"};
	static char *wavefields[] = {"frameCount", "sampleRate", "sampleSize", "channels"};

	USED(self, argc);
	memset(&value, 0, sizeof value);
	value.live = 1;
	switch(op){
	case FInitWindow:
		if(integer(ctx,a[0],&n[0],1,2048)<0 || integer(ctx,a[1],&n[1],1,2048)<0) break;
		s = JS_ToCString(ctx,a[2]);
		if(s == nil) break;
		if(drawing){
			JS_FreeCString(ctx,s);
			return JS_ThrowTypeError(ctx,"window already initialized");
		}
		InitWindow(n[0],n[1],s);
		JS_FreeCString(ctx,s);
		if(!IsWindowReady()) return JS_ThrowInternalError(ctx,"initialize window");
		drawing = 1;
		return JS_UNDEFINED;
	case FCloseWindow:
		if(drawing >= 2) return JS_ThrowTypeError(ctx,"end drawing before closing the window");
		closewindow(); return JS_UNDEFINED;
	case FWindowShouldClose: return JS_NewBool(ctx,WindowShouldClose());
	case FIsWindowReady: return JS_NewBool(ctx,IsWindowReady());
	case FEnableEventWaiting: case FDisableEventWaiting:
		if(need(ctx,0)<0) break;
		if(op==FEnableEventWaiting) EnableEventWaiting();
		else DisableEventWaiting();
		return JS_UNDEFINED;
	case FSetTargetFPS:
		if(integer(ctx,a[0],&n[0],0,1000)<0 || need(ctx,0)<0) break;
		SetTargetFPS(n[0]); return JS_UNDEFINED;
	case FGetFrameTime: return JS_NewFloat64(ctx,GetFrameTime());
	case FGetTime: return JS_NewFloat64(ctx,GetTime());
	case FGetFPS: return JS_NewInt32(ctx,GetFPS());
	case FGetScreenWidth: return JS_NewInt32(ctx,GetScreenWidth());
	case FGetScreenHeight: return JS_NewInt32(ctx,GetScreenHeight());
	case FBeginDrawing:
		if(need(ctx,1)<0) break;
		BeginDrawing(); drawing=2; return JS_UNDEFINED;
	case FEndDrawing:
		if(need(ctx,2)<0) break;
		EndDrawing(); drawing=1; return JS_UNDEFINED;
	case FClearBackground:
		if(color(ctx,a[0],&c)<0 || need(ctx,2)<0) break;
		ClearBackground(c); return JS_UNDEFINED;
	case FBeginMode3D:
		if(camera(ctx,a[0],&cam)<0 || need(ctx,2)<0) break;
		BeginMode3D(cam); drawing=3; return JS_UNDEFINED;
	case FEndMode3D:
		if(need(ctx,3)<0) break;
		EndMode3D(); drawing=2; return JS_UNDEFINED;
	case FIsKeyDown: case FIsKeyPressed: case FIsKeyReleased:
		if(integer(ctx,a[0],&n[0],0,511)<0 || need(ctx,0)<0) break;
		if(op==FIsKeyDown) return JS_NewBool(ctx,IsKeyDown(n[0]));
		if(op==FIsKeyPressed) return JS_NewBool(ctx,IsKeyPressed(n[0]));
		return JS_NewBool(ctx,IsKeyReleased(n[0]));
	case FIsMouseButtonDown: case FIsMouseButtonPressed:
		if(integer(ctx,a[0],&n[0],0,6)<0 || need(ctx,0)<0) break;
		return JS_NewBool(ctx,op==FIsMouseButtonDown ? IsMouseButtonDown(n[0]) : IsMouseButtonPressed(n[0]));
	case FGetMouseX: return JS_NewInt32(ctx,GetMouseX());
	case FGetMouseY: return JS_NewInt32(ctx,GetMouseY());
	case FGetMouseWheelMove: return JS_NewFloat64(ctx,GetMouseWheelMove());
	case FDrawText:
		if(integer(ctx,a[1],&n[0],-32768,32767)<0 || integer(ctx,a[2],&n[1],-32768,32767)<0 ||
		   integer(ctx,a[3],&n[2],1,512)<0 || color(ctx,a[4],&c)<0) break;
		s=JS_ToCString(ctx,a[0]);
		if(s==nil) break;
		ok=need(ctx,2);
		if(ok==0) DrawText(s,n[0],n[1],n[2],c);
		JS_FreeCString(ctx,s);
		if(ok<0) break;
		return JS_UNDEFINED;
	case FDrawFPS:
		if(integer(ctx,a[0],&n[0],-32768,32767)<0 || integer(ctx,a[1],&n[1],-32768,32767)<0 || need(ctx,2)<0) break;
		DrawFPS(n[0],n[1]); return JS_UNDEFINED;
	case FDrawRectangle: case FDrawLine:
		for(i=0;i<4;i++) if(integer(ctx,a[i],&n[i],-32768,32767)<0) return JS_EXCEPTION;
		if(color(ctx,a[4],&c)<0 || need(ctx,2)<0) break;
		if(op==FDrawRectangle) DrawRectangle(n[0],n[1],n[2],n[3],c);
		else DrawLine(n[0],n[1],n[2],n[3],c);
		return JS_UNDEFINED;
	case FDrawCircle:
		if(integer(ctx,a[0],&n[0],-32768,32767)<0 || integer(ctx,a[1],&n[1],-32768,32767)<0 ||
		   number(ctx,a[2],&d[0])<0 || color(ctx,a[3],&c)<0 || need(ctx,2)<0) break;
		DrawCircle(n[0],n[1],d[0],c); return JS_UNDEFINED;
	case FDrawCube: case FDrawCubeWires:
		if(vector(ctx,a[0],&p)<0) break;
		for(i=0;i<3;i++) if(number(ctx,a[i+1],&d[i])<0) return JS_EXCEPTION;
		if(color(ctx,a[4],&c)<0 || need(ctx,3)<0) break;
		if(op==FDrawCube) DrawCube(p,d[0],d[1],d[2],c);
		else DrawCubeWires(p,d[0],d[1],d[2],c);
		return JS_UNDEFINED;
	case FDrawSphere:
		if(vector(ctx,a[0],&p)<0 || number(ctx,a[1],&d[0])<0 || color(ctx,a[2],&c)<0 || need(ctx,3)<0) break;
		DrawSphere(p,d[0],c); return JS_UNDEFINED;
	case FDrawPlane:
		if(vector(ctx,a[0],&p)<0 || fields(ctx,a[1],xy,d,2)<0 || color(ctx,a[2],&c)<0 || need(ctx,3)<0) break;
		DrawPlane(p,(Vector2){d[0],d[1]},c); return JS_UNDEFINED;
	case FDrawGrid:
		if(integer(ctx,a[0],&n[0],1,512)<0 || number(ctx,a[1],&d[0])<0 || need(ctx,3)<0) break;
		DrawGrid(n[0],d[0]); return JS_UNDEFINED;
	case FLoadImage: case FLoadTexture: case FLoadModel: case FLoadSound:
		s=JS_ToCString(ctx,a[0]);
		if(s==nil) break;
		if((op==FLoadTexture || op==FLoadModel) && need(ctx,0)<0){JS_FreeCString(ctx,s); break;}
		if(op==FLoadSound && !IsAudioDeviceReady()){
			JS_FreeCString(ctx,s); return JS_ThrowTypeError(ctx,"audio device is not initialized");
		}
		switch(op){
		case FLoadImage: value.kind=Rimage; value.u.image=LoadImage(s); ok=IsImageValid(value.u.image); break;
		case FLoadTexture: value.kind=Rtexture; value.u.texture=LoadTexture(s); ok=IsTextureValid(value.u.texture); break;
		case FLoadModel: value.kind=Rmodel; value.u.model=LoadModel(s); ok=IsModelValid(value.u.model); break;
		default: value.kind=Rsound; value.u.sound=LoadSound(s); ok=IsSoundValid(value.u.sound); break;
		}
		JS_FreeCString(ctx,s);
		if(!ok){release(&value); return JS_ThrowInternalError(ctx,"load resource failed");}
		return newresource(ctx,&value);
	case FGenImageColor: case FGenImageChecked:
		if(integer(ctx,a[0],&n[0],1,2048)<0 || integer(ctx,a[1],&n[1],1,2048)<0) break;
		value.kind=Rimage;
		if(op==FGenImageColor){
			if(color(ctx,a[2],&c)<0) break;
			value.u.image=GenImageColor(n[0],n[1],c);
		}else{
			if(integer(ctx,a[2],&n[2],1,2048)<0 || integer(ctx,a[3],&n[3],1,2048)<0 || color(ctx,a[4],&c)<0 || color(ctx,a[5],&c2)<0) break;
			value.u.image=GenImageChecked(n[0],n[1],n[2],n[3],c,c2);
		}
		if(!IsImageValid(value.u.image)) return JS_ThrowOutOfMemory(ctx);
		return newresource(ctx,&value);
	case FLoadTextureFromImage:
		if(need(ctx,0)<0 || (r=resource(ctx,a[0],Rimage,1))==nil) break;
		value.kind=Rtexture; value.u.texture=LoadTextureFromImage(r->u.image);
		if(!IsTextureValid(value.u.texture)) return JS_ThrowInternalError(ctx,"load texture failed");
		return newresource(ctx,&value);
	case FUnloadImage: case FUnloadTexture: case FUnloadModel: case FUnloadSound:
		i=op==FUnloadImage ? Rimage : op==FUnloadTexture ? Rtexture : op==FUnloadModel ? Rmodel : Rsound;
		if((r=resource(ctx,a[0],i,0))==nil) break;
		release(r); return JS_UNDEFINED;
	case FDrawTextureEx:
		if(fields(ctx,a[1],xy,d,2)<0 || number(ctx,a[2],&d[2])<0 || number(ctx,a[3],&d[3])<0 || color(ctx,a[4],&c)<0 || need(ctx,2)<0) break;
		if((r=resource(ctx,a[0],Rtexture,1))==nil) break;
		DrawTextureEx(r->u.texture,(Vector2){d[0],d[1]},d[2],d[3],c); return JS_UNDEFINED;
	case FDrawModel:
		if(vector(ctx,a[1],&p)<0 || number(ctx,a[2],&d[0])<0 || color(ctx,a[3],&c)<0 || need(ctx,3)<0) break;
		if((r=resource(ctx,a[0],Rmodel,1))==nil) break;
		DrawModel(r->u.model,p,d[0],c); return JS_UNDEFINED;
	case FInitAudioDevice:
		if(IsAudioDeviceReady()) return JS_ThrowTypeError(ctx,"audio device already initialized");
		InitAudioDevice();
		if(!IsAudioDeviceReady()) return JS_ThrowInternalError(ctx,"initialize audio device");
		return JS_UNDEFINED;
	case FCloseAudioDevice: closeaudio(); return JS_UNDEFINED;
	case FIsAudioDeviceReady: return JS_NewBool(ctx,IsAudioDeviceReady());
	case FLoadSoundFromWave:
		if(fields(ctx,a[0],wavefields,d,4)<0) break;
		for(i=0;i<4;i++) if(floor(d[i])!=d[i]) return JS_ThrowRangeError(ctx,"wave fields must be integers");
		if(d[0]<1 || d[0]>44100*60 || d[1]<8000 || d[1]>192000 ||
		   (d[2]!=8 && d[2]!=16 && d[2]!=32) || d[3]<1 || d[3]>2)
			return JS_ThrowRangeError(ctx,"invalid wave format or length");
		wave=(Wave){d[0],d[1],d[2],d[3],nil};
		data=JS_GetPropertyStr(ctx,a[0],"data");
		if(JS_IsException(data)) break;
		wave.data=JS_GetArrayBuffer(ctx,&len,data);
		if(wave.data==nil){JS_FreeValue(ctx,data); break;}
		if(len != wave.frameCount*wave.channels*(wave.sampleSize/8) || !IsAudioDeviceReady()){
			JS_FreeValue(ctx,data); return JS_ThrowTypeError(ctx,"wave buffer length mismatch or audio device not initialized");
		}
		value.kind=Rsound; value.u.sound=LoadSoundFromWave(wave);
		JS_FreeValue(ctx,data);
		if(!IsSoundValid(value.u.sound)) return JS_ThrowInternalError(ctx,"load sound failed");
		return newresource(ctx,&value);
	case FPlaySound: case FStopSound: case FIsSoundPlaying: case FSetSoundVolume:
		if(op==FSetSoundVolume && (number(ctx,a[1],&d[0])<0)) break;
		if((r=resource(ctx,a[0],Rsound,1))==nil) break;
		if(op==FPlaySound) PlaySound(r->u.sound);
		else if(op==FStopSound) StopSound(r->u.sound);
		else if(op==FSetSoundVolume) SetSoundVolume(r->u.sound,d[0]);
		else return JS_NewBool(ctx,IsSoundPlaying(r->u.sound));
		return JS_UNDEFINED;
	case FLoadImageFromScreen:
		if(need(ctx,1)<0) break;
		value.kind=Rimage; value.u.image=LoadImageFromScreen();
		if(!IsImageValid(value.u.image)) return JS_ThrowInternalError(ctx,"read screen image failed");
		return newresource(ctx,&value);
	case FExportImage:
		s=JS_ToCString(ctx,a[1]);
		if(s==nil) break;
		r=resource(ctx,a[0],Rimage,1);
		if(r==nil){JS_FreeCString(ctx,s); break;}
		ok=ExportImage(r->u.image,s);
		JS_FreeCString(ctx,s);
		return JS_NewBool(ctx,ok);
	}
	return JS_EXCEPTION;
}

#define FN(name, n) JS_CFUNC_MAGIC_DEF(#name, n, call, F##name)
#define INT(name) JS_PROP_INT32_DEF(#name, name, JS_PROP_ENUMERABLE)
static const JSCFunctionListEntry functions[] = {
	FN(InitWindow,3), FN(CloseWindow,0), FN(WindowShouldClose,0), FN(IsWindowReady,0),
	FN(EnableEventWaiting,0), FN(DisableEventWaiting,0), FN(SetTargetFPS,1), FN(GetFrameTime,0), FN(GetTime,0), FN(GetFPS,0), FN(GetScreenWidth,0), FN(GetScreenHeight,0),
	FN(BeginDrawing,0), FN(EndDrawing,0), FN(ClearBackground,1), FN(BeginMode3D,1), FN(EndMode3D,0),
	FN(IsKeyDown,1), FN(IsKeyPressed,1), FN(IsKeyReleased,1), FN(IsMouseButtonDown,1), FN(IsMouseButtonPressed,1),
	FN(GetMouseX,0), FN(GetMouseY,0), FN(GetMouseWheelMove,0),
	FN(DrawText,5), FN(DrawFPS,2), FN(DrawRectangle,5), FN(DrawCircle,4), FN(DrawLine,5),
	FN(DrawCube,5), FN(DrawCubeWires,5), FN(DrawSphere,3), FN(DrawPlane,3), FN(DrawGrid,2),
	FN(LoadImageFromScreen,0), FN(ExportImage,2), FN(LoadImage,1), FN(GenImageColor,3), FN(GenImageChecked,6), FN(UnloadImage,1),
	FN(LoadTexture,1), FN(LoadTextureFromImage,1), FN(UnloadTexture,1), FN(DrawTextureEx,5),
	FN(LoadModel,1), FN(UnloadModel,1), FN(DrawModel,4),
	FN(InitAudioDevice,0), FN(CloseAudioDevice,0), FN(IsAudioDeviceReady,0),
	FN(LoadSound,1), FN(LoadSoundFromWave,1), FN(UnloadSound,1), FN(PlaySound,1), FN(StopSound,1), FN(IsSoundPlaying,1),
	FN(SetSoundVolume,2),
	INT(KEY_UP), INT(KEY_DOWN), INT(KEY_LEFT), INT(KEY_RIGHT), INT(KEY_SPACE), INT(KEY_ENTER), INT(KEY_ESCAPE),
	INT(KEY_W), INT(KEY_A), INT(KEY_S), INT(KEY_D), INT(KEY_R), INT(KEY_P),
	INT(MOUSE_BUTTON_LEFT), INT(MOUSE_BUTTON_RIGHT), INT(MOUSE_BUTTON_MIDDLE),
	INT(CAMERA_PERSPECTIVE), INT(CAMERA_ORTHOGRAPHIC),
};

static struct {
	char *name;
	Color color;
} colors[] = {
	{"LIGHTGRAY", {200, 200, 200, 255}},
	{"GRAY", {130, 130, 130, 255}},
	{"DARKGRAY", {80, 80, 80, 255}},
	{"YELLOW", {253, 249, 0, 255}},
	{"GOLD", {255, 203, 0, 255}},
	{"ORANGE", {255, 161, 0, 255}},
	{"PINK", {255, 109, 194, 255}},
	{"RED", {230, 41, 55, 255}},
	{"MAROON", {190, 33, 55, 255}},
	{"GREEN", {0, 228, 48, 255}},
	{"LIME", {0, 158, 47, 255}},
	{"DARKGREEN", {0, 117, 44, 255}},
	{"SKYBLUE", {102, 191, 255, 255}},
	{"BLUE", {0, 121, 241, 255}},
	{"DARKBLUE", {0, 82, 172, 255}},
	{"PURPLE", {200, 122, 255, 255}},
	{"VIOLET", {135, 60, 190, 255}},
	{"DARKPURPLE", {112, 31, 126, 255}},
	{"BEIGE", {211, 176, 131, 255}},
	{"BROWN", {127, 106, 79, 255}},
	{"DARKBROWN", {76, 63, 47, 255}},
	{"WHITE", {255, 255, 255, 255}},
	{"BLACK", {0, 0, 0, 255}},
	{"BLANK", {0, 0, 0, 0}},
	{"MAGENTA", {255, 0, 255, 255}},
	{"RAYWHITE", {245, 245, 245, 255}},
};

static int
initmodule(JSContext *ctx, JSModuleDef *m)
{
	JSValue v;
	Color c;
	int i;

	if(JS_SetModuleExportList(ctx,m,functions,nelem(functions))<0)
		return -1;
	for(i=0;i<nelem(colors);i++){
		c=colors[i].color;
		v=JS_NewObject(ctx);
		if(JS_IsException(v)) return -1;
		if(JS_DefinePropertyValueStr(ctx,v,"r",JS_NewInt32(ctx,c.r),JS_PROP_ENUMERABLE)<0 ||
		   JS_DefinePropertyValueStr(ctx,v,"g",JS_NewInt32(ctx,c.g),JS_PROP_ENUMERABLE)<0 ||
		   JS_DefinePropertyValueStr(ctx,v,"b",JS_NewInt32(ctx,c.b),JS_PROP_ENUMERABLE)<0 ||
		   JS_DefinePropertyValueStr(ctx,v,"a",JS_NewInt32(ctx,c.a),JS_PROP_ENUMERABLE)<0){
			JS_FreeValue(ctx,v); return -1;
		}
		if(JS_SetModuleExport(ctx,m,colors[i].name,v)<0) return -1;
	}
	return 0;
}

int
rayinit(JSContext *ctx)
{
	JSModuleDef *m;
	JSClassDef def = {"raylib resource", finalize};
	int i;

	JS_NewClassID(&resourceid);
	if(JS_NewClass(JS_GetRuntime(ctx),resourceid,&def)<0){
		JS_ThrowOutOfMemory(ctx);
		return -1;
	}
	m=JS_NewCModule(ctx,"raylib",initmodule);
	if(m==nil)
		return -1;
	if(JS_AddModuleExportList(ctx,m,functions,nelem(functions))<0)
		return -1;
	for(i=0;i<nelem(colors);i++)
		if(JS_AddModuleExport(ctx,m,colors[i].name)<0)
			return -1;
	return 0;
}
