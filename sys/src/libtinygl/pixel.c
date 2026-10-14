/* Pixel storage and conversion. Internal images use BGRA bytes. */
#include "zgl.h"

static int
pixelbytes(int format, int type)
{
	if((type == GL_UNSIGNED_SHORT_5_6_5 && format == GL_RGB)
	|| ((type == GL_UNSIGNED_SHORT_5_5_5_1 || type == GL_UNSIGNED_SHORT_4_4_4_4) && format == GL_RGBA))
		return 2;
	if(type != GL_UNSIGNED_BYTE)
		gl_fatal_error("unsupported pixel type");
	switch(format){
	case GL_ALPHA: case GL_LUMINANCE: return 1;
	case GL_LUMINANCE_ALPHA: return 2;
	case GL_RGB: return 3;
	case GL_RGBA: return 4;
	}
	gl_fatal_error("unsupported pixel format");
	return 0;
}

static uvlong
layout(GLPixelstore *s, int w, int h, int bytes, const void *pixels, uvlong *offset)
{
	uvlong stride, rows, left;

	stride = (uvlong)(s->row_length ? s->row_length : w)*bytes;
	stride = (stride+s->alignment-1) & ~(uvlong)(s->alignment-1);
	*offset = stride*s->skip_rows+(uvlong)s->skip_pixels*bytes;
	if(pixels != nil && w && h){
		left = (uintptr)-1-(uintptr)pixels;
		rows = (uvlong)s->skip_rows+h-1;
		if(stride && rows > left/stride)
			gl_fatal_error("pixel address overflow");
		left -= rows*stride;
		if(((uvlong)s->skip_pixels+w)*bytes > left)
			gl_fatal_error("pixel address overflow");
	}
	return stride;
}

GLImage*
gl_unpack(GLPixelstore *s, int w, int h, int fmt, int type, const void *pixels)
{
	GLImage *im;
	const uchar *src;
	uchar *dst;
	uvlong stride, off;
	ushort u;
	int bytes, x, y, r, g, b, a;

	bytes = pixelbytes(fmt, type);
	stride = layout(s, w, h, bytes, pixels, &off);
	im = gl_zalloc(sizeof(*im)+w*h*4);
	im->width = w;
	im->height = h;
	im->format = GL_RGBA;
	for(y=0; pixels != nil && y<h; y++){
		src = (uchar*)pixels+off+y*stride;
		dst = im->pixels+y*w*4;
		for(x=0; x<w; x++, src+=bytes, dst+=4){
			r = g = b = 0;
			a = 255;
			if(type == GL_UNSIGNED_BYTE){
				switch(fmt){
				case GL_ALPHA: a = src[0]; break;
				case GL_LUMINANCE: r = g = b = src[0]; break;
				case GL_LUMINANCE_ALPHA: r = g = b = src[0]; a = src[1]; break;
				case GL_RGBA: a = src[3]; /* fall through */
				case GL_RGB: r = src[0]; g = src[1]; b = src[2]; break;
				}
			}else{
				memmove(&u, src, 2);
				switch(type){
				case GL_UNSIGNED_SHORT_5_6_5:
					r = (u>>11)*255/31; g = ((u>>5)&63)*255/63; b = (u&31)*255/31;
					break;
				case GL_UNSIGNED_SHORT_5_5_5_1:
					r = (u>>11)*255/31; g = ((u>>6)&31)*255/31; b = ((u>>1)&31)*255/31; a = (u&1)*255;
					break;
				case GL_UNSIGNED_SHORT_4_4_4_4:
					r = (u>>12)*17; g = ((u>>8)&15)*17; b = ((u>>4)&15)*17; a = (u&15)*17;
					break;
				}
			}
			dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = a;
		}
	}
	return im;
}

void
gl_image_format(GLImage *im, int format)
{
	uchar *p;
	int n;

	im->format = format;
	for(p=im->pixels, n=im->width*im->height; n--; p+=4){
		switch(format){
		case GL_ALPHA: p[0] = p[1] = p[2] = 255; break;
		case GL_LUMINANCE: p[3] = 255; /* fall through */
		case GL_LUMINANCE_ALPHA: p[0] = p[1] = p[2]; break;
		case GL_INTENSITY: p[0] = p[1] = p[3] = p[2]; break;
		case GL_RGB: p[3] = 255; break;
		}
	}
}

void
gl_pack(GLPixelstore *s, int w, int h, int fmt, int type,
	const uchar *pixels, int pitch, void *out)
{
	const uchar *src;
	uchar *dst;
	uvlong stride, off;
	ushort u;
	int bytes, x, y;

	bytes = pixelbytes(fmt, type);
	if(w && h && out == nil) gl_fatal_error("nil pixel output");
	stride = layout(s, w, h, bytes, out, &off);
	for(y=0; y<h; y++){
		src = pixels+y*pitch;
		dst = (uchar*)out+off+y*stride;
		for(x=0; x<w; x++, src+=4, dst+=bytes){
			if(type == GL_UNSIGNED_BYTE){
				switch(fmt){
				case GL_ALPHA: dst[0] = src[3]; break;
				case GL_LUMINANCE: dst[0] = src[2]; break;
				case GL_LUMINANCE_ALPHA: dst[0] = src[2]; dst[1] = src[3]; break;
				case GL_RGBA: dst[3] = src[3]; /* fall through */
				case GL_RGB: dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; break;
				}
			}else{
				u = 0;
				switch(type){
				case GL_UNSIGNED_SHORT_5_6_5: u = (src[2]>>3)<<11 | (src[1]>>2)<<5 | src[0]>>3; break;
				case GL_UNSIGNED_SHORT_5_5_5_1: u = (src[2]>>3)<<11 | (src[1]>>3)<<6 | (src[0]>>3)<<1 | src[3]>>7; break;
				case GL_UNSIGNED_SHORT_4_4_4_4: u = (src[2]>>4)<<12 | (src[1]>>4)<<8 | (src[0]>>4)<<4 | src[3]>>4; break;
				}
				memmove(dst, &u, 2);
			}
		}
	}
}
