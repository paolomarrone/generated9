/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */


#include "port.h"
#include "glext.h"
#include "image.h"
#include "base_image.h"
#include "config.h"
#include "video.h"

#include "fs.h"
#include <draw.h>
#include <memdraw.h>
#include <tinygl.h>
#include "/sys/src/cmd/jpg/imagefile.h"

/*---------------------------------------------------------------------------*/

void image_snap(const char *filename)
{
    char *path, *err;
    Memimage *image;
    Biobuf out;
    uchar *pixels;
    int fd, w = video.window_w, h = video.window_h;

    path = smprint("%s/%s", fs_get_write_dir(), filename);
    fd = create(path, OWRITE, 0644);
    free(path);
    if(fd < 0) {
        fprint(2,"screenshot: %r\n");
        return;
    }
    pixels = malloc(w*h*4);
    image = allocmemimage(Rect(0,0,w,h), XRGB32);
    if(pixels == nil || image == nil) sysfatal("screenshot image: %r");
    tglread(pixels, w*h*4);
    loadmemimage(image, image->r, pixels, w*h*4);
    Binit(&out, fd, OWRITE);
    err = memwritepng(&out, image, nil);
    if(err) fprint(2,"screenshot: %s\n", err);
    Bterm(&out);
    close(fd);
    freememimage(image);
    free(pixels);
}

GLuint make_texture(const void *p, int w, int h, int b, int fl)
{
    static const GLenum format[] =
        { 0, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };

    GLuint o = 0;

    /* Scale the image as configured, or to fit the OpenGL limitations. */

#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
    int a = config_get_d(CONFIG_ANISO);
#endif
#ifdef GL_GENERATE_MIPMAP_SGIS
    int m = (fl & IF_MIPMAP) ? config_get_d(CONFIG_MIPMAP) : 0;
#endif
    int k = config_get_d(CONFIG_TEXTURES);
    int W = w;
    int H = h;

    GLint max = gli.max_texture_size;

    void *q = NULL;

    while (w / k > (int) max || h / k > (int) max)
        k *= 2;

    if (k > 1)
        q = image_scale(p, w, h, b, &W, &H, k);

    /* Generate and configure a new OpenGL texture. */

    glGenTextures(1, &o);
    glBindTexture(GL_TEXTURE_2D, o);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

#ifdef GL_GENERATE_MIPMAP_SGIS
    if (m)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
    }
#endif
#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
    if (a) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, a);
#endif

    /* Copy the image to an OpenGL texture. */

    glTexImage2D(GL_TEXTURE_2D, 0,
                 format[b], W, H, 0,
                 format[b], GL_UNSIGNED_BYTE, q ? q : p);

    if (q) free(q);


    return o;
}

/*
 * Load an image from the named file.  Return an OpenGL texture object.
 */
GLuint make_image_from_file(const char *filename, int fl)
{
    void  *p;
    int    w;
    int    h;
    int    b;
    GLuint o = 0;

    /* Load the image. */

    if ((p = image_load(filename, &w, &h, &b)))
    {
        o = make_texture(p, w, h, b, fl);
        free(p);
    }

    return o;
}

/*---------------------------------------------------------------------------*/

/*
 * Render the given  string using the given font.   Transfer the image
 * to a  surface of  power-of-2 size large  enough to fit  the string.
 * Return an OpenGL texture object.
 */
GLuint make_image_from_font(int *W, int *H, int *w, int *h,
	const char *text, TTFont *font, int fl)
{
	TTBitmap *bitmap;
	uchar *pixels, *p;
	GLuint texture;
	int width, height, pw, ph, x, y, i, j, sum;

	width = height = pw = ph = 0;
	texture = 0;
	if(font != nil && text != nil && *text){
		font_measure(font, text, &width, &height);
		image_size(&pw, &ph, width, height);
		bitmap = ttfrender(font, (char*)text, (char*)text+strlen(text), width*2+1, height*2+1, TTFLALIGN, nil);
		if(bitmap == nil)
			sysfatal("render text: %r");
		pixels = mallocz(pw*ph*4, 1);
		if(pixels == nil)
			sysfatal("text image: %r");
		for(y=0; y<height; y++)
			for(x=0; x<width; x++){
				sum = 0;
				for(j=0; j<2; j++)
					for(i=0; i<2; i++)
						sum += (bitmap->bit[(2*y+j)*bitmap->stride+(2*x+i)/8] >> (7-(2*x+i)%8)) & 1;
				p = pixels+4*((y+(ph-height)/2)*pw+x+(pw-width)/2);
				p[0] = p[1] = p[2] = 255;
				p[3] = (255*sum+2)/4;
			}
		ttffreebitmap(bitmap);
		texture = make_texture(pixels, pw, ph, 4, fl);
		free(pixels);
	}
	if(w) *w = width;
	if(h) *h = height;
	if(W) *W = pw;
	if(H) *H = ph;
	return texture;
}
