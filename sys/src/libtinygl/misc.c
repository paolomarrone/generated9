#include "zgl.h"

void glopViewport(GLContext *c,GLParam *p)
{
  int x=p[1].i, y=p[2].i, w=p[3].i, h=p[4].i;

  if (w < 0 || h < 0 || w > 4096 || h > 4096 ||
      x < -4096 || y < -4096 || x > 4096 || y > 4096)
    gl_fatal_error("invalid viewport");
  c->viewport.xmin=x;
  c->viewport.ymin=y;
  c->viewport.xsize=w;
  c->viewport.ysize=h;
}

void glopEnableDisable(GLContext *c,GLParam *p)
{
  int code=p[1].i;
  int v=p[2].i;

  switch(code) {
  case GL_SCISSOR_TEST: c->scissor_enabled=v; break;
  case GL_LINE_SMOOTH: c->line_smooth=v; break;
  case GL_POINT_SPRITE: c->point_sprite=v; break;
  case GL_TEXTURE_GEN_S: case GL_TEXTURE_GEN_T: case GL_TEXTURE_GEN_R: case GL_TEXTURE_GEN_Q:
    c->texunit[c->active_texture].gen[code-GL_TEXTURE_GEN_S]=v;
    break;
  case GL_TEXTURE_2D:
    gl_flush(c);
    c->texunit[c->active_texture].enabled=v;
    break;
  case GL_CULL_FACE:
    c->cull_face_enabled=v;
    break;
  case GL_LIGHTING:
    c->lighting_enabled=v;
    c->matrix_model_projection_updated=1;
    break;
  case GL_COLOR_MATERIAL:
    c->color_material_enabled=v;
    gl_color_material(c);
    break;
  case GL_NORMALIZE:
    c->normalize_enabled=v;
    break;
  case GL_BLEND: c->blend_enabled=v; break;
  case GL_ALPHA_TEST: c->alpha_enabled=v; break;
  case GL_POLYGON_OFFSET_FILL: c->offset_fill=v; break;
  case GL_DEPTH_TEST:
    c->depth_test = v;
    break;
  default:
    if(code >= GL_CLIP_PLANE0 && code < GL_CLIP_PLANE0+MAX_CLIP_PLANES){
      if(v) c->clip_enabled |= 1<<(code-GL_CLIP_PLANE0);
      else c->clip_enabled &= ~(1<<(code-GL_CLIP_PLANE0));
    } else if (code>=GL_LIGHT0 && code<GL_LIGHT0+MAX_LIGHTS) {
      gl_enable_disable_light(c,code - GL_LIGHT0, v);
    } else {
      gl_fatal_error("unsupported capability %x",code);
    }
    break;
  }
}

void glopShadeModel(GLContext *c,GLParam *p)
{
  int code=p[1].i;
  c->current_shade_model=code;
}

void glopCullFace(GLContext *c,GLParam *p)
{
  int code=p[1].i;
  c->current_cull_face=code;
}

void glopFrontFace(GLContext *c,GLParam *p)
{
  int code=p[1].i;
  c->current_front_face=code;
}

void glopPolygonMode(GLContext *c,GLParam *p)
{
  int face=p[1].i;
  int mode=p[2].i;

  if (mode != GL_FILL && mode != GL_LINE && mode != GL_POINT)
    gl_fatal_error("invalid polygon mode");
  switch(face) {
  case GL_BACK:
    c->polygon_mode_back=mode;
    break;
  case GL_FRONT:
    c->polygon_mode_front=mode;
    break;
  case GL_FRONT_AND_BACK:
    c->polygon_mode_front=mode;
    c->polygon_mode_back=mode;
    break;
  default:
    assert(0);
  }
}

void glopHint(GLContext*,GLParam*)
{
}

void
glopPolygonOffset(GLContext *c, GLParam *p)
{
  c->offset_factor=p[1].f;
  c->offset_units=p[2].f;
}

void
glDepthFunc(GLenum func)
{
	GLParam p[2];

	p[0].op = OP_DepthFunc;
	p[1].i = func;
	gl_add_op(p);
}

void
glopDepthFunc(GLContext *c, GLParam *p)
{
	if(p[1].i < GL_NEVER || p[1].i > GL_ALWAYS)
		gl_fatal_error("invalid depth function");
	c->depth_func = p[1].i;
}

void
glDepthMask(GLboolean mask)
{
	GLParam p[2];

	p[0].op = OP_DepthMask;
	p[1].i = mask;
	gl_add_op(p);
}

void
glopDepthMask(GLContext *c, GLParam *p)
{
	c->depth_mask = p[1].i != 0;
}

void
glBlendFunc(GLenum src, GLenum dst)
{
	GLParam p[3];

	p[0].op = OP_BlendFunc;
	p[1].i = src;
	p[2].i = dst;
	gl_add_op(p);
}

static void
blendfactor(int f, int source)
{
	switch(f){
	case GL_ZERO: case GL_ONE: case GL_SRC_ALPHA: case GL_ONE_MINUS_SRC_ALPHA:
	case GL_DST_ALPHA: case GL_ONE_MINUS_DST_ALPHA:
	case GL_SRC_COLOR: case GL_ONE_MINUS_SRC_COLOR:
	case GL_DST_COLOR: case GL_ONE_MINUS_DST_COLOR:
		return;
	case GL_SRC_ALPHA_SATURATE:
		if(source) return;
	}
	gl_fatal_error("invalid blend factor");
}

void
glopBlendFunc(GLContext *c, GLParam *p)
{
	blendfactor(p[1].i, 1);
	blendfactor(p[2].i, 0);
	c->blend_src = p[1].i;
	c->blend_dst = p[2].i;
}

void
glAlphaFunc(GLenum func, GLclampf ref)
{
	GLParam p[3];

	p[0].op = OP_AlphaFunc;
	p[1].i = func;
	p[2].f = ref;
	gl_add_op(p);
}

void
glopAlphaFunc(GLContext *c, GLParam *p)
{
	if(p[1].i < GL_NEVER || p[1].i > GL_ALWAYS)
		gl_fatal_error("invalid alpha function");
	c->alpha_func = p[1].i;
	c->alpha_ref = p[2].f < 0 ? 0 : p[2].f > 1 ? 1 : p[2].f;
}

GLboolean
glIsEnabled(GLenum cap)
{
	GLContext *c;

	c = gl_get_context();
	assert(!c->in_begin);
	switch(cap){
	case GL_CULL_FACE: return c->cull_face_enabled;
	case GL_LIGHTING: return c->lighting_enabled;
	case GL_COLOR_MATERIAL: return c->color_material_enabled;
	case GL_NORMALIZE: return c->normalize_enabled;
	case GL_DEPTH_TEST: return c->depth_test;
	case GL_BLEND: return c->blend_enabled;
	case GL_ALPHA_TEST: return c->alpha_enabled;
	case GL_POLYGON_OFFSET_FILL: return c->offset_fill;
	case GL_SCISSOR_TEST: return c->scissor_enabled;
	case GL_LINE_SMOOTH: return c->line_smooth;
	case GL_POINT_SPRITE: return c->point_sprite;
	case GL_TEXTURE_GEN_S: case GL_TEXTURE_GEN_T: case GL_TEXTURE_GEN_R: case GL_TEXTURE_GEN_Q:
		return c->texunit[c->active_texture].gen[cap-GL_TEXTURE_GEN_S];
	case GL_TEXTURE_2D: return c->texunit[c->active_texture].enabled;
	}
	if(cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0+MAX_CLIP_PLANES)
		return (c->clip_enabled & (1<<(cap-GL_CLIP_PLANE0))) != 0;
	if(cap >= GL_LIGHT0 && cap < GL_LIGHT0+MAX_LIGHTS)
		return c->lights[cap-GL_LIGHT0].enabled;
	gl_fatal_error("unsupported capability");
	return 0;
}

void
glPointSize(GLfloat size)
{
	GLParam p[2];

	p[0].op = OP_PointSize;
	p[1].f = size;
	gl_add_op(p);
}

void
glopPointSize(GLContext *c, GLParam *p)
{
	if(p[1].f <= 0)
		gl_fatal_error("invalid point size");
	c->point_size = p[1].f;
}

void
glPointParameterf(GLenum name, GLfloat value)
{
	if(name == GL_POINT_DISTANCE_ATTENUATION)
		gl_fatal_error("point attenuation needs three values");
	glPointParameterfv(name,&value);
}

void
glPointParameterfv(GLenum name, const GLfloat *value)
{
	GLParam p[5];

	memset(p,0,sizeof(p));
	p[0].op = OP_PointParameter;
	p[1].i = name;
	p[2].f = value[0];
	if(name == GL_POINT_DISTANCE_ATTENUATION){
		p[3].f = value[1];
		p[4].f = value[2];
	}
	gl_add_op(p);
}

void
glopPointParameter(GLContext *c, GLParam *p)
{
	int i;

	if(p[2].f < 0)
		gl_fatal_error("negative point parameter");
	switch(p[1].i){
	case GL_POINT_SIZE_MIN: c->point_min = p[2].f; break;
	case GL_POINT_SIZE_MAX: c->point_max = p[2].f; break;
	case GL_POINT_FADE_THRESHOLD_SIZE: c->point_fade = p[2].f; break;
	case GL_POINT_DISTANCE_ATTENUATION:
		for(i=0; i<3; i++){
			if(p[2+i].f < 0) gl_fatal_error("negative point attenuation");
			c->point_atten[i] = p[2+i].f;
		}
		break;
	default: gl_fatal_error("unsupported point parameter");
	}
}

void
glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
	GLParam p[2];

	p[0].op = OP_ColorMask;
	p[1].i = (r!=0) | (g!=0)<<1 | (b!=0)<<2 | (a!=0)<<3;
	gl_add_op(p);
}

void
glopColorMask(GLContext *c, GLParam *p)
{
	c->color_mask = p[1].i;
}

void
glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
	GLParam p[5];

	p[0].op = OP_Scissor;
	p[1].i = x; p[2].i = y; p[3].i = w; p[4].i = h;
	gl_add_op(p);
}

void
glopScissor(GLContext *c, GLParam *p)
{
	int i;

	if(p[3].i < 0 || p[4].i < 0)
		gl_fatal_error("negative scissor size");
	for(i=0; i<4; i++) c->scissor[i] = p[i+1].i;
}

void
glLineWidth(GLfloat width)
{
	GLParam p[2];

	p[0].op = OP_LineWidth;
	p[1].f = width;
	gl_add_op(p);
}

void
glopLineWidth(GLContext *c, GLParam *p)
{
	if(!(p[1].f > 0)) gl_fatal_error("invalid line width");
	c->line_width = p[1].f;
}
