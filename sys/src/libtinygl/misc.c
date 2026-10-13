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
  case GL_DEPTH_TEST:
    c->depth_test = v;
    break;
  default:
    if (code>=GL_LIGHT0 && code<GL_LIGHT0+MAX_LIGHTS) {
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

  if (mode != GL_FILL) gl_fatal_error("only filled polygons are supported");
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
glopPolygonOffset(GLContext*, GLParam*)
{
  gl_fatal_error("polygon offset is not supported");
}
