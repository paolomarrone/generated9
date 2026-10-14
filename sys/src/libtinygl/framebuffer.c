/* Names own one reference; each framebuffer attachment owns another. */
#include "zgl.h"

static GLFramebuffer*
findframe(GLContext *c, GLuint name)
{
	GLFramebuffer *f;

	for(f=c->framebuffers; f!=nil; f=f->next)
		if(f->name == name)
			return f;
	return nil;
}

static GLRenderbuffer*
finddepth(GLContext *c, GLuint name)
{
	GLRenderbuffer *d;

	for(d=c->renderbuffers; d!=nil; d=d->next)
		if(d->name == name)
			return d;
	return nil;
}

static void
unrefdepth(GLContext *c, GLRenderbuffer *d)
{
	if(d != nil && --d->ref == 0){
		gl_release_depth(c, d);
		free(d);
	}
}

static void
checknames(int n, const GLuint *names)
{
	assert(!gl_get_context()->in_begin);
	if(n < 0 || (n != 0 && names == nil))
		gl_fatal_error("invalid object count");
}

static GLFramebuffer*
newframe(GLContext *c, GLuint name)
{
	GLFramebuffer *f;

	f = gl_zalloc(sizeof(*f));
	f->name = name;
	f->next = c->framebuffers;
	c->framebuffers = f;
	return f;
}

static GLRenderbuffer*
newdepth(GLContext *c, GLuint name)
{
	GLRenderbuffer *d;

	d = gl_zalloc(sizeof(*d));
	d->name = name;
	d->ref = 1;
	d->next = c->renderbuffers;
	c->renderbuffers = d;
	return d;
}

void
glGenFramebuffers(GLsizei n, GLuint *names)
{
	GLContext *c;
	GLuint name;

	c = gl_get_context();
	checknames(n, names);
	name = 1;
	while(n-- > 0){
		while(name != 0 && findframe(c, name) != nil)
			name++;
		if(name == 0)
			gl_fatal_error("framebuffer names exhausted");
		newframe(c, name);
		*names++ = name++;
	}
}

void
glGenRenderbuffers(GLsizei n, GLuint *names)
{
	GLContext *c;
	GLuint name;

	c = gl_get_context();
	checknames(n, names);
	name = 1;
	while(n-- > 0){
		while(name != 0 && finddepth(c, name) != nil)
			name++;
		if(name == 0)
			gl_fatal_error("renderbuffer names exhausted");
		newdepth(c, name);
		*names++ = name++;
	}
}

void
glBindFramebuffer(GLenum target, GLuint name)
{
	GLContext *c;
	GLFramebuffer *f;

	c = gl_get_context();
	assert(!c->in_begin);
	if(target != GL_FRAMEBUFFER)
		gl_fatal_error("invalid framebuffer target");
	gl_flush(c);
	f = nil;
	if(name != 0){
		f = findframe(c, name);
		if(f == nil)
			f = newframe(c, name);
		f->defined = 1;
	}
	c->framebuffer = f;
}

void
glBindRenderbuffer(GLenum target, GLuint name)
{
	GLContext *c;
	GLRenderbuffer *d;

	c = gl_get_context();
	assert(!c->in_begin);
	if(target != GL_RENDERBUFFER)
		gl_fatal_error("invalid renderbuffer target");
	d = nil;
	if(name != 0){
		d = finddepth(c, name);
		if(d == nil)
			d = newdepth(c, name);
		d->defined = 1;
	}
	c->renderbuffer = d;
}

GLboolean
glIsFramebuffer(GLuint name)
{
	GLFramebuffer *f = findframe(gl_get_context(), name);
	assert(!gl_get_context()->in_begin);
	return f != nil && f->defined;
}

GLboolean
glIsRenderbuffer(GLuint name)
{
	GLRenderbuffer *d = finddepth(gl_get_context(), name);
	assert(!gl_get_context()->in_begin);
	return d != nil && d->defined;
}

void
glDeleteFramebuffers(GLsizei n, const GLuint *names)
{
	GLContext *c;
	GLFramebuffer *f, **link;

	c = gl_get_context();
	checknames(n, names);
	gl_flush(c);
	while(n-- > 0){
		for(link=&c->framebuffers; (f=*link)!=nil; link=&f->next){
			if(f->name != *names)
				continue;
			if(c->framebuffer == f)
				c->framebuffer = nil;
			*link = f->next;
			gl_unref_texture(c, f->color);
			unrefdepth(c, f->depth);
			free(f);
			break;
		}
		names++;
	}
}

void
glDeleteRenderbuffers(GLsizei n, const GLuint *names)
{
	GLContext *c;
	GLRenderbuffer *d, **link;

	c = gl_get_context();
	checknames(n, names);
	gl_flush(c);
	while(n-- > 0){
		for(link=&c->renderbuffers; (d=*link)!=nil; link=&d->next){
			if(d->name != *names)
				continue;
			if(c->renderbuffer == d)
				c->renderbuffer = nil;
			if(c->framebuffer != nil && c->framebuffer->depth == d){
				c->framebuffer->depth = nil;
				unrefdepth(c, d);
			}
			*link = d->next;
			unrefdepth(c, d);
			break;
		}
		names++;
	}
}

void
glRenderbufferStorage(GLenum target, GLenum format, GLsizei w, GLsizei h)
{
	GLContext *c;
	GLRenderbuffer *d;

	c = gl_get_context();
	assert(!c->in_begin);
	d = c->renderbuffer;
	if(target != GL_RENDERBUFFER || d == nil)
		gl_fatal_error("no renderbuffer bound");
	if(w < 0 || h < 0 || w > MAX_TEXTURE_SIZE || h > MAX_TEXTURE_SIZE)
		gl_fatal_error("invalid renderbuffer size");
	if(format != GL_DEPTH_COMPONENT && format != GL_DEPTH_COMPONENT16
	&& format != GL_DEPTH_COMPONENT24 && format != GL_DEPTH_COMPONENT32
	&& format != GL_DEPTH_COMPONENT32F)
		gl_fatal_error("unsupported renderbuffer format");
	gl_flush(c);
	gl_release_depth(c, d);
	d->width = w;
	d->height = h;
	d->format = format;
}

static GLFramebuffer*
boundframe(GLenum target)
{
	GLContext *c = gl_get_context();
	assert(!c->in_begin);
	if(target != GL_FRAMEBUFFER || c->framebuffer == nil)
		gl_fatal_error("no framebuffer bound");
	return c->framebuffer;
}

void
glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint name, GLint level)
{
	GLContext *c = gl_get_context();
	GLFramebuffer *f = boundframe(target);
	GLTexture *t = nil;

	if(attachment != GL_COLOR_ATTACHMENT0 || textarget != GL_TEXTURE_2D || level != 0)
		gl_fatal_error("unsupported framebuffer texture attachment");
	if(name != 0){
		t = gl_find_texture(c, name);
		if(t == nil || !t->defined)
			gl_fatal_error("undefined framebuffer texture");
		t->ref++;
	}
	gl_flush(c);
	gl_unref_texture(c, f->color);
	f->color = t;
}

void
glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint name)
{
	GLContext *c = gl_get_context();
	GLFramebuffer *f = boundframe(target);
	GLRenderbuffer *d = nil;

	if(attachment != GL_DEPTH_ATTACHMENT || rbtarget != GL_RENDERBUFFER)
		gl_fatal_error("unsupported framebuffer renderbuffer attachment");
	if(name != 0){
		d = finddepth(c, name);
		if(d == nil || !d->defined)
			gl_fatal_error("undefined framebuffer renderbuffer");
		d->ref++;
	}
	gl_flush(c);
	unrefdepth(c, f->depth);
	f->depth = d;
}

int
gl_framebuffer_status(GLContext *c)
{
	GLFramebuffer *f = c->framebuffer;
	GLImage *im;

	if(f == nil)
		return GL_FRAMEBUFFER_COMPLETE;
	if(f->color == nil)
		return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
	im = f->color->image[0];
	if(im == nil || im->width == 0 || im->height == 0)
		return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
	if(im->format != GL_RGB && im->format != GL_RGBA)
		return GL_FRAMEBUFFER_UNSUPPORTED;
	if(f->depth != nil){
		if(f->depth->width == 0 || f->depth->height == 0)
			return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if(f->depth->width != im->width || f->depth->height != im->height)
			return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
	}
	return GL_FRAMEBUFFER_COMPLETE;
}

GLenum
glCheckFramebufferStatus(GLenum target)
{
	GLContext *c = gl_get_context();
	assert(!c->in_begin);
	if(target != GL_FRAMEBUFFER)
		gl_fatal_error("invalid framebuffer target");
	return gl_framebuffer_status(c);
}

void
glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *v)
{
	GLFramebuffer *f = boundframe(target);
	int type = GL_NONE, name = 0;

	if(attachment == GL_COLOR_ATTACHMENT0){
		if(f->color != nil){
			type = GL_TEXTURE;
			name = f->color->name;
		}
	}else if(attachment == GL_DEPTH_ATTACHMENT){
		if(f->depth != nil){
			type = GL_RENDERBUFFER;
			name = f->depth->name;
		}
	}else
		gl_fatal_error("unsupported framebuffer attachment query");
	switch(pname){
	case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: *v = type; break;
	case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: *v = name; break;
	case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: *v = 0; break;
	default: gl_fatal_error("unsupported framebuffer attachment query");
	}
}

void
glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *v)
{
	GLContext *c = gl_get_context();
	GLRenderbuffer *d = c->renderbuffer;
	assert(!c->in_begin);
	if(target != GL_RENDERBUFFER || d == nil)
		gl_fatal_error("no renderbuffer bound");
	switch(pname){
	case GL_RENDERBUFFER_WIDTH: *v = d->width; break;
	case GL_RENDERBUFFER_HEIGHT: *v = d->height; break;
	case GL_RENDERBUFFER_INTERNAL_FORMAT: *v = d->format; break;
	default: gl_fatal_error("unsupported renderbuffer query");
	}
}

void
gl_free_framebuffers(GLContext *c)
{
	GLuint name;

	while(c->framebuffers != nil){
		name = c->framebuffers->name;
		glDeleteFramebuffers(1, &name);
	}
	while(c->renderbuffers != nil){
		name = c->renderbuffers->name;
		glDeleteRenderbuffers(1, &name);
	}
}
