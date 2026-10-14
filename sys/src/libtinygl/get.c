#include "zgl.h"

void glGetIntegerv(int pname,int *params)
{
  GLContext *c=gl_get_context();

  assert(!c->in_begin);
  switch(pname) {
  case GL_ARRAY_BUFFER_BINDING:
    *params = c->array_buffer == nil ? 0 : c->array_buffer->name;
    break;
  case GL_ELEMENT_ARRAY_BUFFER_BINDING:
    *params = c->element_buffer == nil ? 0 : c->element_buffer->name;
    break;
  case GL_VIEWPORT:
    params[0]=c->viewport.xmin;
    params[1]=c->viewport.ymin;
    params[2]=c->viewport.xsize;
    params[3]=c->viewport.ysize;
    break;
  case GL_MAX_MODELVIEW_STACK_DEPTH:
    *params = MAX_MODELVIEW_STACK_DEPTH;
    break;
  case GL_MAX_PROJECTION_STACK_DEPTH:
    *params = MAX_PROJECTION_STACK_DEPTH;
    break;
  case GL_MAX_LIGHTS:
    *params = MAX_LIGHTS;
    break;
  case GL_MAX_TEXTURE_SIZE:
    *params = MAX_TEXTURE_SIZE;
    break;
  case GL_MAX_TEXTURE_UNITS: *params = MAX_TEXTURE_UNITS; break;
  case GL_ACTIVE_TEXTURE: *params = GL_TEXTURE0+c->active_texture; break;
  case GL_CLIENT_ACTIVE_TEXTURE: *params = GL_TEXTURE0+c->client_texture; break;
  case GL_TEXTURE_2D_BINDING: *params = c->texunit[c->active_texture].texture->name; break;
  case GL_MAX_TEXTURE_STACK_DEPTH:
    *params = MAX_TEXTURE_STACK_DEPTH;
    break;
  default:
    gl_fatal_error("glGet: option not implemented");
    break;
  }
}

void glGetFloatv(int pname, float *v)
{
  int i;
  int mnr = 0; /* just a trick to return the correct matrix */
  GLContext *c = gl_get_context();
  switch (pname) {
  case GL_TEXTURE_MATRIX:
    mnr+=1+c->active_texture;
  case GL_PROJECTION_MATRIX:
    mnr++;
  case GL_MODELVIEW_MATRIX:
    {
      float *p = &c->matrix_stack_ptr[mnr]->m[0][0];;
      for (i = 0; i < 4; i++) {
        *v++ = p[0];
        *v++ = p[4];
        *v++ = p[8];
        *v++ = p[12];
        p++;
      }
    }
    break;
  case GL_LINE_WIDTH:
    *v = 1.0f;
    break;
  case GL_LINE_WIDTH_RANGE:
    v[0] = v[1] = 1.0f;
    break;
  case GL_POINT_SIZE:
    *v = c->point_size;
    break;
  case GL_POINT_SIZE_RANGE:
    v[0] = 1; v[1] = 64;
    break;
  default:
    gl_fatal_error("glGetFloatv: option not implemented");
    break;
  }
}

const GLubyte*
glGetString(GLenum name)
{
  GLContext *c=gl_get_context();
  assert(!c->in_begin);
  switch(name) {
  case GL_VENDOR: return (GLubyte*)"generated9";
  case GL_RENDERER: return (GLubyte*)"TinyGL / VirGL";
  case GL_VERSION: return (GLubyte*)"TinyGL 0.4.1";
  case GL_EXTENSIONS: return (GLubyte*)"";
  }
  gl_fatal_error("invalid string query");
  return nil;
}
