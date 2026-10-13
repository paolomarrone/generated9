/* Float client arrays, adapted from TinyGL; see UPSTREAM. */
#include "zgl.h"

enum { VertexArray = 1, ColorArray = 2, NormalArray = 4, TexcoordArray = 8 };

static int
arraybit(int array)
{
	switch(array){
	case GL_VERTEX_ARRAY: return VertexArray;
	case GL_COLOR_ARRAY: return ColorArray;
	case GL_NORMAL_ARRAY: return NormalArray;
	case GL_TEXTURE_COORD_ARRAY: return TexcoordArray;
	}
	gl_fatal_error("invalid client array");
	return 0;
}

void
glEnableClientState(GLenum array)
{
	GLContext *c = gl_get_context();

	assert(!c->in_begin);
	c->client_states |= arraybit(array);
}

void
glDisableClientState(GLenum array)
{
	GLContext *c = gl_get_context();

	assert(!c->in_begin);
	c->client_states &= ~arraybit(array);
}

static int
arraystride(int size, int type, int stride)
{
	GLContext *c = gl_get_context();

	assert(!c->in_begin);
	if(type != GL_FLOAT || stride < 0 || stride % sizeof(float) != 0)
		gl_fatal_error("arrays require floats and an aligned byte stride");
	return stride == 0 ? size : stride/sizeof(float);
}

void
glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	GLContext *c = gl_get_context();

	if(size < 2 || size > 4)
		gl_fatal_error("vertex array size");
	c->vertex_array_stride = arraystride(size, type, stride);
	c->vertex_array_size = size;
	c->vertex_array = (float*)p;
}

void
glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	GLContext *c = gl_get_context();

	if(size < 3 || size > 4)
		gl_fatal_error("color array size");
	c->color_array_stride = arraystride(size, type, stride);
	c->color_array_size = size;
	c->color_array = (float*)p;
}

void
glNormalPointer(GLenum type, GLsizei stride, const GLvoid *p)
{
	GLContext *c = gl_get_context();

	c->normal_array_stride = arraystride(3, type, stride);
	c->normal_array = (float*)p;
}

void
glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	GLContext *c = gl_get_context();

	if(size < 1 || size > 4)
		gl_fatal_error("texture coordinate array size");
	c->texcoord_array_stride = arraystride(size, type, stride);
	c->texcoord_array_size = size;
	c->texcoord_array = (float*)p;
}

void
glArrayElement(GLint i)
{
	GLContext *c;
	float *v;
	int states, size;

	c = gl_get_context();
	if(i < 0)
		gl_fatal_error("negative array index");
	states = c->client_states;
	/* Expand to values here, so display lists own a snapshot of the data. */
	if(states & ColorArray){
		assert(c->color_array != nil);
		v = c->color_array+(uintptr)i*c->color_array_stride;
		glColor4f(v[0], v[1], v[2], c->color_array_size == 4 ? v[3] : 1);
	}
	if(states & NormalArray){
		assert(c->normal_array != nil);
		glNormal3fv(c->normal_array+(uintptr)i*c->normal_array_stride);
	}
	if(states & TexcoordArray){
		assert(c->texcoord_array != nil);
		v = c->texcoord_array+(uintptr)i*c->texcoord_array_stride;
		size = c->texcoord_array_size;
		glTexCoord4f(v[0], size > 1 ? v[1] : 0, size > 2 ? v[2] : 0, size > 3 ? v[3] : 1);
	}
	if(states & VertexArray){
		assert(c->vertex_array != nil);
		v = c->vertex_array+(uintptr)i*c->vertex_array_stride;
		size = c->vertex_array_size;
		glVertex4f(v[0], v[1], size > 2 ? v[2] : 0, size > 3 ? v[3] : 1);
	}
}
