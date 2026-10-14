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
