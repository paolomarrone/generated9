#include "zgl.h"

void
gl_fatal_error(char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprint(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	sysfatal("TinyGL: %s", buf);
}

void*
gl_malloc(int n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("TinyGL allocation: %r");
	return p;
}

void*
gl_zalloc(int n)
{
	void *p;

	p = gl_malloc(n);
	memset(p, 0, n);
	return p;
}

void
gl_free(void *p)
{
	free(p);
}

/* Do not silently accept operations that this backend cannot render. */
#define UNSUPPORTED(name) \
void glop##name(GLContext*, GLParam*) { gl_fatal_error(#name " is not supported"); }

UNSUPPORTED(InitNames)
UNSUPPORTED(PushName)
UNSUPPORTED(PopName)
UNSUPPORTED(LoadName)

void glSelectBuffer(int, unsigned int*) { gl_fatal_error("selection is not supported"); }
int glRenderMode(int mode)
{
	if(mode != GL_RENDER)
		gl_fatal_error("selection is not supported");
	return 0;
}
