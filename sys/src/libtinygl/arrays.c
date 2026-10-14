/* Client arrays and buffer objects; see tinygl(2). */
#include "zgl.h"

static GLBuffer**
binding(GLContext *c, int target)
{
	assert(!c->in_begin);
	switch(target){
	case GL_ARRAY_BUFFER: return &c->array_buffer;
	case GL_ELEMENT_ARRAY_BUFFER: return &c->element_buffer;
	}
	gl_fatal_error("invalid buffer target");
	return nil;
}

static GLBuffer*
findbuffer(GLContext *c, GLuint name)
{
	GLBuffer *b;

	for(b=c->buffers; b != nil; b=b->next)
		if(b->name == name)
			return b;
	return nil;
}

static GLBuffer*
newbuffer(GLContext *c, GLuint name)
{
	GLBuffer *b;

	b = gl_zalloc(sizeof(*b));
	b->name = name;
	b->usage = GL_STATIC_DRAW;
	b->next = c->buffers;
	c->buffers = b;
	return b;
}

void
glGenBuffers(GLsizei n, GLuint *names)
{
	GLContext *c;
	GLuint name;

	c = gl_get_context();
	assert(!c->in_begin);
	if(n < 0 || (n != 0 && names == nil))
		gl_fatal_error("invalid buffer count");
	name = 1;
	while(n-- > 0){
		while(name != 0 && findbuffer(c, name) != nil)
			name++;
		if(name == 0)
			gl_fatal_error("buffer names exhausted");
		newbuffer(c, name);
		*names++ = name++;
	}
}

void
glBindBuffer(GLenum target, GLuint name)
{
	GLContext *c;
	GLBuffer *b, **slot;

	c = gl_get_context();
	slot = binding(c, target);
	b = nil;
	if(name != 0){
		b = findbuffer(c, name);
		if(b == nil)
			b = newbuffer(c, name);
		b->defined = 1;
	}
	*slot = b;
}

GLboolean
glIsBuffer(GLuint name)
{
	GLContext *c;
	GLBuffer *b;

	c = gl_get_context();
	assert(!c->in_begin);
	b = findbuffer(c, name);
	return b != nil && b->defined;
}

void
glDeleteBuffers(GLsizei n, const GLuint *names)
{
	GLContext *c;
	GLBuffer *b, **link;
	int i;

	c = gl_get_context();
	assert(!c->in_begin);
	if(n < 0 || (n != 0 && names == nil))
		gl_fatal_error("invalid buffer count");
	while(n-- > 0){
		for(link=&c->buffers; (b=*link) != nil; link=&b->next){
			if(b->name != *names)
				continue;
			if(c->array_buffer == b) c->array_buffer = nil;
			if(c->element_buffer == b) c->element_buffer = nil;
			for(i=0; i<Narray; i++)
				if(c->array[i].buffer == b)
					c->array[i].buffer = nil;
			*link = b->next;
			free(b->data);
			free(b);
			break;
		}
		names++;
	}
}

void
gl_free_buffers(GLContext *c)
{
	GLBuffer *b;

	while((b=c->buffers) != nil){
		c->buffers = b->next;
		free(b->data);
		free(b);
	}
}

static GLBuffer*
boundbuffer(int target)
{
	GLBuffer *b;

	b = *binding(gl_get_context(), target);
	if(b == nil)
		gl_fatal_error("no buffer bound");
	return b;
}

void
glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
	GLBuffer *b;
	uchar *p;

	b = boundbuffer(target);
	if(size < 0 || size > 0x7FFFFFFF)
		gl_fatal_error("invalid buffer size");
	if(usage != GL_STATIC_DRAW && usage != GL_DYNAMIC_DRAW && usage != GL_STREAM_DRAW)
		gl_fatal_error("unsupported buffer usage");
	p = size == 0 ? nil : gl_zalloc(size);
	if(size != 0 && data != nil)
		memmove(p, data, size);
	free(b->data);
	b->data = p;
	b->size = size;
	b->usage = usage;
}

static void
range(GLBuffer *b, uintptr off, uintptr size)
{
	if(off > b->size || size > b->size-off)
		gl_fatal_error("buffer access out of bounds");
}

void
glBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, const GLvoid *data)
{
	GLBuffer *b;

	b = boundbuffer(target);
	if(off < 0 || size < 0 || (size != 0 && data == nil))
		gl_fatal_error("invalid buffer update");
	range(b, off, size);
	if(size != 0)
		memmove(b->data+off, data, size);
}

void
glGetBufferParameteriv(GLenum target, GLenum pname, GLint *value)
{
	GLBuffer *b;

	b = boundbuffer(target);
	switch(pname){
	case GL_BUFFER_SIZE: *value = b->size; break;
	case GL_BUFFER_USAGE: *value = b->usage; break;
	default: gl_fatal_error("unsupported buffer query");
	}
}

static GLArray*
array(GLenum which)
{
	GLContext *c;
	int i;

	c = gl_get_context();
	assert(!c->in_begin);
	switch(which){
	case GL_VERTEX_ARRAY: i = ArrayVertex; break;
	case GL_COLOR_ARRAY: i = ArrayColor; break;
	case GL_NORMAL_ARRAY: i = ArrayNormal; break;
	case GL_TEXTURE_COORD_ARRAY: i = ArrayTexcoord+c->client_texture; break;
	default: gl_fatal_error("invalid client array"); return nil;
	}
	return &c->array[i];
}

void glEnableClientState(GLenum which) { array(which)->enabled = 1; }
void glDisableClientState(GLenum which) { array(which)->enabled = 0; }

static int
typesize(int type)
{
	switch(type){
	case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
	case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
	case GL_INT: case GL_UNSIGNED_INT: case GL_FLOAT: return 4;
	case GL_DOUBLE: return 8;
	}
	gl_fatal_error("unsupported array type");
	return 0;
}

static void
pointer(int which, int size, int type, int stride, const void *p)
{
	GLArray *a;
	int n;

	a = array(which);
	n = typesize(type);
	if(stride < 0)
		gl_fatal_error("negative array stride");
	a->size = size;
	a->type = type;
	a->stride = stride == 0 ? size*n : stride;
	a->pointer = p;
	a->buffer = gl_get_context()->array_buffer;
}

void
glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	if(size < 2 || size > 4)
		gl_fatal_error("vertex array size");
	if(type != GL_SHORT && type != GL_INT && type != GL_FLOAT && type != GL_DOUBLE)
		gl_fatal_error("vertex array type");
	pointer(GL_VERTEX_ARRAY, size, type, stride, p);
}

void
glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	if(size < 3 || size > 4)
		gl_fatal_error("color array size");
	pointer(GL_COLOR_ARRAY, size, type, stride, p);
}

void
glNormalPointer(GLenum type, GLsizei stride, const GLvoid *p)
{
	if(type != GL_BYTE && type != GL_SHORT && type != GL_INT && type != GL_FLOAT && type != GL_DOUBLE)
		gl_fatal_error("normal array type");
	pointer(GL_NORMAL_ARRAY, 3, type, stride, p);
}

void
glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	if(size < 1 || size > 4)
		gl_fatal_error("texture coordinate array size");
	if(type != GL_SHORT && type != GL_INT && type != GL_FLOAT && type != GL_DOUBLE)
		gl_fatal_error("texture coordinate array type");
	pointer(GL_TEXTURE_COORD_ARRAY, size, type, stride, p);
}

static double
component(const uchar *p, int type, int norm)
{
	union { schar b; uchar ub; short s; ushort us; int i; uint ui; float f; double d; } v;
	double x, scale;

	memmove(&v, p, typesize(type));
	scale = 1;
	switch(type){
	case GL_BYTE: x = v.b; scale = 127; break;
	case GL_UNSIGNED_BYTE: x = v.ub; scale = 255; break;
	case GL_SHORT: x = v.s; scale = 32767; break;
	case GL_UNSIGNED_SHORT: x = v.us; scale = 65535; break;
	case GL_INT: x = v.i; scale = 2147483647.; break;
	case GL_UNSIGNED_INT: x = v.ui; scale = 4294967295.; break;
	case GL_FLOAT: return v.f;
	case GL_DOUBLE: return v.d;
	default: x = 0; break;
	}
	if(norm){
		/* Classic GL conversion: (2c+1)/(2^b-1) for signed values. */
		if(type == GL_BYTE || type == GL_SHORT || type == GL_INT)
			x = (x+.5)/(scale+.5);
		else
			x /= scale;
	}
	return x;
}

static void
element(GLArray *a, uint i, int norm, float *v)
{
	const uchar *p;
	uintptr off, step;
	int j, n;

	n = typesize(a->type);
	step = (uintptr)i*a->stride;
	if(i != 0 && step/i != a->stride)
		gl_fatal_error("array offset overflow");
	off = (uintptr)a->pointer;
	if(step > (uintptr)-1-off)
		gl_fatal_error("array offset overflow");
	off += step;
	if(a->buffer != nil){
		range(a->buffer, off, a->size*n);
		p = a->buffer->data+off;
	}else{
		if(a->pointer == nil)
			gl_fatal_error("null client array");
		p = (uchar*)off;
	}
	v[0] = v[1] = v[2] = 0;
	v[3] = 1;
	for(j=0; j<a->size; j++)
		v[j] = component(p+j*n, a->type, norm);
}

static void
arrayelement(uint i)
{
	GLContext *c;
	GLArray *a;
	float v[4];
	int unit;

	c = gl_get_context();
	/* Expand values before compilation so display lists own their data. */
	a = &c->array[ArrayColor];
	if(a->enabled){
		element(a, i, 1, v);
		glColor4fv(v);
	}
	a = &c->array[ArrayNormal];
	if(a->enabled){
		element(a, i, 1, v);
		glNormal3fv(v);
	}
	for(unit=0; unit<MAX_TEXTURE_UNITS; unit++){
		a = &c->array[ArrayTexcoord+unit];
		if(a->enabled){
			element(a, i, 0, v);
			glMultiTexCoord4f(GL_TEXTURE0+unit, v[0], v[1], v[2], v[3]);
		}
	}
	a = &c->array[ArrayVertex];
	if(a->enabled){
		element(a, i, 0, v);
		glVertex4f(v[0], v[1], v[2], v[3]);
	}
}

void
glArrayElement(GLint i)
{
	if(i < 0)
		gl_fatal_error("negative array index");
	arrayelement(i);
}

void
glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	int i;

	assert(!gl_get_context()->in_begin);
	if(first < 0 || count < 0 || count > 0x7FFFFFFF-first)
		gl_fatal_error("invalid array range");
	glBegin(mode);
	for(i=0; i<count; i++)
		arrayelement(first+i);
	glEnd();
}

void
glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	GLContext *c;
	const uchar *p;
	uintptr n;
	int i, size;

	c = gl_get_context();
	assert(!c->in_begin);
	if(count < 0 || (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT))
		gl_fatal_error("invalid element array");
	size = typesize(type);
	n = (uintptr)count*size;
	if(n/size != count)
		gl_fatal_error("element array overflow");
	if(c->element_buffer != nil){
		range(c->element_buffer, (uintptr)indices, n);
		p = c->element_buffer->data+(uintptr)indices;
	}else{
		if(count != 0 && indices == nil)
			gl_fatal_error("null element array");
		p = indices;
	}
	glBegin(mode);
	for(i=0; i<count; i++)
		arrayelement(component(p+(uintptr)i*size, type, 0));
	glEnd();
}
