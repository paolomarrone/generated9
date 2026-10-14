#include <u.h>
#include <libc.h>
#include <keyboard.h>
#include <raylib.h>
#include <raygui.h>

static int input, mouseinput;

static void
keys(char *records, int n)
{
	if(write(input,records,n) != n) sysfatal("write keyboard: %r");
	sleep(30);
	PollInputEvents();
}

static void
mouse(int x, int y, int buttons)
{
	Vector2 origin;
	char buf[80];
	int n;

	origin = GetWindowPosition();
	n = snprint(buf,sizeof buf,"m%11d%11d%11d%11d ",
		(int)origin.x+x,(int)origin.y+y,buttons,1);
	if(write(mouseinput,buf,n) != n) sysfatal("write mouse: %r");
	sleep(30);
	PollInputEvents();
}

static void
paste(void)
{
	char file[64], text[12] = "AB", press[16], *p;
	Rune r;
	int fd;

	/* Keep the test clipboard separate from rio's snarf buffer. */
	snprint(file,sizeof file,"/tmp/raygui-snarf.%d",getpid());
	fd = create(file,ORDWR,0600);
	if(fd < 0 || bind(file,"/dev/snarf",MREPL) < 0) sysfatal("test clipboard: %r");
	close(fd);
	SetClipboardText("\303\251\360\237\220\21012345");
	keys("K\0",2);
	p = press;
	*p++ = 'k';
	r = Khome;
	p += runetochar(p,&r);
	*p++ = 0;
	keys(press,p-press);
	GuiTextBox((Rectangle){10,10,300,30},text,sizeof text,true);
	keys("K\0",2);
	p = press;
	*p++ = 'k';
	r = Kctl;
	p += runetochar(p,&r);
	*p++ = 'v';
	*p++ = 0;
	keys(press,p-press);
	GuiTextBox((Rectangle){10,10,300,30},text,sizeof text,true);
	if(strcmp(text,"\303\251\360\237\220\210123AB"))
		sysfatal("UTF-8 clipboard insertion/capacity: %s",text);
	keys("K\0",2);
	remove(file);
}

static void
picker(void)
{
	Rectangle bounds = {10,70,100,100};
	Color color = RED, hue;
	Image image;
	int x, result;

	x = bounds.x+bounds.width+GuiGetStyle(COLORPICKER,HUEBAR_PADDING)+2;
	BeginDrawing();
	ClearBackground(WHITE);
	GuiColorPicker(bounds,nil,&color);
	EndDrawing();
	image = LoadImageFromScreen();
	hue = GetImageColor(image,x,120);
	UnloadImage(image);
	mouse(35,100,1);
	BeginDrawing();
	ClearBackground(WHITE);
	result = GuiColorPicker(bounds,nil,&color);
	EndDrawing();
	if(result != RESULT_CHANGED) sysfatal("color picker did not report change");
	image = LoadImageFromScreen();
	if(!ColorIsEqual(hue,GetImageColor(image,x,120)))
		sysfatal("color picker hue bar vanished while dragging panel");
	UnloadImage(image);
	mouse(35,100,0);
}

void
main(void)
{
	char path[32], text[8] = {0};
	char chars[] = "c\303\251\0c\360\237\220\210\0cA\0cB\0";
	char back[] = "k\b\0";
	Image image;
	Color color;
	int kbd[2], mfd[2];

	if(rfork(RFNAMEG) < 0 || pipe(kbd) < 0 || pipe(mfd) < 0) sysfatal("keyboard namespace: %r");
	snprint(path,sizeof path,"/fd/%d",kbd[0]);
	if(bind(path,"/dev/kbd",MREPL) < 0) sysfatal("bind keyboard: %r");
	input = kbd[1];
	snprint(path,sizeof path,"/fd/%d",mfd[0]);
	if(bind(path,"/dev/mouse",MREPL) < 0) sysfatal("bind mouse: %r");
	mouseinput = mfd[1];
	InitWindow(400,200,"raygui tests");
	GuiLoadStyleDefault();
	GuiSetStyle(BUTTON,BASE_COLOR_NORMAL,0x123456ff);
	GuiLock();
	BeginDrawing();
	ClearBackground(WHITE);
	GuiButton((Rectangle){10,10,80,30},"Button");
	GuiDrawIcon(ICON_FILE_OPEN,110,10,2,RED);
	EndDrawing();
	image = LoadImageFromScreen();
	color = GetImageColor(image,14,14);
	if(color.r != 0x12 || color.g != 0x34 || color.b != 0x56)
		sysfatal("raygui style color: %d %d %d",color.r,color.g,color.b);
	UnloadImage(image);
	GuiUnlock();
	keys(chars,sizeof chars-1);
	GuiTextBox((Rectangle){10,60,300,30},text,sizeof text,true);
	if(strcmp(text,"\303\251\360\237\220\210A") != 0)
		sysfatal("UTF-8 textbox capacity: %s",text);
	keys(back,sizeof back-1);
	GuiTextBox((Rectangle){10,60,300,30},text,sizeof text,true);
	if(strcmp(text,"\303\251\360\237\220\210") != 0)
		sysfatal("textbox backspace: %s",text);
	keys("K\0",2);
	keys(back,sizeof back-1);
	GuiTextBox((Rectangle){10,60,300,30},text,sizeof text,true);
	if(strcmp(text,"\303\251") != 0)
		sysfatal("textbox Unicode backspace: %s",text);
	paste();
	picker();
	CloseWindow();
	print("PASS: raygui drawing, styles, UTF-8 editing and paste, buffer boundaries and color picker interaction\n");
	exits(nil);
}
