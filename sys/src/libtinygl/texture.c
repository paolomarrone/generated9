/* Texture objects, pixel unpacking and fixed-function texture state. */
#include "zgl.h"

static GLTexture*
findtexture(GLContext *c, GLuint name)
{
	GLTexture *t;

	for(t=c->textures; t!=nil; t=t->next)
		if(t->name == name)
			return t;
	return nil;
}

static GLTexture*
newtexture(GLContext *c, GLuint name)
{
	GLTexture *t;

	t = gl_zalloc(sizeof(*t));
	t->name = name;
	t->min = GL_NEAREST_MIPMAP_LINEAR;
	t->mag = GL_LINEAR;
	t->wrap[0] = t->wrap[1] = GL_REPEAT;
	t->next = c->textures;
	c->textures = t;
	return t;
}

void
gl_init_textures(GLContext *c)
{
	GLTexture *t;
	GLTexenv *e;
	int i, j;

	t = newtexture(c, 0);
	for(i=0; i<MAX_TEXTURE_UNITS; i++){
		c->texunit[i].texture = t;
		c->texunit[i].coord.W = 1;
		for(j=0; j<4; j++){
			c->texunit[i].genmode[j] = GL_EYE_LINEAR;
			if(j < 2){
				c->texunit[i].objectplane[j].v[j] = 1;
				c->texunit[i].eyeplane[j].v[j] = 1;
			}
		}
		e = &c->texunit[i].env;
		e->mode = GL_MODULATE;
		for(j=0; j<2; j++){
			e->combine[j] = GL_MODULATE;
			e->scale[j] = 1;
			e->source[j][0] = GL_TEXTURE;
			e->source[j][1] = GL_PREVIOUS;
			e->source[j][2] = GL_CONSTANT;
			e->operand[j][0] = e->operand[j][1] = j ? GL_SRC_ALPHA : GL_SRC_COLOR;
			e->operand[j][2] = GL_SRC_ALPHA;
		}
	}
	c->unpack_alignment = 4;
}

void
glGenTextures(int n, unsigned int *names)
{
	GLContext *c;
	GLuint name;

	c = gl_get_context();
	assert(!c->in_begin);
	if(n < 0 || (n != 0 && names == nil))
		gl_fatal_error("invalid texture count");
	name = 1;
	while(n-- > 0){
		while(name != 0 && findtexture(c, name) != nil)
			name++;
		if(name == 0)
			gl_fatal_error("texture names exhausted");
		newtexture(c, name);
		*names++ = name++;
	}
}

GLboolean
glIsTexture(GLuint name)
{
	GLTexture *t;
	GLContext *c;

	c = gl_get_context();
	assert(!c->in_begin);
	t = findtexture(c, name);
	return name != 0 && t != nil && t->defined;
}

void
glopBindTexture(GLContext *c, GLParam *p)
{
	GLTexture *t;

	if(p[1].i != GL_TEXTURE_2D)
		gl_fatal_error("unsupported texture target");
	t = findtexture(c, p[2].ui);
	if(t == nil)
		t = newtexture(c, p[2].ui);
	gl_flush(c);
	t->defined = 1;
	c->texunit[c->active_texture].texture = t;
}

static void
freeimages(GLTexture *t)
{
	int i;

	for(i=0; i<MAX_TEXTURE_LEVELS; i++)
		free(t->image[i]);
}

void
glDeleteTextures(int n, const unsigned int *names)
{
	GLContext *c;
	GLTexture *t, **link;
	int i;

	c = gl_get_context();
	assert(!c->in_begin);
	if(n < 0 || (n != 0 && names == nil))
		gl_fatal_error("invalid texture count");
	gl_flush(c);
	while(n-- > 0){
		for(link=&c->textures; (t=*link)!=nil; link=&t->next){
			if(t->name != *names || t->name == 0)
				continue;
			for(i=0; i<MAX_TEXTURE_UNITS; i++)
				if(c->texunit[i].texture == t)
					c->texunit[i].texture = findtexture(c, 0);
			gl_release_texture(c, t);
			*link = t->next;
			freeimages(t);
			free(t);
			break;
		}
		names++;
	}
}

void
gl_free_textures(GLContext *c)
{
	GLTexture *t;

	/* The native context owns and releases all remaining GPU resources. */
	while((t=c->textures) != nil){
		c->textures = t->next;
		freeimages(t);
		free(t);
	}
}

static int
unit(int texture)
{
	int i;

	i = texture-GL_TEXTURE0;
	if(i < 0 || i >= MAX_TEXTURE_UNITS)
		gl_fatal_error("invalid texture unit");
	return i;
}

void
glActiveTexture(GLenum texture)
{
	GLParam p[2];

	p[0].op = OP_ActiveTexture;
	p[1].i = texture;
	gl_add_op(p);
}

void
glopActiveTexture(GLContext *c, GLParam *p)
{
	c->active_texture = unit(p[1].i);
	if(c->matrix_mode >= 2)
		c->matrix_mode = 2+c->active_texture;
}

void
glClientActiveTexture(GLenum texture)
{
	GLContext *c;

	c = gl_get_context();
	assert(!c->in_begin);
	c->client_texture = unit(texture);
}

void
glMultiTexCoord4f(GLenum texture, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	GLParam p[6];

	p[0].op = OP_MultiTexCoord;
	p[1].i = texture;
	p[2].f = s; p[3].f = t; p[4].f = r; p[5].f = q;
	gl_add_op(p);
}

void
glopPixelStore(GLContext *c, GLParam *p)
{
	int n;

	assert(!c->in_begin);
	n = p[2].i;
	if(n < 0)
		gl_fatal_error("negative pixel storage parameter");
	switch(p[1].i){
	case GL_UNPACK_ALIGNMENT:
		if(n != 1 && n != 2 && n != 4 && n != 8)
			gl_fatal_error("invalid pixel alignment");
		c->unpack_alignment = n;
		break;
	case GL_UNPACK_ROW_LENGTH: c->unpack_row_length = n; break;
	case GL_UNPACK_SKIP_ROWS: c->unpack_skip_rows = n; break;
	case GL_UNPACK_SKIP_PIXELS: c->unpack_skip_pixels = n; break;
	default: gl_fatal_error("unsupported pixel storage parameter");
	}
}

static int
format(int f)
{
	switch(f){
	case 1: case GL_LUMINANCE8: return GL_LUMINANCE;
	case 2: case GL_LUMINANCE8_ALPHA8: return GL_LUMINANCE_ALPHA;
	case 3: case GL_RGB8: return GL_RGB;
	case 4: case GL_RGBA8: return GL_RGBA;
	case GL_ALPHA8: return GL_ALPHA;
	case GL_INTENSITY8: return GL_INTENSITY;
	case GL_ALPHA: case GL_LUMINANCE: case GL_LUMINANCE_ALPHA:
	case GL_RGB: case GL_RGBA: case GL_INTENSITY: return f;
	}
	gl_fatal_error("unsupported texture format");
	return 0;
}

static GLImage*
newimage(int w, int h, int f)
{
	GLImage *im;

	im = gl_zalloc(sizeof(*im)+w*h*4);
	im->width = w;
	im->height = h;
	im->format = f;
	return im;
}

void
glTexImage2D(int target, int level, int components, int w, int h, int border,
	int fmt, int type, void *pixels)
{
	GLContext *c;
	GLImage *im;
	GLParam p[3];
	uchar *src, *dst, r, g, b, a;
	uvlong stride, off, rows, bytesleft;
	int f, bytes, x, y;

	c = gl_get_context();
	assert(!c->in_begin);
	if(target != GL_TEXTURE_2D || border != 0 || type != GL_UNSIGNED_BYTE)
		gl_fatal_error("unsupported texture image");
	if(level < 0 || level >= MAX_TEXTURE_LEVELS || w < 0 || h < 0
	|| w > MAX_TEXTURE_SIZE>>level || h > MAX_TEXTURE_SIZE>>level)
		gl_fatal_error("invalid texture dimensions");
	f = format(components);
	switch(fmt){
	case GL_ALPHA: case GL_LUMINANCE: bytes = 1; break;
	case GL_LUMINANCE_ALPHA: bytes = 2; break;
	case GL_RGB: bytes = 3; break;
	case GL_RGBA: bytes = 4; break;
	default: gl_fatal_error("unsupported pixel format"); return;
	}
	stride = (uvlong)(c->unpack_row_length ? c->unpack_row_length : w)*bytes;
	stride = (stride+c->unpack_alignment-1) & ~(uvlong)(c->unpack_alignment-1);
	if(pixels != nil && w && h){
		bytesleft = (uintptr)-1-(uintptr)pixels;
		rows = (uvlong)c->unpack_skip_rows+h-1;
		if(stride && rows > bytesleft/stride)
			gl_fatal_error("pixel address overflow");
		bytesleft -= rows*stride;
		if(((uvlong)c->unpack_skip_pixels+w)*bytes > bytesleft)
			gl_fatal_error("pixel address overflow");
	}
	off = stride*c->unpack_skip_rows+(uvlong)c->unpack_skip_pixels*bytes;
	im = newimage(w, h, f);
	for(y=0; pixels != nil && y<h; y++){
		src = (uchar*)pixels+off+y*stride;
		dst = im->pixels+y*w*4;
		for(x=0; x<w; x++, src+=bytes, dst+=4){
			r = g = b = 0;
			a = 255;
			switch(fmt){
			case GL_ALPHA: a = src[0]; break;
			case GL_LUMINANCE: r = g = b = src[0]; break;
			case GL_LUMINANCE_ALPHA: r = g = b = src[0]; a = src[1]; break;
			case GL_RGBA: a = src[3]; /* fall through */
			case GL_RGB: r = src[0]; g = src[1]; b = src[2]; break;
			}
			switch(f){
			case GL_ALPHA: r = g = b = 255; break;
			case GL_LUMINANCE: a = 255; /* fall through */
			case GL_LUMINANCE_ALPHA: g = b = r; break;
			case GL_INTENSITY: g = b = a = r; break;
			case GL_RGB: a = 255; break;
			}
			dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = a;
		}
	}
	p[0].op = OP_TexImage2D;
	p[1].i = level;
	p[2].p = im;
	gl_add_op(p);
	if(c->compile_flag){
		im->next = c->compile_list->images;
		c->compile_list->images = im;
	}else
		free(im);
}

static void
mipmaps(GLTexture *t)
{
	GLImage *a, *b;
	int level, w, h, x, y, i, j, k, sum, n;

	a = t->image[0];
	for(level=1; a->width>1 || a->height>1; level++){
		w = a->width > 1 ? a->width/2 : 1;
		h = a->height > 1 ? a->height/2 : 1;
		b = newimage(w, h, a->format);
		for(y=0; y<h; y++)
			for(x=0; x<w; x++)
				for(k=0; k<4; k++){
					sum = n = 0;
					for(j=y*a->height/h; j<(y+1)*a->height/h; j++)
						for(i=x*a->width/w; i<(x+1)*a->width/w; i++){
							sum += a->pixels[4*(j*a->width+i)+k];
							n++;
						}
					b->pixels[4*(y*w+x)+k] = (sum+n/2)/n;
				}
		free(t->image[level]);
		t->image[level] = b;
		a = b;
	}
}

void
glopTexImage2D(GLContext *c, GLParam *p)
{
	GLTexture *t;
	GLImage *src, *im;
	int level;

	t = c->texunit[c->active_texture].texture;
	level = p[1].i;
	src = p[2].p;
	im = newimage(src->width, src->height, src->format);
	memmove(im->pixels, src->pixels, src->width*src->height*4);
	gl_flush(c);
	gl_release_texture(c, t);
	free(t->image[level]);
	t->image[level] = im;
	if(level == 0 && t->generate && im->width && im->height)
		mipmaps(t);
}

int
gl_texture_levels(GLTexture *t)
{
	GLImage *im;
	int w, h, f, i;

	im = t->image[0];
	if(im == nil || im->width == 0 || im->height == 0)
		return 0;
	w = im->width;
	h = im->height;
	f = im->format;
	if(t->min == GL_NEAREST || t->min == GL_LINEAR)
		return 1;
	for(i=1; w>1 || h>1; i++){
		w = w>1 ? w/2 : 1;
		h = h>1 ? h/2 : 1;
		im = t->image[i];
		if(im == nil || im->width != w || im->height != h || im->format != f)
			return 0;
	}
	return i;
}

void
glopTexParameter(GLContext *c, GLParam *p)
{
	GLTexture *t;
	int n;

	if(p[1].i != GL_TEXTURE_2D)
		gl_fatal_error("unsupported texture target");
	t = c->texunit[c->active_texture].texture;
	n = p[3].i;
	gl_flush(c);
	switch(p[2].i){
	case GL_TEXTURE_MIN_FILTER:
		if(n != GL_NEAREST && n != GL_LINEAR && (n < GL_NEAREST_MIPMAP_NEAREST || n > GL_LINEAR_MIPMAP_LINEAR))
			gl_fatal_error("invalid texture minification filter");
		if(t->min != n) gl_release_texture(c, t);
		t->min = n;
		break;
	case GL_TEXTURE_MAG_FILTER:
		if(n != GL_NEAREST && n != GL_LINEAR)
			gl_fatal_error("invalid texture magnification filter");
		t->mag = n;
		break;
	case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T:
		if(n != GL_REPEAT && n != GL_CLAMP && n != GL_CLAMP_TO_EDGE)
			gl_fatal_error("unsupported texture wrap");
		t->wrap[p[2].i-GL_TEXTURE_WRAP_S] = n;
		break;
	case GL_GENERATE_MIPMAP_SGIS:
		t->generate = n != 0;
		break;
	default: gl_fatal_error("unsupported texture parameter");
	}
}

void
glTexEnvfv(GLenum target, GLenum pname, const GLfloat *v)
{
	GLParam p[8];
	int i;

	if(pname != GL_TEXTURE_ENV_COLOR){
		glTexEnvi(target, pname, v[0]);
		return;
	}
	p[0].op = OP_TexEnv;
	p[1].i = target;
	p[2].i = pname;
	p[3].i = 0;
	for(i=0; i<4; i++) p[4+i].f = v[i];
	gl_add_op(p);
}

void
glopTexEnv(GLContext *c, GLParam *p)
{
	GLTexenv *e;
	int code, n, i, a;

	if(p[1].i == GL_POINT_SPRITE && p[2].i == GL_COORD_REPLACE){
		gl_flush(c);
		c->texunit[c->active_texture].replace = p[3].i != 0;
		return;
	}
	if(p[1].i != GL_TEXTURE_ENV)
		gl_fatal_error("unsupported texture environment target");
	gl_flush(c);
	e = &c->texunit[c->active_texture].env;
	code = p[2].i;
	n = p[3].i;
	switch(code){
	case GL_TEXTURE_ENV_COLOR:
		for(i=0; i<4; i++) e->color.v[i] = p[4+i].f<0 ? 0 : p[4+i].f>1 ? 1 : p[4+i].f;
		return;
	case GL_TEXTURE_ENV_MODE:
		if(n != GL_MODULATE && n != GL_REPLACE && n != GL_DECAL && n != GL_ADD && n != GL_BLEND && n != GL_COMBINE)
			gl_fatal_error("unsupported texture environment mode");
		e->mode = n;
		return;
	case GL_COMBINE_RGB: case GL_COMBINE_ALPHA:
		if(n != GL_MODULATE && n != GL_REPLACE && n != GL_ADD && n != GL_ADD_SIGNED && n != GL_INTERPOLATE)
			gl_fatal_error("unsupported texture combiner");
		e->combine[code == GL_COMBINE_ALPHA] = n;
		return;
	case GL_RGB_SCALE: case GL_ALPHA_SCALE:
		if(n != 1 && n != 2 && n != 4)
			gl_fatal_error("invalid texture combiner scale");
		e->scale[code == GL_ALPHA_SCALE] = n;
		return;
	}
	a = code >= GL_SRC0_ALPHA && code <= GL_SRC2_ALPHA;
	i = code-(a ? GL_SRC0_ALPHA : GL_SRC0_RGB);
	if(i >= 0 && i<3){
		if(n != GL_TEXTURE && n != GL_CONSTANT && n != GL_PRIMARY_COLOR && n != GL_PREVIOUS)
			gl_fatal_error("unsupported texture combiner source");
		e->source[a][i] = n;
		return;
	}
	a = code >= GL_OPERAND0_ALPHA && code <= GL_OPERAND2_ALPHA;
	i = code-(a ? GL_OPERAND0_ALPHA : GL_OPERAND0_RGB);
	if(i >= 0 && i<3){
		if(n != GL_SRC_ALPHA && n != GL_ONE_MINUS_SRC_ALPHA && (a || (n != GL_SRC_COLOR && n != GL_ONE_MINUS_SRC_COLOR)))
			gl_fatal_error("invalid texture combiner operand");
		e->operand[a][i] = n;
		return;
	}
	gl_fatal_error("unsupported texture environment parameter");
}

void
glTexGeni(GLenum coord, GLenum name, GLint value)
{
	float v[4];

	if(name != GL_TEXTURE_GEN_MODE)
		gl_fatal_error("texture generation mode required");
	v[0] = value;
	glTexGenfv(coord,name,v);
}

void
glTexGenfv(GLenum coord, GLenum name, const GLfloat *value)
{
	GLParam p[7];
	int i;

	memset(p,0,sizeof(p));
	p[0].op = OP_TexGen;
	p[1].i = coord;
	p[2].i = name;
	for(i=0; i<(name == GL_TEXTURE_GEN_MODE ? 1 : 4); i++)
		p[3+i].f = value[i];
	gl_add_op(p);
}

void
glopTexGen(GLContext *c, GLParam *p)
{
	GLTexunit *u;
	M4 inv, trans;
	V4 plane;
	int i, mode;

	i = p[1].i-GL_S;
	if(i < 0 || i >= 4)
		gl_fatal_error("invalid generated texture coordinate");
	u = &c->texunit[c->active_texture];
	plane = gl_V4_New(p[3].f,p[4].f,p[5].f,p[6].f);
	switch(p[2].i){
	case GL_TEXTURE_GEN_MODE:
		mode = p[3].f;
		if(mode != GL_OBJECT_LINEAR && mode != GL_EYE_LINEAR && (mode != GL_SPHERE_MAP || i>1))
			gl_fatal_error("unsupported texture generation mode");
		u->genmode[i] = mode;
		break;
	case GL_OBJECT_PLANE: u->objectplane[i] = plane; break;
	case GL_EYE_PLANE:
		gl_M4_Inv(&inv,c->matrix_stack_ptr[0]);
		gl_M4_Transpose(&trans,&inv);
		gl_M4_MulV4(&u->eyeplane[i],&trans,&plane);
		break;
	default: gl_fatal_error("invalid texture generation parameter");
	}
}

void
gl_texcoord(GLContext *c, GLVertex *v)
{
	GLTexunit *u;
	V4 coord, *p, *q;
	V3 eye, normal, reflect;
	float dot, m, distance, size;
	int i, j, k, sphere;

	sphere = 0;
	for(i=0; i<MAX_TEXTURE_UNITS; i++){
		u = &c->texunit[i];
		if(!u->enabled)
			continue;
		coord = u->coord;
		for(j=0; j<4; j++){
			if(!u->gen[j]) continue;
			if(u->genmode[j] == GL_SPHERE_MAP){
				if(!sphere){
					eye = gl_V3_New(v->ec.X,v->ec.Y,v->ec.Z);
					normal = v->normal;
					gl_V3_Norm(&eye);
					gl_V3_Norm(&normal);
					dot = eye.X*normal.X+eye.Y*normal.Y+eye.Z*normal.Z;
					for(k=0; k<3; k++) reflect.v[k] = eye.v[k]-2*dot*normal.v[k];
					m = 2*sqrt(reflect.X*reflect.X+reflect.Y*reflect.Y+(reflect.Z+1)*(reflect.Z+1));
					reflect.X = m == 0 ? .5 : reflect.X/m+.5;
					reflect.Y = m == 0 ? .5 : reflect.Y/m+.5;
					sphere = 1;
				}
				coord.v[j] = reflect.v[j];
			}else{
				p = u->genmode[j] == GL_OBJECT_LINEAR ? &u->objectplane[j] : &u->eyeplane[j];
				q = u->genmode[j] == GL_OBJECT_LINEAR ? &v->coord : &v->ec;
				coord.v[j] = 0;
				for(k=0; k<4; k++) coord.v[j] += p->v[k]*q->v[k];
			}
		}
		gl_M4_MulV4(&v->texcoord[i],c->matrix_stack_ptr[2+i],&coord);
	}
	if(c->begin_type == GL_POINTS){
		distance = sqrt(v->ec.X*v->ec.X+v->ec.Y*v->ec.Y+v->ec.Z*v->ec.Z);
		if(v->ec.W != 0) distance /= fabs(v->ec.W);
		m = c->point_atten[0]+distance*(c->point_atten[1]+distance*c->point_atten[2]);
		size = m > 0 ? c->point_size/sqrt(m) : c->point_max;
		if(size < c->point_min) size = c->point_min;
		if(size > c->point_max) size = c->point_max;
		v->point_size = size < 1 ? 1 : size > 64 ? 64 : size;
	}
}
