/* generated9 native backend. See ../UPSTREAM for source and licensing. */
#define Image P9Image
#define Font P9Font
#define Rectangle P9Rectangle
#define Point P9Point
#include <draw.h>
#include <event.h>
#include <cursor.h>
#undef Image
#undef Font
#undef Rectangle
#undef Point
#include <keyboard.h>
#include <tinygl.h>
#include <plumb.h>

static struct {
	P9Image *image;
	uchar *pixels;
	char *snarf;
	ulong keys, window;
	int initialized, resize, buttons;
	P9Point mouse;
	Cursor invisible;
} platform;

static int
wctl(char *fmt, ...)
{
	va_list arg;
	char buf[128], err[ERRMAX];
	int fd, n, ok;

	va_start(arg, fmt);
	n = vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	fd = open("/dev/wctl", OWRITE);
	ok = fd >= 0 && write(fd, buf, n) == n;
	if(!ok){
		rerrstr(err, sizeof err);
		TraceLog(LOG_WARNING, "WINDOW: %s: %s", buf, err);
	}
	if(fd >= 0) close(fd);
	return ok;
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("raylib: getwindow: %r");
	platform.resize = 1;
}

static void
resize(void)
{
	int w, h;

	w = Dx(screen->r);
	h = Dy(screen->r);
	if(w < 1 || h < 1) return;
	tglresize(w, h);
	freeimage(platform.image);
	free(platform.pixels);
	platform.image = allocimage(display, Rect(0,0,w,h), XRGB32, 0, DNofill);
	platform.pixels = malloc(w*h*4);
	if(platform.image == nil || platform.pixels == nil)
		sysfatal("raylib: allocate frame: %r");
	CORE.Window.screen.width = CORE.Window.render.width = CORE.Window.currentFbo.width = w;
	CORE.Window.screen.height = CORE.Window.render.height = CORE.Window.currentFbo.height = h;
	rlSetFramebufferWidth(w);
	rlSetFramebufferHeight(h);
	SetupViewport(w, h);
	CORE.Window.resizedLastFrame = true;
	platform.resize = 0;
}

bool WindowShouldClose(void) { return !CORE.Window.ready || CORE.Window.shouldClose; }
void ToggleFullscreen(void) { TraceLog(LOG_WARNING, "WINDOW: Fullscreen is managed by rio"); }
void ToggleBorderlessWindowed(void) { TraceLog(LOG_WARNING, "WINDOW: Borders are managed by rio"); }
void MaximizeWindow(void) { TraceLog(LOG_WARNING, "WINDOW: Maximize is managed by rio"); }
void MinimizeWindow(void) { if(wctl("hide")) CORE.Window.flags |= FLAG_WINDOW_MINIMIZED|FLAG_WINDOW_HIDDEN; }
void RestoreWindow(void) { if(wctl("unhide")) CORE.Window.flags &= ~(FLAG_WINDOW_MINIMIZED|FLAG_WINDOW_HIDDEN); }
void SetWindowIcon(Image image) { USED(image); TraceLog(LOG_WARNING, "WINDOW: Icons are unavailable"); }
void SetWindowIcons(Image *images, int count) { USED(images,count); TraceLog(LOG_WARNING, "WINDOW: Icons are unavailable"); }

void
SetWindowState(unsigned int flags)
{
	if((flags & FLAG_WINDOW_HIDDEN) && wctl("hide")) CORE.Window.flags |= FLAG_WINDOW_HIDDEN;
	if(flags & ~(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_RESIZABLE))
		TraceLog(LOG_WARNING, "WINDOW: Unsupported window flags 0x%x", flags & ~(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_RESIZABLE));
	CORE.Window.flags |= flags & FLAG_WINDOW_RESIZABLE;
}

void
ClearWindowState(unsigned int flags)
{
	if((flags & FLAG_WINDOW_HIDDEN) && wctl("unhide"))
		CORE.Window.flags &= ~(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_MINIMIZED);
	/* rio windows remain resizable. */
}

void
SetWindowTitle(const char *title)
{
	int fd;

	if(title == nil) return;
	CORE.Window.title = title;
	fd = open("/dev/label", OWRITE);
	if(fd >= 0){
		write(fd, title, strlen(title));
		close(fd);
	}
}

void SetWindowPosition(int x, int y) { wctl("move -minx %d -miny %d", x, y); }
void SetWindowMonitor(int monitor) { if(monitor != 0) TraceLog(LOG_WARNING, "WINDOW: No such monitor"); }
void SetWindowMinSize(int width, int height) { USED(width,height); TraceLog(LOG_WARNING, "WINDOW: Size constraints are unavailable"); }
void SetWindowMaxSize(int width, int height) { USED(width,height); TraceLog(LOG_WARNING, "WINDOW: Size constraints are unavailable"); }
void SetWindowSize(int width, int height) { if(width > 0 && height > 0) wctl("resize -dx %d -dy %d", width+8, height+8); }
void SetWindowOpacity(float opacity) { USED(opacity); TraceLog(LOG_WARNING, "WINDOW: Opacity is unavailable"); }
void SetWindowFocused(void) { wctl("current"); }
void *GetWindowHandle(void) { return screen; }
int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { USED(monitor); return (Vector2){0,0}; }
int GetMonitorWidth(int monitor) { return monitor == 0 && display != nil ? Dx(display->image->r) : 0; }
int GetMonitorHeight(int monitor) { return monitor == 0 && display != nil ? Dy(display->image->r) : 0; }
int GetMonitorPhysicalWidth(int monitor) { USED(monitor); return 0; }
int GetMonitorPhysicalHeight(int monitor) { USED(monitor); return 0; }
int GetMonitorRefreshRate(int monitor) { USED(monitor); return 0; }
const char *GetMonitorName(int monitor) { return monitor == 0 ? "rio" : ""; }
Vector2 GetWindowPosition(void) { return (Vector2){screen->r.min.x,screen->r.min.y}; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){1,1}; }

void
SetClipboardText(const char *text)
{
	int fd;
	char err[ERRMAX];

	if(text == nil) return;
	fd = open("/dev/snarf", OWRITE|OTRUNC);
	if(fd < 0 || write(fd, text, strlen(text)) != strlen(text)){
		rerrstr(err, sizeof err);
		TraceLog(LOG_WARNING, "CLIPBOARD: Write: %s", err);
	}
	if(fd >= 0) close(fd);
}

const char *
GetClipboardText(void)
{
	char buf[4096], *p;
	int fd, n, len;

	free(platform.snarf);
	platform.snarf = strdup("");
	if(platform.snarf == nil) return nil;
	fd = open("/dev/snarf", OREAD);
	if(fd < 0) return nil;
	len = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		p = realloc(platform.snarf, len+n+1);
		if(p == nil){ close(fd); return nil; }
		platform.snarf = p;
		memmove(p+len, buf, n);
		len += n;
		p[len] = 0;
	}
	close(fd);
	return n < 0 ? nil : platform.snarf;
}

Image GetClipboardImage(void) { TraceLog(LOG_WARNING, "CLIPBOARD: Images are unavailable"); return (Image){nil,0,0,0,0}; }
void ShowCursor(void) { esetcursor(nil); CORE.Input.Mouse.cursorHidden = false; }
void HideCursor(void) { esetcursor(&platform.invisible); CORE.Input.Mouse.cursorHidden = true; }

void
SetMousePosition(int x, int y)
{
	emoveto(addpt(screen->r.min, Pt(x,y)));
	platform.mouse = Pt(x,y);
	CORE.Input.Mouse.currentPosition = (Vector2){x,y};
	CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

void EnableCursor(void) { CORE.Input.Mouse.cursorLocked = false; ShowCursor(); }
void DisableCursor(void) { SetMousePosition(GetScreenWidth()/2, GetScreenHeight()/2); CORE.Input.Mouse.cursorLocked = true; HideCursor(); }
void SetMouseCursor(int cursor) { if(cursor != MOUSE_CURSOR_DEFAULT && cursor != MOUSE_CURSOR_ARROW) TraceLog(LOG_WARNING, "INPUT: Only the default cursor is available"); else ShowCursor(); }

void
SwapScreenBuffer(void)
{
	int n;

	n = CORE.Window.render.width*CORE.Window.render.height*4;
	tglread(platform.pixels, n);
	if(loadimage(platform.image, platform.image->r, platform.pixels, n) < 0)
		sysfatal("raylib: load frame: %r");
	drawop(screen, rectaddpt(platform.image->r, screen->r.min), platform.image, nil, ZP, S);
	if(flushimage(display, 1) < 0)
		sysfatal("raylib: present frame: %r");
}

double GetTime(void) { return (uptime() - CORE.Time.base)*1e-9; }

void
OpenURL(const char *url)
{
	int fd;
	char err[ERRMAX];

	fd = plumbopen("send", OWRITE);
	if(fd < 0 || plumbsendtext(fd, "raylib", "web", nil, (char*)url) < 0){
		rerrstr(err, sizeof err);
		TraceLog(LOG_WARNING, "SYSTEM: Plumb URL: %s", err);
	}
	if(fd >= 0) close(fd);
}

int SetGamepadMappings(const char *mappings) { USED(mappings); TraceLog(LOG_WARNING, "INPUT: Gamepads are unavailable"); return 0; }
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration) { USED(gamepad,leftMotor,rightMotor,duration); TraceLog(LOG_WARNING, "INPUT: Gamepads are unavailable"); }

static int
keycode(Rune r)
{
	if(r >= 'a' && r <= 'z') return r-'a'+'A';
	if(r >= ' ' && r <= '~') return r;
	if(r > KF && r <= (KF|12)) return KEY_F1+r-(KF|1);
	switch(r){
	case Kesc: return KEY_ESCAPE;
	case '\n': return KEY_ENTER;
	case '\t': return KEY_TAB;
	case Kbs: return KEY_BACKSPACE;
	case Kins: return KEY_INSERT;
	case Kdel: return KEY_DELETE;
	case Kright: return KEY_RIGHT;
	case Kleft: return KEY_LEFT;
	case Kdown: return KEY_DOWN;
	case Kup: return KEY_UP;
	case Kpgup: return KEY_PAGE_UP;
	case Kpgdown: return KEY_PAGE_DOWN;
	case Khome: return KEY_HOME;
	case Kend: return KEY_END;
	case Kcaps: return KEY_CAPS_LOCK;
	case Knum: return KEY_NUM_LOCK;
	case Kscroll: return KEY_SCROLL_LOCK;
	case Kprint: return KEY_PRINT_SCREEN;
	case Kbreak: return KEY_PAUSE;
	case Kshift: return KEY_LEFT_SHIFT;
	case Kctl: return KEY_LEFT_CONTROL;
	case Kalt: return KEY_LEFT_ALT;
	case Kaltgr: return KEY_RIGHT_ALT;
	case Kmod4: return KEY_LEFT_SUPER;
	}
	return 0;
}

const char *
GetKeyName(int key)
{
	static char name[2];

	if(key < 32 || key > 126) return "";
	name[0] = key;
	return name;
}

static void
keyboard(char *p)
{
	uchar down[MAX_KEYBOARD_KEYS];
	Rune r;
	int i, key;

	if(*p == 'c'){
		chartorune(&r, p+1);
		if(r >= ' ' && r != Kdel && (r < KF || r >= Spec+0x100)
		&& CORE.Input.Keyboard.charPressedQueueCount < MAX_CHAR_PRESSED_QUEUE)
			CORE.Input.Keyboard.charPressedQueue[CORE.Input.Keyboard.charPressedQueueCount++] = r;
		key = keycode(r);
		if(key && CORE.Input.Keyboard.currentKeyState[key] && CORE.Input.Keyboard.previousKeyState[key])
			CORE.Input.Keyboard.keyRepeatInFrame[key] = 1;
		return;
	}
	if(*p != 'k' && *p != 'K') return;
	memset(down, 0, sizeof down);
	for(p++; *p;){
		p += chartorune(&r, p);
		key = keycode(r);
		if(key) down[key] = 1;
	}
	for(i=1; i<MAX_KEYBOARD_KEYS; i++){
		if(down[i] && !CORE.Input.Keyboard.currentKeyState[i]){
			if(CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
				CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount++] = i;
			if(i == CORE.Input.Keyboard.exitKey) CORE.Window.shouldClose = true;
		}
		CORE.Input.Keyboard.currentKeyState[i] = down[i];
	}
}

void
PollInputEvents(void)
{
	Event e;
	ulong kind, mask;
	char *p, *end, *z;
	P9Point pos, center;
	int i, waiting;

#if SUPPORT_GESTURES_SYSTEM
	UpdateGestures();
#endif
	CORE.Window.resizedLastFrame = false;
	CORE.Input.Keyboard.keyPressedQueueCount = 0;
	CORE.Input.Keyboard.charPressedQueueCount = 0;
	memset(CORE.Input.Keyboard.keyRepeatInFrame, 0, sizeof CORE.Input.Keyboard.keyRepeatInFrame);
	memmove(CORE.Input.Keyboard.previousKeyState, CORE.Input.Keyboard.currentKeyState, sizeof CORE.Input.Keyboard.currentKeyState);
	memmove(CORE.Input.Mouse.previousButtonState, CORE.Input.Mouse.currentButtonState, sizeof CORE.Input.Mouse.currentButtonState);
	memmove(CORE.Input.Touch.previousTouchState, CORE.Input.Touch.currentTouchState, sizeof CORE.Input.Touch.currentTouchState);
	CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
	CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
	CORE.Input.Mouse.currentWheelMove = (Vector2){0,0};
	mask = Emouse|platform.keys|platform.window;
	waiting = CORE.Window.eventWaiting;
	while(waiting || ecanread(mask)){
		waiting = 0;
		kind = eread(mask, &e);
		if(kind == platform.keys){
			end = (char*)e.data+e.n;
			for(p=(char*)e.data; p<end; p=z+1){
				z = memchr(p, 0, end-p);
				if(z == nil) sysfatal("raylib: incomplete keyboard record");
				keyboard(p);
			}
		}else if(kind == platform.window){
			char state[256], *fields[6];
			if(e.n >= sizeof state) sysfatal("raylib: invalid window state");
			memmove(state, e.data, e.n);
			state[e.n] = 0;
			if(tokenize(state, fields, nelem(fields)) != 6)
				sysfatal("raylib: invalid window state");
			if(strcmp(fields[4], "current") == 0) CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED;
			else CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;
			if(strcmp(fields[5], "hidden") == 0) CORE.Window.flags |= FLAG_WINDOW_HIDDEN;
			else CORE.Window.flags &= ~(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_MINIMIZED);
		}else if(kind == Emouse){
			pos = subpt(e.mouse.xy, screen->r.min);
			CORE.Input.Mouse.cursorOnScreen = ptinrect(e.mouse.xy, screen->r);
			if(CORE.Input.Mouse.cursorLocked){
				CORE.Input.Mouse.currentPosition.x += pos.x-platform.mouse.x;
				CORE.Input.Mouse.currentPosition.y += pos.y-platform.mouse.y;
			}else CORE.Input.Mouse.currentPosition = (Vector2){pos.x,pos.y};
			platform.mouse = pos;
			CORE.Input.Mouse.currentButtonState[0] = (e.mouse.buttons&1) != 0;
			CORE.Input.Mouse.currentButtonState[1] = (e.mouse.buttons&4) != 0;
			CORE.Input.Mouse.currentButtonState[2] = (e.mouse.buttons&2) != 0;
			i = e.mouse.buttons & ~platform.buttons;
			CORE.Input.Mouse.currentWheelMove.y += ((i&8) != 0) - ((i&16) != 0);
			CORE.Input.Touch.position[0] = CORE.Input.Mouse.currentPosition;
			CORE.Input.Touch.currentTouchState[0] = CORE.Input.Mouse.currentButtonState[0];
#if SUPPORT_GESTURES_SYSTEM && SUPPORT_MOUSE_GESTURES
			GestureEvent gesture = {0};
			gesture.touchAction = TOUCH_ACTION_MOVE;
			if((platform.buttons^e.mouse.buttons)&1)
				gesture.touchAction = e.mouse.buttons&1 ? TOUCH_ACTION_DOWN : TOUCH_ACTION_UP;
			gesture.pointCount = 1;
			gesture.position[0] = GetMousePosition();
			gesture.position[0].x /= GetScreenWidth();
			gesture.position[0].y /= GetScreenHeight();
			ProcessGestureEvent(gesture);
#endif
			platform.buttons = e.mouse.buttons;
		}
	}
	if(CORE.Input.Mouse.cursorLocked && !ptinrect(platform.mouse, insetrect(Rect(0,0,GetScreenWidth(),GetScreenHeight()),32))){
		center = Pt(GetScreenWidth()/2,GetScreenHeight()/2);
		emoveto(addpt(screen->r.min, center));
		platform.mouse = center;
	}
	if(platform.resize) resize();
}

int
InitPlatform(void)
{
	int fd;
	uint flags;
	char err[ERRMAX];

	if(!platform.initialized){
		if(initdraw(nil, nil, (char*)CORE.Window.title) < 0){
			rerrstr(err, sizeof err);
			TraceLog(LOG_WARNING, "WINDOW: initdraw: %s", err);
			return -1;
		}
		einit(Emouse);
		fd = open("/dev/kbd", OREAD);
		if(fd < 0) sysfatal("raylib: open keyboard: %r");
		platform.keys = estart(4, fd, 256);
		fd = open("/dev/wctl", OREAD);
		if(fd < 0) sysfatal("raylib: open window control: %r");
		platform.window = estart(8, fd, 256);
		platform.initialized = 1;
	}
	SetWindowSize(CORE.Window.screen.width, CORE.Window.screen.height);
	if(getwindow(display, Refnone) < 0) sysfatal("raylib: getwindow: %r");
	tglinit(nil, Dx(screen->r), Dy(screen->r));
	resize();
	CORE.Window.ready = true;
	CORE.Window.shouldClose = false;
	flags = CORE.Window.flags;
	CORE.Window.flags = FLAG_WINDOW_RESIZABLE;
	SetWindowState(flags);
	SetWindowTitle(CORE.Window.title);
	platform.buttons = 0;
	InitTimer();
	CORE.Storage.basePath = GetWorkingDirectory();
	return 0;
}

void
ClosePlatform(void)
{
	ShowCursor();
	tglclose();
	freeimage(platform.image);
	free(platform.pixels);
	free(platform.snarf);
	platform.image = nil;
	platform.pixels = nil;
	platform.snarf = nil;
	CORE.Window.ready = false;
}
