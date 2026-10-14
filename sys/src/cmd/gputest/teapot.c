#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <event.h>
#include <gpu.h>

enum { Header = 24 };

static Gpu gpu;
static u32int color, depth, indexbuf;
static int width = 640, height = 480, imagefd, nvertex, nindex;
static uchar *pixels;
static Image *picture;
static int resized, paused, measure;
static double angle;
static vlong submitns, readns, presentns;
static vlong loadns, drawns, flushns;
static char caption[128] = "Utah teapot  |  VirGL  |  space: pause  q: quit";

extern void teapotmesh(float (**)[8], ushort**, int*, int*);

static char vertexshader[] =
	"VERT\nDCL IN[0]\nDCL IN[1]\nDCL CONST[0..6]\n"
	"DCL OUT[0], POSITION\nDCL OUT[1], GENERIC[0]\n"
	"DP4 OUT[0].x, IN[0], CONST[0]\n"
	"DP4 OUT[0].y, IN[0], CONST[1]\n"
	"DP4 OUT[0].z, IN[0], CONST[2]\n"
	"DP4 OUT[0].w, IN[0], CONST[3]\n"
	"DP3 OUT[1].x, IN[1], CONST[4]\n"
	"DP3 OUT[1].y, IN[1], CONST[5]\n"
	"DP3 OUT[1].z, IN[1], CONST[6]\nEND\n";

static char fragmentshader[] =
	"FRAG\nDCL IN[0], GENERIC[0], PERSPECTIVE\n"
	"DCL OUT[0], COLOR\nDCL TEMP[0..2]\n"
	"IMM[0] FLT32 {0.408248, 0.816497, 0.408248, 0.0}\n"
	"IMM[1] FLT32 {0.243259, 0.486519, 0.839121, 1.0}\n"
	"IMM[2] FLT32 {0.14, 0.48, 0.68, 1.0}\n"
	"IMM[3] FLT32 {0.18, 0.82, 32.0, 0.7}\n"
	"DP3 TEMP[0].w, IN[0], IN[0]\n"
	"RSQ TEMP[0].w, TEMP[0].wwww\n"
	"MUL TEMP[0].xyz, IN[0], TEMP[0].wwww\n"
	"DP3 TEMP[1].x, TEMP[0], IMM[0]\n"
	"MAX TEMP[1].x, TEMP[1].xxxx, IMM[0].wwww\n"
	"MAD TEMP[1].x, TEMP[1].xxxx, IMM[3].yyyy, IMM[3].xxxx\n"
	"MUL TEMP[2].xyz, IMM[2], TEMP[1].xxxx\n"
	"DP3 TEMP[1].y, TEMP[0], IMM[1]\n"
	"MAX TEMP[1].y, TEMP[1].yyyy, IMM[0].wwww\n"
	"POW TEMP[1].y, TEMP[1].yyyy, IMM[3].zzzz\n"
	"MAD OUT[0].xyz, TEMP[1].yyyy, IMM[3].wwww, TEMP[2]\n"
	"MOV OUT[0].w, IMM[1].wwww\nEND\n";

static u32int
buffer(void *data, int n, int bind)
{
	char desc[96];
	u32int id;
	int fd;

	snprint(desc, sizeof(desc), "0 64 %d %d 1 1 1 0 0 0 %d", bind, n, n);
	id = gpuresource(&gpu, desc);
	fd = gpuopen(&gpu, id, OWRITE);
	if(write(fd, data, n) != n)
		sysfatal("write mesh: %r");
	close(fd);
	if(fprint(gpu.ctl, "upload %ud 0 0 0 0 %d 1 1 0 0 0", id, n) < 0)
		sysfatal("upload mesh: %r");
	return id;
}

static void
setup(ushort **indices)
{
	float (*vertices)[8];
	u32int *p, vertex, caps[65];
	int fd;

	gpuinit(&gpu, nil);
	fd = open("#∇/caps", OREAD);
	if(fd < 0 || readn(fd, caps, sizeof(caps)) != sizeof(caps))
		sysfatal("read gpu capabilities: %r");
	close(fd);
	/* VirGL v1 advertises depth formats in sampler; float4 vertices are baseline. */
	if((caps[17] & 1<<GpuBgra) == 0 || (caps[1] & 1<<GpuZ32) == 0)
		sysfatal("gpu lacks BGRA rendering or Z32 depth");
	teapotmesh(&vertices, indices, &nvertex, &nindex);
	vertex = buffer(vertices, nvertex*sizeof(*vertices), 16);
	indexbuf = buffer(*indices, nindex*sizeof(**indices), 32);
	free(vertices);
	p = gpuemit(&gpu, GpuCreate, GpuElements, 9);
	p[0] = 3;
	p[4] = GpuFloat4;
	p[5] = 16;
	p[8] = GpuFloat4;
	gpubind(&gpu, GpuElements, 3);
	p = gpuemit(&gpu, GpuVertices, 0, 3);
	p[0] = 32;
	p[2] = vertex;
	p = gpuemit(&gpu, GpuIndices, 0, 3);
	p[0] = indexbuf;
	p[1] = 2;
	gpushader(&gpu, 4, 0, vertexshader);
	gpushader(&gpu, 5, 1, fragmentshader);
	p = gpuemit(&gpu, GpuCreate, GpuBlend, 11);
	p[0] = 6;
	p[3] = 15U<<27;
	gpubind(&gpu, GpuBlend, 6);
	p = gpuemit(&gpu, GpuCreate, GpuDsa, 5);
	p[0] = 7;
	p[1] = 1 | 1<<1 | 1<<2;	/* depth test, depth write, LESS */
	gpubind(&gpu, GpuDsa, 7);
	p = gpuemit(&gpu, GpuCreate, GpuRasterizer, 9);
	p[0] = 8;
	p[1] = 2 | 1U<<29 | 1U<<30;
	p[2] = p[5] = gpufloat(1);
	gpubind(&gpu, GpuRasterizer, 8);
	gpusubmit(&gpu);
}

static void
target(int w, int h)
{
	char desc[96];
	u32int *p;
	int n;

	if(color != 0){
		gpuemit(&gpu, GpuFramebuffer, 0, 2);
		gpuemit(&gpu, GpuDestroy, GpuSurface, 1)[0] = 1;
		gpuemit(&gpu, GpuDestroy, GpuSurface, 1)[0] = 2;
		gpusubmit(&gpu);
		close(imagefd);
		if(fprint(gpu.ctl, "unref %ud", color) < 0 || fprint(gpu.ctl, "unref %ud", depth) < 0)
			sysfatal("release target: %r");
		free(pixels);
	}
	width = w;
	height = h;
	n = w*h*4;
	pixels = malloc(n);
	if(pixels == nil)
		sysfatal("allocate pixels: %r");
	snprint(desc, sizeof(desc), "2 %d 2 %d %d 1 1 0 0 1 %d", GpuBgra, w, h, n);
	color = gpuresource(&gpu, desc);
	snprint(desc, sizeof(desc), "2 %d 1 %d %d 1 1 0 0 1 %d", GpuZ32, w, h, n);
	depth = gpuresource(&gpu, desc);
	imagefd = gpuopen(&gpu, color, OREAD);
	p = gpuemit(&gpu, GpuCreate, GpuSurface, 5);
	p[0] = 1;
	p[1] = color;
	p[2] = GpuBgra;
	p = gpuemit(&gpu, GpuCreate, GpuSurface, 5);
	p[0] = 2;
	p[1] = depth;
	p[2] = GpuZ32;
	p = gpuemit(&gpu, GpuFramebuffer, 0, 3);
	p[0] = 1;
	p[1] = 2;
	p[2] = 1;
	p = gpuemit(&gpu, GpuViewport, 0, 7);
	p[1] = p[4] = gpufloat(w/2.);
	p[2] = p[5] = gpufloat(h/2.);
	p[3] = p[6] = gpufloat(.5);
	gpusubmit(&gpu);
}

static void
render(double a)
{
	float r[3][4], m[7][4];
	double c, s, ct, st, f;
	u32int *p;
	vlong start, now;
	int i, j, n;

	c = cos(a);
	s = sin(a);
	ct = cos(.4);
	st = sin(.4);
	r[0][0] = c; r[0][1] = 0; r[0][2] = s; r[0][3] = 0;
	r[1][0] = st*s; r[1][1] = ct; r[1][2] = -st*c; r[1][3] = 0;
	r[2][0] = -ct*s; r[2][1] = st; r[2][2] = ct*c; r[2][3] = -4.8;
	f = 1/tan(.35);
	for(j=0; j<4; j++){
		m[0][j] = r[0][j]*f*height/width;
		m[1][j] = r[1][j]*f;
		m[2][j] = r[2][j]*(-20.5/19.5);
		m[3][j] = -r[2][j];
	}
	m[2][3] -= 20./19.5;
	memmove(m+4, r, sizeof(r));
	p = gpuemit(&gpu, GpuConstants, 0, 2+7*4);
	for(i=0; i<7; i++)
		for(j=0; j<4; j++)
			p[2+i*4+j] = gpufloat(m[i][j]);
	p = gpuemit(&gpu, GpuClearbuf, 0, 8);
	p[0] = 5;	/* color and depth */
	p[1] = gpufloat(16./255);
	p[2] = gpufloat(24./255);
	p[3] = gpufloat(32./255);
	p[4] = gpufloat(1);
	p[6] = 0x3ff00000;	/* double 1.0 depth */
	p = gpuemit(&gpu, GpuDraw, 0, 12);
	p[1] = nindex;
	p[2] = 4;	/* triangles */
	p[3] = p[4] = 1;
	p[10] = nvertex-1;
	start = uptime();
	gpusubmit(&gpu);
	now = uptime();
	submitns += now-start;
	if(fprint(gpu.ctl, "download %ud 0 0 0 0 %d %d 1 0 0 0", color, width, height) < 0)
		sysfatal("download frame: %r");
	n = width*height*4;
	if(pread(imagefd, pixels, n, 0) != n)
		sysfatal("read frame: %r");
	readns += uptime()-now;
}

static void
save(char *path)
{
	Memimage *m;
	int fd;

	memimageinit();
	m = allocmemimage(Rect(0, 0, width, height), XRGB32);
	if(m == nil || loadmemimage(m, m->r, pixels, width*height*4) < 0)
		sysfatal("load snapshot: %r");
	fd = create(path, OWRITE, 0666);
	if(fd < 0 || writememimage(fd, m) < 0)
		sysfatal("save snapshot: %r");
	close(fd);
	freememimage(m);
}

static void
check(ushort *indices)
{
	uchar *first;
	ushort *reverse;
	int i, j, fd, n, changed, covered, bright, dark;

	render(.6);
	n = width*height*4;
	first = malloc(n);
	reverse = malloc(nindex*sizeof(*reverse));
	if(first == nil || reverse == nil)
		sysfatal("test memory: %r");
	memmove(first, pixels, n);
	covered = bright = dark = 0;
	for(i=0; i<n; i+=4){
		if(pixels[i] == 32 && pixels[i+1] == 24 && pixels[i+2] == 16)
			continue;
		covered++;
		bright += pixels[i] > 180;
		dark += pixels[i] < 90;
	}
	if(covered < width*height/10 || covered > width*height*3/4 || bright < 100 || dark < 100)
		sysfatal("teapot coverage or lighting: %d covered, %d bright, %d dark", covered, bright, dark);
	/* Reversing draw order must preserve occlusion, without relying on a golden image. */
	for(i=0; i<nindex; i+=3)
		for(j=0; j<3; j++)
			reverse[i+j] = indices[nindex-i-3+j];
	fd = gpuopen(&gpu, indexbuf, OWRITE);
	if(write(fd, reverse, nindex*2) != nindex*2
	|| fprint(gpu.ctl, "upload %ud 0 0 0 0 %d 1 1 0 0 0", indexbuf, nindex*2) < 0)
		sysfatal("reverse triangles: %r");
	close(fd);
	render(.6);
	changed = 0;
	for(i=0; i<n; i+=4)
		if(memcmp(first+i, pixels+i, 3) != 0)
			changed++;
	if(changed > 100)
		sysfatal("depth depends on triangle order: %d pixels", changed);
	render(1.4);
	changed = 0;
	for(i=0; i<n; i+=4)
		if(memcmp(first+i, pixels+i, 3) != 0)
			changed++;
	if(changed < width*height/20)
		sysfatal("rotation did not change the image");
	free(reverse);
	free(first);
	target(320, 240);
	render(.6);
	target(640, 480);
	render(.6);
	print("PASS: teapot geometry, lighting, depth order, rotation and target recreation (%d triangles)\n", nindex/3);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	resized = 1;
}

static void
resize(void)
{
	int w, h;

	w = Dx(screen->r);
	h = Dy(screen->r)-Header;
	w = w < 64 ? 64 : w > 2048 ? 2048 : w;
	h = h < 64 ? 64 : h > 2048 ? 2048 : h;
	if(picture == nil || width != w || height != h){
		target(w, h);
		freeimage(picture);
		picture = allocimage(display, Rect(0, 0, w, h), XRGB32, 0, DNofill);
		if(picture == nil)
			sysfatal("allocate picture: %r");
	}
	draw(screen, screen->r, display->black, nil, ZP);
	resized = 0;
}

static void
present(void)
{
	vlong start, last, now;
	Point at;
	Rectangle r;

	start = uptime();
	if(loadimage(picture, picture->r, pixels, width*height*4) < 0)
		sysfatal("present frame: %r");
	last = start;
	if(measure){
		if(flushimage(display, 0) < 0)
			sysfatal("flush pixels: %r");
		last = uptime();
		loadns += last-start;
	}
	at = addpt(screen->r.min, Pt((Dx(screen->r)-width)/2, Header));
	drawop(screen, rectaddpt(picture->r, at), picture, nil, ZP, S);
	r = screen->r;
	r.max.y = r.min.y+Header;
	draw(screen, r, display->black, nil, ZP);
	string(screen, addpt(r.min, Pt(6, 4)), display->white, ZP, font, caption);
	if(measure){
		if(flushimage(display, 0) < 0)
			sysfatal("flush drawing: %r");
		now = uptime();
		drawns += now-last;
		last = now;
	}
	if(flushimage(display, 1) < 0)
		sysfatal("flush frame: %r");
	now = uptime();
	flushns += now-last;
	presentns += now-start;
}

static void
stats(int frames, vlong elapsed)
{
	fprint(2, "%dx%d, %d triangles, %d frames: %.1f fps; submit %.2f ms, readback %.2f ms, present %.2f ms/frame\n",
		width, height, nindex/3, frames, frames*1e9/elapsed,
		submitns/1e6/frames, readns/1e6/frames, presentns/1e6/frames);
	if(measure)
		fprint(2, "load %.2f ms, draw %.2f ms, flush %.2f ms/frame\n",
			loadns/1e6/frames, drawns/1e6/frames, flushns/1e6/frames);
}

void
main(int argc, char **argv)
{
	ushort *indices;
	char *out;
	Event ev;
	ulong timer, e;
	vlong start, last, now, report;
	int headless, test, limit, frames, since;

	headless = test = limit = 0;
	out = nil;
	ARGBEGIN{
	case 't': test = headless = 1; break;
	case 'b': headless = 1; /* fall through */
	case 'n':
		limit = atoi(EARGF(sysfatal("missing frame count")));
		if(limit <= 0)
			sysfatal("frame count must be positive");
		break;
	case 'o': out = EARGF(sysfatal("missing image path")); headless = 1; break;
	default: sysfatal("usage: teapot [-t] [-b frames | -n frames] [-o image]");
	}ARGEND
	if(argc != 0)
		sysfatal("usage: teapot [-t] [-b frames | -n frames] [-o image]");
	setup(&indices);
	if(headless){
		target(width, height);
		if(test)
			check(indices);
		else{
			if(limit == 0)
				limit = 1;
			render(.6);	/* exclude shader compilation and first use from timing */
			submitns = readns = 0;
			start = uptime();
			for(frames=0; frames<limit; frames++)
				render(.6+frames*.02);
			stats(frames, uptime()-start);
		}
		if(out != nil)
			save(out);
	}else{
		measure = limit != 0;
		if(initdraw(nil, nil, "Utah teapot — VirGL") < 0)
			sysfatal("initdraw: %r");
		einit(Emouse|Ekeyboard);
		timer = etimer(0, 17);
		eresized(0);
		resize();
		render(.6);
		present();
		angle = .6;
		frames = since = 0;
		submitns = readns = presentns = 0;
		loadns = drawns = flushns = 0;
		start = last = report = uptime();
		for(;;){
			e = event(&ev);
			now = uptime();
			if(e == Ekeyboard){
				if(ev.kbdc == 'q' || ev.kbdc == 0x7f)
					break;
				if(ev.kbdc == ' '){
					paused = !paused;
					last = now;
					report = now;
					since = 0;
					snprint(caption, sizeof(caption), "Utah teapot  |  %s  |  space: pause  q: quit", paused ? "paused" : "VirGL");
					present();
				}
			}
			if(resized)
				resize();
			else if(e != timer || paused)
				continue;
			if(!paused)
				angle += (now-last)*.6e-9;
			last = now;
			render(angle);
			present();
			frames++;
			since++;
			if(now-report >= 1000000000LL){
				snprint(caption, sizeof(caption), "Utah teapot  |  %.1f fps  |  space: pause  q: quit", since*1e9/(now-report));
				since = 0;
				report = now;
			}
			if(limit != 0 && frames >= limit)
				break;
		}
		if(frames != 0)
			stats(frames, uptime()-start);
	}
	free(indices);
	close(imagefd);
	gpufini(&gpu);
	exits(nil);
}
