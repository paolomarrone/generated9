/* Native VirGL rasterizer. Protocol encoding: see ../libgpu/UPSTREAM. */
#include "zgl.h"
#include <gpu.h>

enum { Nvertex = 8190 };

typedef struct State State;
typedef struct Render Render;
struct State {
	int mode, depth;
	int x, y, w, h;
	u32int raster;
};
struct Render {
	Gpu gpu;
	u32int color, depth, vertices;
	int imagefd, vertexfd, width, height, nvertex;
	State state;
	float vertex[Nvertex][8];
};

static char vs[] =
	"VERT\nDCL IN[0..1]\nDCL OUT[0], POSITION\n"
	"DCL OUT[1], GENERIC[0]\n"
	"MOV OUT[0], IN[0]\nMOV OUT[1], IN[1]\nEND\n";
static char fs[] =
	"FRAG\nDCL IN[0], GENERIC[0], PERSPECTIVE\n"
	"DCL OUT[0], COLOR\nMOV OUT[0], IN[0]\nEND\n";

void
gl_flush(GLContext *c)
{
	Render *r;
	Gpu *g;
	State *s;
	u32int *p;
	int n;

	r = c->opaque;
	if(r->nvertex == 0)
		return;
	g = &r->gpu;
	s = &r->state;
	n = r->nvertex*sizeof(r->vertex[0]);
	if(pwrite(r->vertexfd, r->vertex, n, 0) != n
	|| fprint(g->ctl, "upload %ud 0 0 0 0 %d 1 1 0 0 0", r->vertices, n) < 0)
		sysfatal("upload GL vertices: %r");
	p = gpuemit(g, GpuCreate, GpuRasterizer, 9);
	p[0] = 9;
	p[1] = s->raster;
	p[2] = p[5] = gpufloat(1);
	gpubind(g, GpuRasterizer, 9);
	gpubind(g, GpuDsa, s->depth ? 7 : 8);
	p = gpuemit(g, GpuViewport, 0, 7);
	p[1] = gpufloat(s->w/2.);
	p[2] = gpufloat(s->h/2.);
	p[3] = p[6] = gpufloat(.5);
	p[4] = gpufloat(s->x+s->w/2.);
	p[5] = gpufloat(s->y+s->h/2.);
	p = gpuemit(g, GpuDraw, 0, 12);
	p[1] = r->nvertex;
	p[2] = s->mode;
	p[4] = 1;
	p[10] = r->nvertex-1;
	gpubind(g, GpuRasterizer, 0);
	gpuemit(g, GpuDestroy, GpuRasterizer, 1)[0] = 9;
	gpusubmit(g);
	r->nvertex = 0;
}

static void
primitive(GLContext *c, int mode, GLVertex **v, int n)
{
	Render *r;
	State s;
	V4 *color;
	float *p, f;
	int i, j, cull;

	r = c->opaque;
	if(c->viewport.xsize == 0 || c->viewport.ysize == 0)
		return;
	memset(&s, 0, sizeof(s));
	s.mode = mode;
	s.depth = c->depth_test;
	s.x = c->viewport.xmin;
	s.y = c->viewport.ymin;
	s.w = c->viewport.xsize;
	s.h = c->viewport.ysize;
	cull = 0;
	if(c->cull_face_enabled){
		if(c->current_cull_face != GL_BACK)
			cull |= 1;
		if(c->current_cull_face != GL_FRONT)
			cull |= 2;
	}
	/* Y_0_TOP resources invert readback; the viewport uses GL coordinates. */
	s.raster = 2 | cull<<8 | (!c->current_front_face)<<15 | 1U<<29 | 1U<<30;
	if(r->nvertex+n > Nvertex || memcmp(&s, &r->state, sizeof(s)) != 0)
		gl_flush(c);
	r->state = s;
	for(i=0; i<n; i++){
		p = r->vertex[r->nvertex++];
		color = c->current_shade_model == GL_FLAT ? &c->flat_color : &v[i]->color;
		for(j=0; j<4; j++){
			p[j] = v[i]->pc.v[j];
			f = color->v[j];
			p[j+4] = f < 0 ? 0 : f > 1 ? 1 : f;
		}
	}
}

void
gl_draw_triangle(GLContext *c, GLVertex *a, GLVertex *b, GLVertex *d)
{
	GLVertex *v[3];

	v[0] = a; v[1] = b; v[2] = d;
	primitive(c, 4, v, 3);
}

void
gl_draw_line(GLContext *c, GLVertex *a, GLVertex *b)
{
	GLVertex *v[2];

	v[0] = a; v[1] = b;
	primitive(c, 1, v, 2);
}

void
gl_draw_point(GLContext *c, GLVertex *a)
{
	primitive(c, 0, &a, 1);
}

void
gl_clear(GLContext *c, int mask)
{
	Render *r;
	u32int *p;
	union { double d; u32int u[2]; } depth;
	int i;

	if(mask & ~(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT))
		gl_fatal_error("unsupported clear mask");
	assert(!c->in_begin);
	gl_flush(c);
	if(mask == 0)
		return;
	r = c->opaque;
	p = gpuemit(&r->gpu, GpuClearbuf, 0, 8);
	p[0] = (mask & GL_COLOR_BUFFER_BIT ? 4 : 0) | (mask & GL_DEPTH_BUFFER_BIT ? 1 : 0);
	for(i=0; i<4; i++)
		p[i+1] = gpufloat(c->clear_color.v[i]);
	depth.d = c->clear_depth;
	p[5] = depth.u[0];
	p[6] = depth.u[1];
	gpusubmit(&r->gpu);
}

void
tglresize(int w, int h)
{
	GLContext *c;
	Render *r;
	Gpu *g;
	char desc[128];
	u32int *p;
	int n;

	if(w < 1 || h < 1 || w > 2048 || h > 2048)
		gl_fatal_error("target size must be between 1 and 2048");
	c = gl_get_context();
	assert(!c->in_begin && !c->compile_flag);
	r = c->opaque;
	if(w == r->width && h == r->height)
		return;
	gl_flush(c);
	g = &r->gpu;
	if(r->color != 0){
		gpuemit(g, GpuFramebuffer, 0, 2);
		gpuemit(g, GpuDestroy, GpuSurface, 1)[0] = 1;
		gpuemit(g, GpuDestroy, GpuSurface, 1)[0] = 2;
		gpusubmit(g);
		close(r->imagefd);
		if(fprint(g->ctl, "unref %ud", r->color) < 0
		|| fprint(g->ctl, "unref %ud", r->depth) < 0)
			sysfatal("release GL target: %r");
	}
	r->width = w;
	r->height = h;
	n = w*h*4;
	snprint(desc, sizeof(desc), "2 %d 2 %d %d 1 1 0 0 1 %d", GpuBgra, w, h, n);
	r->color = gpuresource(g, desc);
	snprint(desc, sizeof(desc), "2 %d 1 %d %d 1 1 0 0 1 %d", GpuZ32, w, h, n);
	r->depth = gpuresource(g, desc);
	r->imagefd = gpuopen(g, r->color, OREAD);
	p = gpuemit(g, GpuCreate, GpuSurface, 5);
	p[0] = 1;
	p[1] = r->color;
	p[2] = GpuBgra;
	p = gpuemit(g, GpuCreate, GpuSurface, 5);
	p[0] = 2;
	p[1] = r->depth;
	p[2] = GpuZ32;
	p = gpuemit(g, GpuFramebuffer, 0, 3);
	p[0] = 1;
	p[1] = 2;
	p[2] = 1;
	gpusubmit(g);
}

void
tglinit(char *device, int width, int height)
{
	Render *r;
	Gpu *g;
	u32int *p, caps[65];
	char desc[128], *path;
	int fd;

	if(gl_ctx != nil)
		gl_fatal_error("context already initialized");
	r = gl_zalloc(sizeof(*r));
	glInit(width, height);
	gl_ctx->opaque = r;
	g = &r->gpu;
	gpuinit(g, device);
	path = smprint("%s/caps", g->root);
	if(path == nil)
		sysfatal("gpu path: %r");
	fd = open(path, OREAD);
	free(path);
	if(fd < 0 || readn(fd, caps, sizeof(caps)) != sizeof(caps))
		sysfatal("read GPU capabilities: %r");
	close(fd);
	if((caps[17] & 1<<GpuBgra) == 0 || (caps[1] & 1<<GpuZ32) == 0)
		sysfatal("GPU lacks BGRA rendering or Z32 depth");
	snprint(desc, sizeof(desc), "0 64 16 %d 1 1 1 0 0 0 %d", sizeof(r->vertex), sizeof(r->vertex));
	r->vertices = gpuresource(g, desc);
	r->vertexfd = gpuopen(g, r->vertices, OWRITE);
	p = gpuemit(g, GpuCreate, GpuElements, 9);
	p[0] = 3;
	p[4] = GpuFloat4;
	p[5] = 16;
	p[8] = GpuFloat4;
	gpubind(g, GpuElements, 3);
	p = gpuemit(g, GpuVertices, 0, 3);
	p[0] = 32;
	p[2] = r->vertices;
	gpushader(g, 4, 0, vs);
	gpushader(g, 5, 1, fs);
	p = gpuemit(g, GpuCreate, GpuBlend, 11);
	p[0] = 6;
	p[3] = 15U<<27;
	gpubind(g, GpuBlend, 6);
	p = gpuemit(g, GpuCreate, GpuDsa, 5);
	p[0] = 7;
	p[1] = 1 | 1<<1 | 1<<2;	/* depth test, write, LESS */
	p = gpuemit(g, GpuCreate, GpuDsa, 5);
	p[0] = 8;
	gpubind(g, GpuDsa, 8);
	gpusubmit(g);
	tglresize(width, height);
}

void
tglread(void *pixels, int bytes)
{
	GLContext *c;
	Render *r;
	int n;

	c = gl_get_context();
	assert(!c->in_begin);
	r = c->opaque;
	n = r->width*r->height*4;
	if(pixels == nil || bytes < n)
		gl_fatal_error("readback buffer too small");
	gl_flush(c);
	if(fprint(r->gpu.ctl, "download %ud 0 0 0 0 %d %d 1 0 0 0",
		r->color, r->width, r->height) < 0 || pread(r->imagefd, pixels, n, 0) != n)
		sysfatal("read GL frame: %r");
}

void
tglclose(void)
{
	GLContext *c;
	Render *r;

	c = gl_get_context();
	assert(!c->in_begin && !c->compile_flag);
	r = c->opaque;
	gl_flush(c);
	close(r->vertexfd);
	close(r->imagefd);
	gpufini(&r->gpu);
	glClose();
	free(r);
}
