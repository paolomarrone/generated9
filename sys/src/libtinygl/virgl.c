/* Native VirGL rasterizer. Protocol encoding: see ../libgpu/UPSTREAM. */
#include "zgl.h"
#include <gpu.h>

enum { Nvertex = 8190, Ncomponent = 16+4*MAX_TEXTURE_UNITS };

typedef struct Texkey Texkey;
typedef struct Shader Shader;
struct Texkey {
	int format;
	GLTexenv env;
};
struct Shader {
	int valid;
	Texkey key[MAX_TEXTURE_UNITS];
};

typedef struct State State;
typedef struct Render Render;
struct State {
	int mode;
	u32int depth, blend, alpha, offset[2];
	int x, y, w, h;
	u32int raster, sprite, linewidth;
	int scissor[4];
};
struct Render {
	Gpu gpu;
	u32int color, depth, vertices;
	int imagefd, width, height, nvertex, vertexoffset;
	State state;
	float vertex[Nvertex][Ncomponent];
	Shader shader[64];
	u32int sampler[MAX_TEXTURE_UNITS][3];
	uint nextshader;
	u32int nextview;
	uvlong clock;
};

/* Texture state changes flush the batch before changing this state. */
void
gl_release_texture(GLContext *c, GLTexture *t)
{
	Render *r;
	Gpu *g;
	u32int *p;

	if(t->resource == 0)
		return;
	r = c->opaque;
	g = &r->gpu;
	p = gpuemit(g, GpuSamplerViews, 0, 2+MAX_TEXTURE_UNITS);
	p[0] = 1;
	gpuemit(g, GpuDestroy, GpuSamplerView, 1)[0] = t->view;
	gpusubmit(g);
	if(fprint(g->ctl, "unref %ud", t->resource) < 0)
		sysfatal("release GL texture: %r");
	t->resource = t->view = 0;
}

static GLTexture*
oldtexture(GLContext *c)
{
	GLTexture *t, *old;
	int i;

	old = nil;
	for(t=c->textures; t!=nil; t=t->next){
		if(t->resource == 0)
			continue;
		for(i=0; i<MAX_TEXTURE_UNITS; i++)
			if(c->texunit[i].enabled && c->texunit[i].texture == t)
				break;
		if(i == MAX_TEXTURE_UNITS && (old == nil || t->used < old->used))
			old = t;
	}
	return old;
}

static u32int
resource(GLContext *c, char *desc)
{
	Render *r;
	GLTexture *old;
	u32int id;

	r = c->opaque;
	/* CPU images own the contents; GPU copies can be evicted under pressure. */
	while((id = gputryresource(&r->gpu, desc)) == 0){
		old = oldtexture(c);
		if(old == nil)
			sysfatal("GL resource: %r");
		gl_release_texture(c, old);
	}
	return id;
}

static void
uploadtexture(GLContext *c, GLTexture *t, int levels)
{
	Render *r;
	Gpu *g;
	GLImage *im;
	u32int *p;
	char desc[128];
	int i, bytes, off, fd, n;

	r = c->opaque;
	t->used = ++r->clock;
	if(t->resource != 0)
		return;
	g = &r->gpu;
	bytes = 0;
	for(i=0; i<levels; i++)
		bytes += t->image[i]->width*t->image[i]->height*4;
	im = t->image[0];
	snprint(desc, sizeof(desc), "2 %d 8 %d %d 1 1 %d 0 0 %d",
		GpuBgra, im->width, im->height, levels-1, bytes);
	t->resource = resource(c, desc);
	t->view = r->nextview++;
	fd = gpuopen(g, t->resource, OWRITE);
	off = 0;
	for(i=0; i<levels; i++){
		im = t->image[i];
		n = im->width*im->height*4;
		if(pwrite(fd, im->pixels, n, off) != n
		|| fprint(g->ctl, "upload %ud %d 0 0 0 %d %d 1 %d %d 0",
			t->resource, i, im->width, im->height, off, im->width*4) < 0)
			sysfatal("upload GL texture: %r");
		off += n;
	}
	close(fd);
	p = gpuemit(g, GpuCreate, GpuSamplerView, 6);
	p[0] = t->view;
	p[1] = t->resource;
	p[2] = GpuBgra;
	p[4] = (levels-1)<<8;
	p[5] = 0 | 1<<3 | 2<<6 | 3<<9;
}

static int
wrap(int v)
{
	switch(v){
	case GL_REPEAT: return 0;
	case GL_CLAMP: return 1;
	default: return 2;
	}
}

static void
samplers(GLContext *c)
{
	Render *r;
	Gpu *g;
	GLTexture *t;
	u32int *p, view[MAX_TEXTURE_UNITS];
	int i, levels, min, mip;
	u32int s0;

	r = c->opaque;
	g = &r->gpu;
	memset(view, 0, sizeof(view));
	for(i=0; i<MAX_TEXTURE_UNITS; i++){
		if(!c->texunit[i].enabled)
			continue;
		t = c->texunit[i].texture;
		levels = gl_texture_levels(t);
		if(levels == 0)
			continue;
		uploadtexture(c, t, levels);
		view[i] = t->view;
		min = t->min == GL_LINEAR || t->min == GL_LINEAR_MIPMAP_NEAREST || t->min == GL_LINEAR_MIPMAP_LINEAR;
		mip = t->min == GL_NEAREST || t->min == GL_LINEAR ? 2 : t->min >= GL_NEAREST_MIPMAP_LINEAR;
		s0 = wrap(t->wrap[0]) | wrap(t->wrap[1])<<3 | 2<<6 | min<<9 | mip<<11 | (t->mag == GL_LINEAR)<<13;
		if(!r->sampler[i][0] || r->sampler[i][1] != s0 || r->sampler[i][2] != levels){
			p = gpuemit(g, GpuSamplerStates, 0, 3);
			p[0] = 1;
			p[1] = i;
			if(r->sampler[i][0]) gpuemit(g, GpuDestroy, GpuSamplerState, 1)[0] = 16+i;
			p = gpuemit(g, GpuCreate, GpuSamplerState, 9);
			p[0] = 16+i;
			p[1] = s0;
			p[4] = gpufloat(levels-1);
			r->sampler[i][0] = 1;
			r->sampler[i][1] = s0;
			r->sampler[i][2] = levels;
		}
	}
	p = gpuemit(g, GpuSamplerViews, 0, 2+MAX_TEXTURE_UNITS);
	p[0] = 1;
	for(i=0; i<MAX_TEXTURE_UNITS; i++) p[2+i] = view[i];
	p = gpuemit(g, GpuSamplerStates, 0, 2+MAX_TEXTURE_UNITS);
	p[0] = 1;
	for(i=0; i<MAX_TEXTURE_UNITS; i++) p[2+i] = view[i] ? 16+i : 0;
}

/* Generate the fixed-function fragment program; no guest shader compiler. */
static char*
combine(char *p, char *end, GLTexenv *e, int unit, int alpha)
{
	char src[32], *mask, *swiz;
	int i, n, op, neg;

	mask = alpha ? "w" : "xyz";
	op = e->combine[alpha];
	n = op == GL_REPLACE ? 1 : op == GL_INTERPOLATE ? 3 : 2;
	for(i=0; i<n; i++){
		switch(e->source[alpha][i]){
		case GL_TEXTURE: strcpy(src, "TEMP[1]"); break;
		case GL_PREVIOUS: strcpy(src, "TEMP[0]"); break;
		case GL_PRIMARY_COLOR: strcpy(src, "IN[0]"); break;
		default: snprint(src, sizeof(src), "CONST[%d]", unit); break;
		}
		swiz = e->operand[alpha][i] == GL_SRC_ALPHA || e->operand[alpha][i] == GL_ONE_MINUS_SRC_ALPHA ? ".wwww" : "";
		neg = e->operand[alpha][i] == GL_ONE_MINUS_SRC_ALPHA || e->operand[alpha][i] == GL_ONE_MINUS_SRC_COLOR;
		if(neg)
			p = seprint(p, end, "SUB TEMP[%d], IMM[0].xxxx, %s%s\n", 2+i, src, swiz);
		else
			p = seprint(p, end, "MOV TEMP[%d], %s%s\n", 2+i, src, swiz);
	}
	switch(op){
	case GL_REPLACE: p = seprint(p, end, "MOV TEMP[5].%s, TEMP[2]\n", mask); break;
	case GL_MODULATE: p = seprint(p, end, "MUL TEMP[5].%s, TEMP[2], TEMP[3]\n", mask); break;
	case GL_ADD: case GL_ADD_SIGNED:
		p = seprint(p, end, "ADD TEMP[5].%s, TEMP[2], TEMP[3]\n", mask);
		if(op == GL_ADD_SIGNED)
			p = seprint(p, end, "SUB TEMP[5].%s, TEMP[5], IMM[0].yyyy\n", mask);
		break;
	case GL_INTERPOLATE:
		p = seprint(p, end, "LRP TEMP[5].%s, TEMP[4], TEMP[2], TEMP[3]\n", mask);
		break;
	}
	if(e->scale[alpha] != 1)
		p = seprint(p, end, "MUL TEMP[5].%s, TEMP[5], %s\n", mask,
			e->scale[alpha] == 2 ? "IMM[0].wwww" : "IMM[1].xxxx");
	return p;
}

static void
fragment(GLContext *c)
{
	Render *r;
	Gpu *g;
	Texkey key[MAX_TEXTURE_UNITS];
	GLTexenv *e;
	GLTexture *t;
	Shader *s;
	char text[7000], *p, *end;
	u32int *cmd;
	int i, j, f;

	r = c->opaque;
	g = &r->gpu;
	memset(key, 0, sizeof(key));
	for(i=0; i<MAX_TEXTURE_UNITS; i++){
		if(!c->texunit[i].enabled)
			continue;
		t = c->texunit[i].texture;
		key[i].format = gl_texture_levels(t) == 0 ? -1 : t->image[0]->format;
		key[i].env = c->texunit[i].env;
		memset(&key[i].env.color, 0, sizeof(V4));
	}
	for(j=0; j<nelem(r->shader); j++)
		if(r->shader[j].valid && memcmp(key, r->shader[j].key, sizeof(key)) == 0)
			break;
	if(j == nelem(r->shader)){
		j = r->nextshader++ % nelem(r->shader);
		s = &r->shader[j];
		if(s->valid){
			cmd = gpuemit(g, GpuBindshader, 0, 2);
			cmd[1] = 1;
			gpuemit(g, GpuDestroy, GpuShader, 1)[0] = 64+j;
		}
		memmove(s->key, key, sizeof(key));
		s->valid = 1;
		end = text+sizeof(text);
		p = seprint(text, end, "FRAG\nDCL IN[0], COLOR[0], PERSPECTIVE\n");
		for(i=0; i<MAX_TEXTURE_UNITS; i++)
			if(key[i].format > 0)
				p = seprint(p, end, "DCL IN[%d], GENERIC[%d], PERSPECTIVE\nDCL SAMP[%d]\nDCL SVIEW[%d], 2D, FLOAT\n", i+1, i+1, i, i);
		p = seprint(p, end, "DCL OUT[0], COLOR\nDCL TEMP[0..5]\nDCL CONST[0..%d]\n"
			"IMM[0] FLT32 {1, 0.5, 0, 2}\nIMM[1] FLT32 {4, 0, 0, 1}\nMOV TEMP[0], IN[0]\n", MAX_TEXTURE_UNITS-1);
		for(i=0; i<MAX_TEXTURE_UNITS; i++){
			f = key[i].format;
			if(f == 0)
				continue;
			e = &key[i].env;
			if(f < 0){
				p = seprint(p, end, "MOV TEMP[1], IMM[0].zzzx\n");
				f = GL_RGBA;
			}else
				p = seprint(p, end, "TXP TEMP[1], IN[%d], SAMP[%d], 2D\n", i+1, i);
			switch(e->mode){
			case GL_COMBINE:
				p = combine(p, end, e, i, 0);
				p = combine(p, end, e, i, 1);
				p = seprint(p, end, "MOV_SAT TEMP[0], TEMP[5]\n");
				break;
			case GL_MODULATE:
				p = seprint(p, end, "MUL TEMP[0], TEMP[0], TEMP[1]\n");
				break;
			case GL_REPLACE:
				p = seprint(p, end, "MOV TEMP[0].%s, TEMP[1]\n",
					f == GL_ALPHA ? "w" : f == GL_RGB || f == GL_LUMINANCE ? "xyz" : "xyzw");
				break;
			case GL_DECAL:
				if(f == GL_RGB) p = seprint(p, end, "MOV TEMP[0].xyz, TEMP[1]\n");
				if(f == GL_RGBA) p = seprint(p, end, "LRP TEMP[0].xyz, TEMP[1].wwww, TEMP[1], TEMP[0]\n");
				break;
			case GL_ADD: case GL_BLEND:
				if(f != GL_ALPHA){
					if(e->mode == GL_ADD)
						p = seprint(p, end, "ADD_SAT TEMP[0].%s, TEMP[0], TEMP[1]\n", f == GL_INTENSITY ? "xyzw" : "xyz");
					else
						p = seprint(p, end, "LRP TEMP[0].%s, TEMP[1], CONST[%d], TEMP[0]\n", f == GL_INTENSITY ? "xyzw" : "xyz", i);
				}
				if(f != GL_INTENSITY) p = seprint(p, end, "MUL TEMP[0].w, TEMP[0], TEMP[1]\n");
				break;
			}
		}
		p = seprint(p, end, "MOV OUT[0], TEMP[0]\nEND\n");
		if(p >= end-1)
			gl_fatal_error("texture program too large");
		gpushader(g, 64+j, 1, text);
	}else{
		cmd = gpuemit(g, GpuBindshader, 0, 2);
		cmd[0] = 64+j;
		cmd[1] = 1;
	}
	cmd = gpuemit(g, GpuConstants, 0, 2+4*MAX_TEXTURE_UNITS);
	cmd[0] = 1;
	for(i=0; i<MAX_TEXTURE_UNITS; i++)
		for(j=0; j<4; j++)
			cmd[2+4*i+j] = gpufloat(c->texunit[i].env.color.v[j]);
}

/* Encode the vertex batch; glFlush submits the accumulated commands. */
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
	samplers(c);
	fragment(c);
	s = &r->state;
	n = r->nvertex*sizeof(r->vertex[0]);
	/* Inline writes are unsynchronized: reuse storage only after a fence. */
	if(n > sizeof(r->vertex)-r->vertexoffset){
		gpusubmit(g);
		r->vertexoffset = 0;
	}
	gpuwrite(g, r->vertices, r->vertexoffset, r->vertex, n);
	p = gpuemit(g, GpuVertices, 0, 3);
	p[0] = sizeof(r->vertex[0]);
	p[1] = r->vertexoffset;
	p[2] = r->vertices;
	r->vertexoffset += n;
	p = gpuemit(g, GpuCreate, GpuRasterizer, 9);
	p[0] = 9;
	p[1] = s->raster;
	p[3] = s->sprite;
	p[2] = gpufloat(1);
	p[5] = s->linewidth;
	p[6] = s->offset[0];
	p[7] = s->offset[1];
	gpubind(g, GpuRasterizer, 9);
	p = gpuemit(g, GpuCreate, GpuDsa, 5);
	p[0] = 7;
	p[1] = s->depth;
	p[4] = s->alpha;
	gpubind(g, GpuDsa, 7);
	p = gpuemit(g, GpuCreate, GpuBlend, 11);
	p[0] = 6;
	p[3] = s->blend;
	gpubind(g, GpuBlend, 6);
	p = gpuemit(g, GpuViewport, 0, 7);
	p[1] = gpufloat(s->w/2.);
	p[2] = gpufloat(s->h/2.);
	p[3] = p[6] = gpufloat(.5);
	p[4] = gpufloat(s->x+s->w/2.);
	p[5] = gpufloat(s->y+s->h/2.);
	p = gpuemit(g, GpuScissor, 0, 3);
	p[1] = s->scissor[0] | s->scissor[1]<<16;
	p[2] = s->scissor[2] | s->scissor[3]<<16;
	p = gpuemit(g, GpuDraw, 0, 12);
	p[1] = r->nvertex;
	p[2] = s->mode;
	p[4] = 1;
	p[10] = r->nvertex-1;
	gpubind(g, GpuBlend, 0);
	gpubind(g, GpuDsa, 0);
	gpuemit(g, GpuDestroy, GpuBlend, 1)[0] = 6;
	gpuemit(g, GpuDestroy, GpuDsa, 1)[0] = 7;
	gpubind(g, GpuRasterizer, 0);
	gpuemit(g, GpuDestroy, GpuRasterizer, 1)[0] = 9;
	r->nvertex = 0;
}

void
glFlush(void)
{
	GLContext *c;
	Render *r;

	c = gl_get_context();
	assert(!c->in_begin);
	r = c->opaque;
	gl_flush(c);
	gpusubmit(&r->gpu);
	r->vertexoffset = 0;
}

static int
blendfactor(int f)
{
	switch(f){
	case GL_ONE: return 1;
	case GL_SRC_COLOR: return 2;
	case GL_SRC_ALPHA: return 3;
	case GL_DST_ALPHA: return 4;
	case GL_DST_COLOR: return 5;
	case GL_SRC_ALPHA_SATURATE: return 6;
	case GL_ZERO: return 17;
	case GL_ONE_MINUS_SRC_COLOR: return 18;
	case GL_ONE_MINUS_SRC_ALPHA: return 19;
	case GL_ONE_MINUS_DST_ALPHA: return 20;
	case GL_ONE_MINUS_DST_COLOR: return 21;
	}
	gl_fatal_error("invalid blend factor");
	return 0;
}

static void
primitive(GLContext *c, int mode, GLVertex **v, int n)
{
	Render *r;
	State s;
	V4 *color, *back;
	float *p, f;
	int i, j, k, cull, src, dst;

	r = c->opaque;
	if(c->viewport.xsize == 0 || c->viewport.ysize == 0)
		return;
	memset(&s, 0, sizeof(s));
	s.mode = mode;
	s.depth = c->depth_test | c->depth_mask<<1 | (c->depth_func-GL_NEVER)<<2
		| c->alpha_enabled<<8 | (c->alpha_func-GL_NEVER)<<9;
	s.alpha = gpufloat(c->alpha_ref);
	src = blendfactor(c->blend_src);
	dst = blendfactor(c->blend_dst);
	s.blend = (u32int)c->color_mask<<27 | c->blend_enabled | src<<4 | dst<<9 | src<<17 | dst<<22;
	s.offset[0] = gpufloat(c->offset_units);
	s.offset[1] = gpufloat(c->offset_factor);
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
	s.linewidth = gpufloat(c->line_width > 64 ? 64 : c->line_width);
	for(i=0; i<4; i++){
		vlong v = c->scissor[i&1];
		if(i >= 2) v += c->scissor[i];
		s.scissor[i] = v < 0 ? 0 : v > 65535 ? 65535 : v;
	}
	s.sprite = 0;
	if(mode == 0){
		for(i=0; i<MAX_TEXTURE_UNITS; i++)
			if(c->point_sprite && c->texunit[i].replace) s.sprite |= 1<<(i+1);
	}
	s.raster = 2 | c->line_smooth<<26 | c->scissor_enabled<<14
		| (c->polygon_mode_front == GL_LINE ? 1 : c->polygon_mode_front == GL_POINT ? 2 : 0)<<10
		| (c->polygon_mode_back == GL_LINE ? 1 : c->polygon_mode_back == GL_POINT ? 2 : 0)<<12
		| (c->lighting_enabled && c->light_model_two_side)<<5 | (mode == 0)<<24 | (mode == 0 && c->point_sprite)<<7 | c->offset_fill<<20 | cull<<8 | (!c->current_front_face)<<15 | 1U<<29 | 1U<<30;
	if(r->nvertex+n > Nvertex || memcmp(&s, &r->state, sizeof(s)) != 0)
		gl_flush(c);
	r->state = s;
	for(i=0; i<n; i++){
		p = r->vertex[r->nvertex++];
		color = c->current_shade_model == GL_FLAT ? &c->flat_color : &v[i]->color;
		back = c->current_shade_model == GL_FLAT ? &c->flat_back_color : &v[i]->back_color;
		for(j=0; j<4; j++){
			p[j] = v[i]->pc.v[j];
			f = color->v[j];
			p[j+4] = f < 0 ? 0 : f > 1 ? 1 : f;
			f = back->v[j];
			p[Ncomponent-8+j] = f < 0 ? 0 : f > 1 ? 1 : f;
			p[Ncomponent-4+j] = v[i]->point_size;
			for(k=0; k<MAX_TEXTURE_UNITS; k++)
				p[8+4*k+j] = v[i]->texcoord[k].v[j];
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

/* Gallium's clear ignores GL masks and scissors. Draw a rectangle when needed. */
static void
clearrect(GLContext *c, int mask)
{
	GLContext clear;
	GLVertex v[4];
	Render *r;
	int i;

	r = c->opaque;
	clear = *c;
	clear.viewport.xmin = clear.viewport.ymin = 0;
	clear.viewport.xsize = r->width;
	clear.viewport.ysize = r->height;
	clear.depth_test = clear.depth_mask = (mask&GL_DEPTH_BUFFER_BIT) != 0;
	clear.depth_func = GL_ALWAYS;
	clear.color_mask = mask&GL_COLOR_BUFFER_BIT ? c->color_mask : 0;
	clear.blend_enabled = clear.alpha_enabled = clear.cull_face_enabled = 0;
	clear.offset_fill = clear.lighting_enabled = 0;
	clear.current_shade_model = GL_SMOOTH;
	clear.polygon_mode_front = clear.polygon_mode_back = GL_FILL;
	for(i=0; i<MAX_TEXTURE_UNITS; i++) clear.texunit[i].enabled = 0;
	memset(v, 0, sizeof v);
	for(i=0; i<4; i++){
		v[i].pc.X = i&1 ? 1 : -1;
		v[i].pc.Y = i&2 ? 1 : -1;
		v[i].pc.Z = 2*c->clear_depth-1;
		v[i].pc.W = 1;
		v[i].color = v[i].back_color = c->clear_color;
	}
	gl_draw_triangle(&clear, &v[0], &v[1], &v[2]);
	gl_draw_triangle(&clear, &v[1], &v[3], &v[2]);
	gl_flush(&clear);
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
	if(!c->depth_mask) mask &= ~GL_DEPTH_BUFFER_BIT;
	if(c->color_mask == 0) mask &= ~GL_COLOR_BUFFER_BIT;
	if(mask == 0)
		return;
	if(c->scissor_enabled || ((mask&GL_COLOR_BUFFER_BIT) && c->color_mask != 15)){
		clearrect(c, mask);
		return;
	}
	r = c->opaque;
	p = gpuemit(&r->gpu, GpuClearbuf, 0, 8);
	p[0] = (mask & GL_COLOR_BUFFER_BIT ? 4 : 0) | (mask & GL_DEPTH_BUFFER_BIT ? 1 : 0);
	for(i=0; i<4; i++)
		p[i+1] = gpufloat(c->clear_color.v[i]);
	depth.d = c->clear_depth;
	p[5] = depth.u[0];
	p[6] = depth.u[1];
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
	r->color = resource(c, desc);
	snprint(desc, sizeof(desc), "2 %d 1 %d %d 1 1 0 0 1 %d", GpuZ32, w, h, n);
	r->depth = resource(c, desc);
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
	char desc[128], *path, vs[1024], *v, *end;
	int fd, i;

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
	p = gpuemit(g, GpuCreate, GpuElements, 1+Ncomponent);
	p[0] = 3;
	for(i=0; i<Ncomponent/4; i++){
		p[1+4*i] = 16*i;
		p[4+4*i] = GpuFloat4;
	}
	gpubind(g, GpuElements, 3);
	end = vs+sizeof(vs);
	v = seprint(vs, end, "VERT\nDCL IN[0..%d]\nDCL OUT[0], POSITION\n", 3+MAX_TEXTURE_UNITS);
	v = seprint(v, end, "DCL OUT[1], COLOR[0]\nDCL OUT[6], BCOLOR[0]\n");
	for(i=0; i<MAX_TEXTURE_UNITS; i++)
		v = seprint(v, end, "DCL OUT[%d], GENERIC[%d]\n", i+2, i+1);
	v = seprint(v, end, "DCL OUT[7], PSIZE\n");
	for(i=0; i<4+MAX_TEXTURE_UNITS; i++)
		v = seprint(v, end, "MOV OUT[%d], IN[%d]\n", i, i);
	seprint(v, end, "END\n");
	gpushader(g, 4, 0, vs);
	r->nextview = 1024;
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
	glFlush();
	if(fprint(r->gpu.ctl, "download %ud 0 0 0 0 %d %d 1 0 0 0",
		r->color, r->width, r->height) < 0 || pread(r->imagefd, pixels, n, 0) != n)
		sysfatal("read GL frame: %r");
}

void
glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, GLvoid *out)
{
	GLContext *c;
	Render *r;
	uchar *pixels, *p;
	int i, l;

	c = gl_get_context();
	assert(!c->in_begin);
	r = c->opaque;
	if(x < 0 || y < 0 || w < 0 || h < 0 || x > r->width || y > r->height
	|| w > r->width-x || h > r->height-y)
		gl_fatal_error("read pixels outside framebuffer");
	if(w == 0 || h == 0) return;
	pixels = gl_malloc(r->width*r->height*4);
	tglread(pixels, r->width*r->height*4);
	/* Framebuffer luminance is the clamped RGB sum, unlike texture extraction. */
	if(fmt == GL_LUMINANCE || fmt == GL_LUMINANCE_ALPHA)
		for(p=pixels, i=0; i<r->width*r->height; i++, p+=4){
			l = p[0]+p[1]+p[2];
			p[2] = l > 255 ? 255 : l;
		}
	gl_pack(&c->pack, w, h, fmt, type, pixels+((r->height-1-y)*r->width+x)*4, -r->width*4, out);
	free(pixels);
}

void
tglclose(void)
{
	GLContext *c;
	Render *r;

	c = gl_get_context();
	assert(!c->in_begin && !c->compile_flag);
	r = c->opaque;
	glFlush();
	close(r->imagefd);
	gpufini(&r->gpu);
	glClose();
	free(r);
}
