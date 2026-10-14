#ifndef IMAGE_H
#define IMAGE_H


#include "port.h"
#include "glext.h"
#include "base_image.h"

/*---------------------------------------------------------------------------*/

#define IF_MIPMAP 0x01

#include "font.h"

void   image_snap(const char *);

GLuint make_image_from_file(const char *, int);
GLuint make_image_from_font(int *, int *,
                            int *, int *, const char *, TTFont *, int);
GLuint make_texture(const void *, int, int, int, int);


/*---------------------------------------------------------------------------*/

#endif
