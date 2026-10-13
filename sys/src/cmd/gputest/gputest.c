/*
 * VirGL triangle, based on virglrenderer's test_virgl_cmd.c and
 * testvirgl_encode.c. Copyright (C) 2014, 2015 Red Hat Inc.
 * See LICENSE and UPSTREAM in this directory.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <event.h>

#include <gpu.h>

enum {
	Width = 400,
	Height = 300,
};

static Gpu gpu;
static Image *picture;

static uchar*
render(void)
{
	static float vertices[3][8] = {
		{0, .75, 0, 1, 1, 0, 0, 1},
		{-.75, -.75, 0, 1, 0, 1, 0, 1},
		{.75, -.75, 0, 1, 0, 0, 1, 1},
	};
	char buf[128];
	u32int image, vertex, *p;
	uchar *pixels;
	int fd;

	gpuinit(&gpu, nil);
	snprint(buf, sizeof(buf), "2 %d 10 %d %d 1 1 0 0 1 %d",
		GpuBgra, Width, Height, Width*Height*4);
	image = gpuresource(&gpu, buf);
	snprint(buf, sizeof(buf), "0 64 16 %d 1 1 1 0 0 0 %d", sizeof(vertices), sizeof(vertices));
	vertex = gpuresource(&gpu, buf);
	fd = gpuopen(&gpu, vertex, OWRITE);
	if(write(fd, vertices, sizeof(vertices)) != sizeof(vertices))
		sysfatal("write vertices: %r");
	close(fd);
	if(fprint(gpu.ctl, "upload %ud 0 0 0 0 %d 1 1 0 0 0", vertex, sizeof(vertices)) < 0)
		sysfatal("upload vertices: %r");

	p = gpuemit(&gpu, GpuCreate, GpuSurface, 5);
	p[0] = 1;
	p[1] = image;
	p[2] = GpuBgra;
	p = gpuemit(&gpu, GpuFramebuffer, 0, 3);
	p[0] = 1;
	p[2] = 1;
	p = gpuemit(&gpu, GpuCreate, GpuElements, 9);
	p[0] = 2;
	p[4] = GpuFloat4;
	p[5] = 16;
	p[8] = GpuFloat4;
	gpubind(&gpu, GpuElements, 2);
	p = gpuemit(&gpu, GpuVertices, 0, 3);
	p[0] = sizeof(vertices[0]);
	p[2] = vertex;
	gpushader(&gpu, 3, 0,
		"VERT\nDCL IN[0]\nDCL IN[1]\n"
		"DCL OUT[0], POSITION\nDCL OUT[1], GENERIC[0]\n"
		"MOV OUT[0], IN[0]\nMOV OUT[1], IN[1]\nEND\n");
	gpushader(&gpu, 4, 1,
		"FRAG\nDCL IN[0], GENERIC[0], LINEAR\n"
		"DCL OUT[0], COLOR\nMOV OUT[0], IN[0]\nEND\n");
	p = gpuemit(&gpu, GpuCreate, GpuBlend, 11);
	p[0] = 5;
	p[3] = 15U<<27;
	gpubind(&gpu, GpuBlend, 5);
	p = gpuemit(&gpu, GpuCreate, GpuDsa, 5);
	p[0] = 6;
	gpubind(&gpu, GpuDsa, 6);
	p = gpuemit(&gpu, GpuCreate, GpuRasterizer, 9);
	p[0] = 7;
	p[1] = 2 | 1U<<29 | 1U<<30;
	p[2] = p[5] = gpufloat(1);
	gpubind(&gpu, GpuRasterizer, 7);
	p = gpuemit(&gpu, GpuViewport, 0, 7);
	p[1] = p[4] = gpufloat(Width/2.);
	p[2] = p[5] = gpufloat(Height/2.);
	p[3] = p[6] = gpufloat(.5);
	p = gpuemit(&gpu, GpuClearbuf, 0, 8);
	p[0] = 4;
	p[1] = gpufloat(16./255);
	p[2] = gpufloat(24./255);
	p[3] = gpufloat(32./255);
	p[4] = gpufloat(1);
	p = gpuemit(&gpu, GpuDraw, 0, 12);
	p[1] = 3;
	p[2] = 4;
	p[4] = 1;
	p[10] = 2;
	gpusubmit(&gpu);
	if(fprint(gpu.ctl, "download %ud 0 0 0 0 %d %d 1 0 0 0", image, Width, Height) < 0)
		sysfatal("download: %r");
	pixels = malloc(Width*Height*4);
	if(pixels == nil)
		sysfatal("malloc: %r");
	fd = gpuopen(&gpu, image, OREAD);
	if(readn(fd, pixels, Width*Height*4) != Width*Height*4)
		sysfatal("read pixels: %r");
	close(fd);
	gpufini(&gpu);
	return pixels;
}

static void
check(uchar *pixels)
{
	uchar *p;
	int i, colored;

	if(abs(pixels[0]-32) > 1 || abs(pixels[1]-24) > 1 || abs(pixels[2]-16) > 1)
		sysfatal("wrong clear color: %ud %ud %ud", pixels[2], pixels[1], pixels[0]);
	p = pixels+4*((Height/2)*Width+Width/2);
	if(p[0] < 45 || p[0] > 85 || p[1] < 45 || p[1] > 85 || p[2] < 110 || p[2] > 145)
		sysfatal("wrong interpolated center: %ud %ud %ud", p[2], p[1], p[0]);
	colored = 0;
	for(i=0; i<Width*Height; i++)
		if(pixels[4*i+2] != pixels[2] || pixels[4*i+1] != pixels[1] || pixels[4*i] != pixels[0])
			colored++;
	if(abs(colored-Width*Height*9/32) > Width)
		sysfatal("wrong triangle area: %d", colored);
	fprint(2, "PASS: VirGL clear, triangle, interpolation, readback (%d pixels)\n", colored);
}

void
eresized(int new)
{
	Point p;

	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	draw(screen, screen->r, display->black, nil, ZP);
	p = addpt(screen->r.min, Pt((Dx(screen->r)-Width)/2, (Dy(screen->r)-Height)/2));
	draw(screen, rectaddpt(picture->r, p), picture, nil, ZP);
	flushimage(display, 1);
}

void
main(int argc, char **argv)
{
	uchar *pixels;
	char *out;
	Memimage *m;
	Event ev;
	int test, fd;

	test = 0;
	out = nil;
	ARGBEGIN{
	case 't': test = 1; break;
	case 'o': out = EARGF(sysfatal("missing output file")); break;
	default: sysfatal("usage: gputest [-t] [-o image]");
	}ARGEND
	if(argc != 0)
		sysfatal("usage: gputest [-t] [-o image]");
	pixels = render();
	check(pixels);
	if(out != nil){
		memimageinit();
		m = allocmemimage(Rect(0, 0, Width, Height), ARGB32);
		if(m == nil || loadmemimage(m, m->r, pixels, Width*Height*4) < 0)
			sysfatal("load image: %r");
		fd = create(out, OWRITE, 0666);
		if(fd < 0 || writememimage(fd, m) < 0)
			sysfatal("write image: %r");
		close(fd);
		freememimage(m);
	}
	if(test || out != nil)
		exits(nil);
	if(initdraw(nil, nil, "generated9 VirGL") < 0)
		sysfatal("initdraw: %r");
	picture = allocimage(display, Rect(0, 0, Width, Height), ARGB32, 0, DNofill);
	if(picture == nil || loadimage(picture, picture->r, pixels, Width*Height*4) < 0)
		sysfatal("loadimage: %r");
	free(pixels);
	einit(Emouse|Ekeyboard);
	eresized(0);
	for(;;)
		if(event(&ev) == Ekeyboard && (ev.kbdc == 'q' || ev.kbdc == 0x7f))
			exits(nil);
}
