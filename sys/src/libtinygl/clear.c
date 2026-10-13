#include "zgl.h"


void glopClearColor(GLContext *c,GLParam *p)
{
  c->clear_color.v[0]=p[1].f;
  c->clear_color.v[1]=p[2].f;
  c->clear_color.v[2]=p[3].f;
  c->clear_color.v[3]=p[4].f;
}
void glopClearDepth(GLContext *c,GLParam *p)
{
  c->clear_depth=p[1].f < 0 ? 0 : p[1].f > 1 ? 1 : p[1].f;
}


void glopClear(GLContext *c,GLParam *p)
{
  gl_clear(c,p[1].i);
}
