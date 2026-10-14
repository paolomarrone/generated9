#include "port.h"
#include "glext.h"

struct gl_info gli;

int
glext_init(void)
{
	glGetIntegerv(GL_MAX_TEXTURE_UNITS,&gli.max_texture_units);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&gli.max_texture_size);
	return 1;
}

void
glClipPlane4f_(GLenum plane, GLfloat a, GLfloat b, GLfloat c, GLfloat d)
{
	GLdouble v[4];
	v[0]=a; v[1]=b; v[2]=c; v[3]=d;
	glClipPlane(plane,v);
}
