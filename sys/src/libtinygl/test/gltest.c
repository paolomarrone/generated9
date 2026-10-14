#include <u.h>
#include <libc.h>
#include <GL/gl.h>
#include <tinygl.h>

enum { W = 64, H = 64 };
static uchar pixels[W*H*4], first[W*H*4];
static char *device;

static void
readback(void)
{
	tglread(pixels, sizeof(pixels));
}

static void
pixel(int x, int y, int r, int g, int b, char *test)
{
	uchar *p;

	p = pixels+4*(y*W+x);
	if(p[0] != b || p[1] != g || p[2] != r)
		sysfatal("%s: pixel %d,%d is %d %d %d, expected %d %d %d", test,
			x, y, p[2], p[1], p[0], r, g, b);
}

static void
triangle(float z)
{
	glBegin(GL_TRIANGLES);
	glVertex3f(-.75, -.75, z);
	glVertex3f(.75, -.75, z);
	glVertex3f(0, .75, z);
	glEnd();
}

static void
clear(void)
{
	glClearColor(0, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

static void
texquad(void)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(-1, -1);
	glTexCoord2f(1, 0); glVertex2f(1, -1);
	glTexCoord2f(1, 1); glVertex2f(1, 1);
	glTexCoord2f(0, 1); glVertex2f(-1, 1);
	glEnd();
}

static void
clipplanes(void)
{
	double plane[4] = {1,0,0,0}, got[4];
	uint list;
	int i, n;

	glGetIntegerv(GL_MAX_CLIP_PLANES,&n);
	if(n != 6) sysfatal("clip plane count");
	glClipPlane(GL_CLIP_PLANE0,plane);
	glEnable(GL_CLIP_PLANE0);
	clear();
	glColor3f(1,0,0);
	texquad();
	readback();
	pixel(16,32,0,0,255,"clipped quad");
	pixel(48,32,255,0,0,"retained quad");

	glTranslatef(.25,0,0);
	glClipPlane(GL_CLIP_PLANE0,plane);
	glLoadIdentity();
	glGetClipPlane(GL_CLIP_PLANE0,got);
	if(got[0] != 1 || got[3] != -.25) sysfatal("clip plane eye transform");
	clear();
	texquad();
	readback();
	pixel(36,32,0,0,255,"captured clip transform");
	pixel(48,32,255,0,0,"captured clip transform");

	list = glGenLists(1);
	glNewList(list,GL_COMPILE);
	glClipPlane(GL_CLIP_PLANE0,plane);
	glEndList();
	glTranslatef(-.25,0,0);
	glCallList(list);
	glLoadIdentity();
	glDeleteLists(list,1);
	glGetClipPlane(GL_CLIP_PLANE0,got);
	if(got[3] != .25) sysfatal("clip plane display list execution");
	for(i=0; i<n; i++){
		memset(plane,0,sizeof plane);
		plane[i/2] = (i&1) ? -1 : 1;
		plane[3] = .5;
		glClipPlane(GL_CLIP_PLANE0+i,plane);
		glEnable(GL_CLIP_PLANE0+i);
		if(!glIsEnabled(GL_CLIP_PLANE0+i)) sysfatal("clip enable query");
	}
	clear();
	texquad();
	readback();
	pixel(32,32,255,0,0,"six clip planes");
	pixel(8,32,0,0,255,"six clip planes left");
	pixel(56,32,0,0,255,"six clip planes right");
	pixel(32,8,0,0,255,"six clip planes top");
	pixel(32,56,0,0,255,"six clip planes bottom");
	clear();
	glLineWidth(3);
	glBegin(GL_LINES);
	glColor3f(0,1,0);
	glVertex2f(-1,0); glVertex2f(1,0);
	glEnd();
	glPointSize(5);
	glBegin(GL_POINTS);
	glColor3f(1,0,0);
	glVertex2f(0,.25); glVertex2f(.75,.25);
	glEnd();
	readback();
	pixel(32,32,0,255,0,"clipped line");
	pixel(8,32,0,0,255,"discarded line");
	pixel(32,24,255,0,0,"retained point");
	pixel(56,24,0,0,255,"discarded point");
	glLineWidth(1);
	glPointSize(1);
	for(i=0; i<n; i++) glDisable(GL_CLIP_PLANE0+i);
	print("PASS: user clip planes, eye transforms, display lists, triangles, lines and points\n");
}

static void
streaming(void)
{
	int x, y;

	/* Distinct colors survive command submissions and vertex-buffer reuse. */
	clear();
	for(y=0; y<H; y++)
		for(x=0; x<W; x++){
			glViewport(x, y, 1, 1);
			glColor3f(x*4/255., y*4/255., (x^y)*4/255.);
			texquad();
		}
	readback();
	for(y=0; y<H; y++)
		for(x=0; x<W; x++)
			pixel(x, H-1-y, x*4, y*4, (x^y)*4, "streamed vertices");
	glViewport(0, 0, W, H);
	print("PASS: vertex contents across command submissions and buffer reuse\n");
}

static void
texturepressure(int count, int size)
{
	GLuint *tex;
	uchar *data;
	int i, pass;

	tex = malloc(count*sizeof(*tex));
	data = malloc(size*size*4);
	if(tex == nil || data == nil) sysfatal("texture test allocation: %r");
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glGenTextures(count, tex);
	for(pass=0; pass<2; pass++){
		if(pass == 1 && size > 1){
			tglresize(2048, 2048);
			tglresize(W, H);
		}
		for(i=0; i<count; i++){
			glBindTexture(GL_TEXTURE_2D, tex[i]);
			if(pass == 0){
				memset(data, i%80*3, size*size*4);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			}
			texquad();
			readback();
			pixel(32, 32, i%80*3, i%80*3, i%80*3, "texture residency and reupload");
		}
	}
	glDeleteTextures(count, tex);
	free(tex);
	free(data);
	glDisable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	print("PASS: texture residency for %d images of %d by %d\n", count, size, size);
}

static void
textures(void)
{
	uchar rgb[16] = {255,0,0, 0,255,0, 99,99, 0,0,255, 255,255,255, 99,99};
	uchar green[4] = {0,255,0,255};
	GLuint t[2], list;

	glGenTextures(2, t);
	if(glIsTexture(t[0])) sysfatal("reserved texture is already defined");
	glBindTexture(GL_TEXTURE_2D, t[0]);
	if(!glIsTexture(t[0])) sysfatal("bound texture is not defined");
	glEnable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
	glColor4f(1,1,1,1);
	texquad();
	readback();
	pixel(16,48,255,0,0,"texture bottom left");
	pixel(48,48,0,255,0,"texture bottom right");
	pixel(16,16,0,0,255,"texture top left");
	pixel(48,16,255,255,255,"texture top right");

	/* A texture replacement must first finish the old batch. */
	glViewport(0,0,W/2,H);
	texquad();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
	glViewport(W/2,0,W/2,H);
	texquad();
	readback();
	pixel(8,48,255,0,0,"texture update preserves old draw");
	pixel(48,48,0,255,0,"texture update new draw");
	glViewport(0,0,W,H);

	list = glGenLists(1);
	glNewList(list,GL_COMPILE);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);
	glEndList();
	memset(rgb,0,sizeof(rgb));
	glCallList(list);
	glDeleteLists(list,1);
	texquad();
	readback();
	pixel(16,48,255,0,0,"display list owns texture pixels");

	/* Independent matrices, bindings and combiner state on a second unit. */
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,t[1]);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,green);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_ADD);
	glMultiTexCoord4f(GL_TEXTURE1,0,0,0,1);
	glMatrixMode(GL_TEXTURE);
	glTranslatef(.5,.5,0);
	glActiveTexture(GL_TEXTURE0);
	glLoadIdentity();
	glTranslatef(.5,0,0);
	glMatrixMode(GL_MODELVIEW);
	texquad();
	readback();
	pixel(16,48,0,255,0,"texture matrices are per unit");
	pixel(48,48,255,255,0,"texture combiner add");
	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
	texquad();
	readback();
	pixel(16,48,0,0,0,"incomplete mipmap texture");
	glTexParameteri(GL_TEXTURE_2D,GL_GENERATE_MIPMAP_SGIS,1);
	/* Four pixels reduce to a single exact green texel. */
	memset(rgb,0,sizeof(rgb));
	rgb[1]=rgb[4]=rgb[9]=rgb[12]=255;
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);
	glMatrixMode(GL_TEXTURE);
	glScalef(128,128,1);
	glMatrixMode(GL_MODELVIEW);
	texquad();
	readback();
	pixel(16,48,0,255,0,"generated mipmap");
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glDeleteTextures(2,t);
	if(glIsTexture(t[0])) sysfatal("texture not deleted");
	glDisable(GL_TEXTURE_2D);
	print("PASS: textures, unpack alignment, updates, list ownership, multitexture and mipmaps\n");
}

static void
matrices(void)
{
	float m[16];

	glPushMatrix();
	glRotatef(120, 1, 1, 1);
	glGetFloatv(GL_MODELVIEW_MATRIX, m);
	/* A positive 120-degree rotation about (1,1,1) maps X to Y. */
	if(fabs(m[0]) > 1e-5 || fabs(m[1]-1) > 1e-5 || fabs(m[2]) > 1e-5)
		sysfatal("general axis rotation");
	glPopMatrix();
	glGetFloatv(GL_MODELVIEW_MATRIX, m);
	if(m[0] != 1 || m[5] != 1 || m[10] != 1 || m[15] != 1)
		sysfatal("matrix stack restore");
	print("PASS: general axis rotation and matrix stack\n");
}

static void
fragmentstate(void)
{
	uchar *p;

	clear();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1,0,0,.5);
	triangle(0);
	readback();
	p = pixels+4*(32*W+32);
	if(p[2] < 127 || p[2] > 128 || p[1] != 0 || p[0] < 127 || p[0] > 128)
		sysfatal("alpha blending: %d %d %d",p[2],p[1],p[0]);
	glDisable(GL_BLEND);
	clear();
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,.5);
	triangle(0);
	readback();
	pixel(32,32,0,0,255,"alpha rejected");
	glColor4f(1,0,0,.75);
	triangle(0);
	readback();
	pixel(32,32,255,0,0,"alpha accepted");
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	clear();
	glDepthMask(GL_FALSE);
	triangle(-.5);
	glColor3f(0,1,0);
	triangle(.5);
	readback();
	pixel(32,32,0,255,0,"depth writes disabled");
	glDepthMask(GL_TRUE);
	clear();
	glColor3f(1,0,0);
	triangle(0);
	glColor3f(0,1,0);
	triangle(0);
	readback();
	pixel(32,32,0,255,0,"equal depth accepted");
	glDepthFunc(GL_LESS);
	glColor3f(1,0,0);
	triangle(0);
	readback();
	pixel(32,32,0,255,0,"equal depth rejected");
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(0,-1);
	triangle(0);
	readback();
	pixel(32,32,255,0,0,"polygon offset");
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_DEPTH_TEST);
	print("PASS: alpha blending and testing, depth mask/function and polygon offset\n");
}

static void
vertexlayouts(void)
{
	GLuint texture;
	uchar rgba[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
	double plane[] = {0,0,0,-1};

	clear();
	glActiveTexture(GL_TEXTURE3);
	glGenTextures(1,&texture);
	glBindTexture(GL_TEXTURE_2D,texture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
	glEnable(GL_TEXTURE_2D);
	glMultiTexCoord4f(GL_TEXTURE3,.75,.25,0,1);
	glColor3f(1,1,1);
	texquad();
	glDisable(GL_TEXTURE_2D);
	glColor3f(1,0,0);
	glPointSize(7);
	glBegin(GL_POINTS);
	glVertex2f(0,0);
	glEnd();
	readback();
	pixel(16,16,0,255,0,"sparse texture unit");
	pixel(48,48,0,255,0,"sparse texture coordinates");
	pixel(32,32,255,0,0,"point after textured triangles");
	/* A masked clear ignores user clipping and the previous primitive type. */
	glClipPlane(GL_CLIP_PLANE0,plane);
	glEnable(GL_CLIP_PLANE0);
	glScissor(0,0,W/2,H);
	glEnable(GL_SCISSOR_TEST);
	clear();
	readback();
	pixel(16,16,0,0,255,"clipped masked clear");
	pixel(48,48,0,255,0,"outside masked clear");
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CLIP_PLANE0);
	glPointSize(1);
	glDeleteTextures(1,&texture);
	glActiveTexture(GL_TEXTURE0);
	print("PASS: sparse texture inputs, vertex layout changes and clipped masked clears\n");
}

static void
texgenpoints(void)
{
	GLuint t;
	uchar rgb[16] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
	float s[4] = {.5,0,0,.5}, tplane[4] = {0,.5,0,.5};
	float atten[3] = {4,0,0};

	glGenTextures(1,&t);
	glBindTexture(GL_TEXTURE_2D,t);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgb);
	glEnable(GL_TEXTURE_2D);
	glColor4f(1,1,1,1);
	glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);
	glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);
	glTexGenfv(GL_S,GL_OBJECT_PLANE,s);
	glTexGenfv(GL_T,GL_OBJECT_PLANE,tplane);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	texquad();
	readback();
	pixel(16,48,255,0,0,"object texgen bottom left");
	pixel(48,16,255,255,255,"object texgen top right");
	glPushMatrix();
	glTranslatef(.5,0,0);
	glTexGenfv(GL_S,GL_EYE_PLANE,s);
	glTexGenfv(GL_T,GL_EYE_PLANE,tplane);
	glPopMatrix();
	glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);
	glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);
	texquad();
	readback();
	pixel(40,48,255,0,0,"eye plane captures specification matrix");
	pixel(56,48,0,255,0,"eye plane transformed coordinates");
	glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
	glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glOrtho(-1,1,-1,1,1,3);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(0,0,-2);
	glNormal3f(0,0,1);
	texquad();
	readback();
	pixel(16,48,255,0,0,"sphere map bottom left");
	pixel(48,16,255,255,255,"sphere map top right");
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	clear();
	glPointSize(32);
	glEnable(GL_POINT_SPRITE);
	glTexEnvi(GL_POINT_SPRITE,GL_COORD_REPLACE,GL_TRUE);
	glBegin(GL_POINTS);
	glVertex2f(0,0);
	glEnd();
	readback();
	pixel(24,40,255,0,0,"point sprite bottom left");
	pixel(40,24,255,255,255,"point sprite top right");
	pixel(8,32,0,0,255,"point size bounds");
	clear();
	glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION,atten);
	glBegin(GL_POINTS);
	glVertex2f(0,0);
	glEnd();
	readback();
	pixel(20,32,0,0,255,"attenuated point bounds");
	pixel(28,36,255,0,0,"attenuated point sprite");
	atten[0] = 1;
	glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION,atten);
	glPointSize(1);
	glDisable(GL_POINT_SPRITE);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1,&t);
	print("PASS: generated texture coordinates, point sprites, size and attenuation\n");
}

static void
geometry(void)
{
	int i;

	clear();
	glColor3f(1, 0, 0);
	triangle(2);
	triangle(-2);
	readback();
	pixel(32, 32, 0, 0, 255, "far and near clipping");
	glBegin(GL_TRIANGLES);
	glVertex3f(-2, -.5, 0);
	glVertex3f(.5, -.5, 0);
	glVertex3f(.5, .5, 0);
	glEnd();
	readback();
	pixel(1, 40, 255, 0, 0, "side clipping");
	pixel(60, 40, 0, 0, 255, "outside clipped polygon");
	clear();
	glBegin(GL_TRIANGLES);
	glVertex3f(-.75, -.75, -2);
	glVertex3f(.75, -.75, 0);
	glVertex3f(0, .75, 0);
	glEnd();
	readback();
	pixel(32, 32, 255, 0, 0, "near-plane intersection");
	pixel(12, 52, 0, 0, 255, "clipped near corner");

	clear();
	glTranslatef(.125, .125, 0);
	triangle(0);
	readback();
	memmove(first, pixels, sizeof(first));
	clear();
	glBegin(GL_TRIANGLES);
	glVertex4f(-1.5, -1.5, 0, 2);
	glVertex4f(1.5, -1.5, 0, 2);
	glVertex4f(0, 1.5, 0, 2);
	glEnd();
	readback();
	if(memcmp(first, pixels, sizeof(first)) != 0)
		sysfatal("homogeneous vertex coordinates");
	glLoadIdentity();

	clear();
	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	glColor3f(1, 0, 0); glVertex2f(-.75, -.75);
	glColor3f(0, 0, 1); glVertex2f(.75, -.75);
	glColor3f(1, 1, 1); glVertex2f(.75, .75);
	glColor3f(0, 1, 0); glVertex2f(-.75, .75);
	glEnd();
	readback();
	pixel(16, 16, 0, 255, 0, "quad provoking vertex");
	pixel(48, 48, 0, 255, 0, "quad second triangle");
	glShadeModel(GL_SMOOTH);

	clear();
	glColor3f(1, 0, 0);
	glViewport(0, 0, W/2, H/2);
	triangle(0);
	glViewport(W/2, H/2, W/2, H/2);
	glColor3f(0, 1, 0);
	triangle(0);
	readback();
	pixel(16, 48, 255, 0, 0, "queued viewport");
	pixel(48, 16, 0, 255, 0, "new viewport");
	glViewport(0, 0, W, H);

	clear();
	glTranslatef(-.5,0,0);
	glScalef(.4,.4,1);
	glColor3f(1,0,0);
	triangle(0);
	glLoadIdentity();
	glTranslatef(.5,0,0);
	glScalef(.4,.4,1);
	glColor3f(0,1,0);
	triangle(0);
	glLoadIdentity();
	readback();
	pixel(16,32,255,0,0,"queued vertex matrix");
	pixel(48,32,0,255,0,"changed vertex matrix");

	clear();
	for(i=0; i<3000; i++) triangle(0);
	glColor3f(1, 0, 0);
	triangle(0);
	readback();
	pixel(32, 32, 255, 0, 0, "vertex batch boundary");
	glClear(GL_COLOR_BUFFER_BIT);
	readback();
	pixel(32, 32, 0, 0, 255, "clear after draw");
	print("PASS: clipping, homogeneous coordinates, flat quads, state changes and batching\n");
}

static void
lists(void)
{
	uint list, reserved;

	clear();
	list = glGenLists(1);
	if(list == 0 || glIsList(list)) sysfatal("reserved name is not a defined list");
	glCallList(list);
	glCallList(0);
	glCallList(0xFFFFFFFFU);
	glNewList(list, GL_COMPILE);
	glColor3f(1, 0, 0);
	triangle(0);
	glEndList();

	/* The old definition remains callable while its replacement is compiled. */
	glNewList(list, GL_COMPILE_AND_EXECUTE);
	glCallList(list);
	readback();
	pixel(32, 32, 255, 0, 0, "call old list during replacement");
	glEndList();
	/* The replacement now refers to itself; delete it without executing it. */
	glDeleteLists(list, 1);

	/* Deletion affects the public table, never the buffer being compiled. */
	glNewList(list, GL_COMPILE);
	glDeleteLists(list, 1);
	glColor3f(0, 1, 0);
	triangle(0);
	reserved = glGenLists(1);
	if(reserved == list) sysfatal("list under construction was not reserved");
	glEndList();
	glCallList(list);
	readback();
	pixel(32, 32, 0, 255, 0, "delete while compiling");
	glDeleteLists(list, 1);
	glDeleteLists(reserved, 1);
	print("PASS: list reservation, replacement and deletion while compiling\n");
}

static void
polygons(void)
{
	int i;
	double a;

	clear();
	glEnable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);
	glBegin(GL_POLYGON);
	glEnd();
	glBegin(GL_POLYGON);
	glVertex2f(0, 0);
	glVertex2f(1, 0);
	glEnd();
	readback();
	pixel(32, 32, 0, 0, 255, "empty and incomplete polygons");
	glBegin(GL_POLYGON);
	for(i=0; i<10000; i++){
		a = i*2*PI/10000;
		glColor3f(i == 0, i != 0, 0);
		glVertex2f(.75*cos(a), .75*sin(a));
	}
	glEnd();
	readback();
	pixel(32, 32, 255, 0, 0, "large flat polygon");
	pixel(16, 32, 255, 0, 0, "polygon first vertex color");
	pixel(0, 0, 0, 0, 255, "outside polygon");
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	print("PASS: empty and large convex polygons across GPU batches\n");
}

static void
arrays(void)
{
	schar signedcolor[4] = {0,63,-128,127};
	float v[3][7] = {
		{-.75, -.75, 0, 1, 0, 0, 99},
		{.75, -.75, 0, 1, 0, 0, 99},
		{0, .75, 0, 1, 0, 0, 99},
	};
	uint list;
	int i;

	clear();
	glVertexPointer(3, GL_FLOAT, sizeof(v[0]), v);
	glColorPointer(3, GL_FLOAT, sizeof(v[0]), &v[0][3]);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	list = glGenLists(1);
	glNewList(list, GL_COMPILE);
	glBegin(GL_TRIANGLES);
	for(i=0; i<3; i++) glArrayElement(i);
	glEnd();
	glEndList();
	/* A display list must not retain pointers to client arrays. */
	memset(v, 0, sizeof(v));
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glCallList(list);
	readback();
	pixel(32, 32, 255, 0, 0, "interleaved arrays and list snapshot");
	glDeleteLists(list, 1);
	glColorPointer(4,GL_BYTE,0,signedcolor);
	glEnableClientState(GL_COLOR_ARRAY);
	glArrayElement(0);
	glDisableClientState(GL_COLOR_ARRAY);
	triangle(0);
	readback();
	pixel(32,32,1,127,0,"classic signed color normalization");
	print("PASS: byte strides, color arrays and display list ownership\n");
}

static void
buffers(void)
{
	struct { uchar color[4]; short pos[2]; } v[] = {
		{{255, 0, 0, 255}, {-3, -3}},
		{{255, 0, 0, 255}, {3, -3}},
		{{255, 0, 0, 255}, {0, 3}},
	};
	ushort indices[] = {99, 2, 0, 1};
	uchar green[4] = {0, 255, 0, 255};
	GLuint b[2], list;
	int i, size;

	clear();
	glGenBuffers(2, b);
	if(b[0] == 0 || b[0] == b[1] || glIsBuffer(b[0]))
		sysfatal("buffer name reservation");
	glBindBuffer(GL_ARRAY_BUFFER, b[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
	if(!glIsBuffer(b[0]) || size != sizeof(v)) sysfatal("buffer definition");
	glVertexPointer(2, GL_SHORT, sizeof(v[0]), (void*)4);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(v[0]), nil);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	/* Array pointers capture the binding, not the current buffer slot. */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glPushMatrix();
	glScalef(.25, .25, 1);
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void*)2);
	readback();
	pixel(32, 32, 255, 0, 0, "interleaved byte color and short vertex buffers");

	glBindBuffer(GL_ARRAY_BUFFER, b[0]);
	for(i=0; i<3; i++)
		glBufferSubData(GL_ARRAY_BUFFER, i*sizeof(v[0]), sizeof(green), green);
	clear();
	glDrawArrays(GL_TRIANGLES, 0, 3);
	readback();
	pixel(32, 32, 0, 255, 0, "buffer partial update");
	list = glGenLists(1);
	glNewList(list, GL_COMPILE);
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void*)2);
	glEndList();
	glDeleteBuffers(2, b);
	if(glIsBuffer(b[0]) || glIsBuffer(b[1])) sysfatal("buffer deletion");
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	clear();
	glCallList(list);
	readback();
	pixel(32, 32, 0, 255, 0, "display list owns deleted buffer contents");
	glDeleteLists(list, 1);
	glPopMatrix();
	print("PASS: indexed buffers, mixed array types, updates and display list ownership\n");
}

static void
lighting(void)
{
	float ambient[4] = {0, 0, 0, 1};
	float diffuse[4] = {1, 0, 0, 1};
	float normals[3][3] = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};

	clear();
	glEnable(GL_LIGHTING);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glEnable(GL_LIGHT0);
	/* Removing a non-head light must leave the other enabled lights linked. */
	glEnable(GL_LIGHT1);
	glDisable(GL_LIGHT0);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
	glNormalPointer(GL_FLOAT, 0, normals);
	glEnableClientState(GL_NORMAL_ARRAY);
	glBegin(GL_TRIANGLES);
	glArrayElement(0); glVertex3f(-.75, -.75, 0);
	glArrayElement(1); glVertex3f(.75, -.75, 0);
	glArrayElement(2); glVertex3f(0, .75, 0);
	glEnd();
	glDisableClientState(GL_NORMAL_ARRAY);
	readback();
	pixel(32, 32, 255, 0, 0, "normal array and light list");
	glDisable(GL_LIGHT1);
	glEnable(GL_LIGHT0);
	glColor3f(0, 1, 0);
	glEnable(GL_COLOR_MATERIAL);
	clear();
	triangle(0);
	readback();
	pixel(32, 32, 0, 255, 0, "color material initial color");
	glColor3f(0, 0, 1);
	clear();
	triangle(0);
	readback();
	pixel(32, 32, 0, 0, 255, "color material follows color");
	glColorMaterial(GL_FRONT_AND_BACK, GL_EMISSION);
	glDisable(GL_LIGHT0);
	clear();
	triangle(0);
	readback();
	pixel(32, 32, 0, 0, 255, "color material new mode");
	glDisable(GL_COLOR_MATERIAL);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,1);
	glMaterialfv(GL_FRONT,GL_EMISSION,diffuse);
	diffuse[0]=0; diffuse[1]=1;
	glMaterialfv(GL_BACK,GL_EMISSION,diffuse);
	clear();
	triangle(0);
	readback();
	pixel(32,32,255,0,0,"front material");
	glFrontFace(GL_CW);
	triangle(0);
	readback();
	pixel(32,32,0,255,0,"back material");
	glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,ambient);
	glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,diffuse);
	glEnable(GL_LIGHT0);
	glNormal3f(0,0,-1);
	clear();
	triangle(0);
	readback();
	pixel(32,32,0,255,0,"back normal is reversed");
	glFrontFace(GL_CCW);
	triangle(0);
	readback();
	pixel(32,32,0,0,0,"front normal is preserved");
	glNormal3f(0,0,1);
	glDisable(GL_LIGHT0);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,0);
	glDisable(GL_LIGHTING);
	print("PASS: material lighting, color tracking, normal arrays and light lifetime\n");
}

static void
primitives(void)
{
	int i, n;
	uchar *p;

	clear();
	glBegin(GL_TRIANGLES);
	glColor3f(1, 0, 0); glVertex2f(-.75, -.75);
	glColor3f(0, 1, 0); glVertex2f(.75, -.75);
	glColor3f(0, 0, 1); glVertex2f(0, .75);
	glEnd();
	readback();
	p = pixels+4*(32*W+32);
	if(p[0] < 110 || p[0] > 140 || p[1] < 50 || p[1] > 80 || p[2] < 50 || p[2] > 80)
		sysfatal("smooth color interpolation: %d %d %d", p[2], p[1], p[0]);
	clear();
	glColor3f(1, 1, 1);
	glBegin(GL_POINTS);
	glVertex2f(0, 0);
	glEnd();
	readback();
	n = 0;
	for(i=0; i<sizeof(pixels); i+=4) n += pixels[i+2] != 0;
	if(n != 1) sysfatal("point covers %d pixels", n);
	clear();
	glBegin(GL_LINES);
	glVertex2f(-.75, 0);
	glVertex2f(.75, 0);
	glEnd();
	readback();
	n = 0;
	for(i=0; i<sizeof(pixels); i+=4) n += pixels[i+2] != 0;
	if(n < 40 || n > 52) sysfatal("line covers %d pixels", n);

	glEnable(GL_DEPTH_TEST);
	glClearDepth(0);
	clear();
	triangle(0);
	readback();
	pixel(32, 32, 0, 0, 255, "clear depth zero");
	glClearDepth(1);
	glClear(GL_DEPTH_BUFFER_BIT);
	triangle(0);
	readback();
	pixel(32, 32, 255, 255, 255, "depth-only clear");
	glDisable(GL_DEPTH_TEST);
	print("PASS: smooth colors, points, lines and clear depth\n");
}

static void
errors(void)
{
	Waitmsg *w;
	int i, pid, fd;
	GLuint buffer;
	char *expected[] = {
		"unsupported capability", "readback buffer too small",
		"target size must", "bad display list id", "invalid viewport",
		"invalid operation inside glBegin", "display list nesting too deep",
		"buffer access out of bounds", "buffer access out of bounds",
	};

	for(i=0; i<nelem(expected); i++){
		pid = fork();
		if(pid < 0) sysfatal("fork: %r");
		if(pid == 0){
			fd = open("/dev/null", OWRITE);
			if(fd >= 0){ dup(fd, 2); close(fd); }
			switch(i){
			case 0: glEnable(GL_TEXTURE_1D); break;
			case 1: tglread(pixels, 1); break;
			case 2: tglresize(0, 64); break;
			case 3: glNewList(0xFFFFFFFFU, GL_COMPILE); break;
			case 4: glViewport(0, 0, -1, 64); break;
			case 5: glBegin(GL_TRIANGLES); glLoadIdentity(); break;
			case 6:
				glNewList(1, GL_COMPILE);
				glCallList(1);
				glEndList();
				glCallList(1);
				break;
			case 7:
				glBindBuffer(GL_ARRAY_BUFFER, 1);
				glBufferData(GL_ARRAY_BUFFER, 16, nil, GL_STATIC_DRAW);
				glBufferSubData(GL_ARRAY_BUFFER, 15, 4, pixels);
				break;
			case 8:
				glGenBuffers(1, &buffer);
				glBindBuffer(GL_ARRAY_BUFFER, buffer);
				glBufferData(GL_ARRAY_BUFFER, 4, nil, GL_STATIC_DRAW);
				glVertexPointer(3, GL_FLOAT, 0, nil);
				glEnableClientState(GL_VERTEX_ARRAY);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			}
			exits("invalid input accepted");
		}
		w = wait();
		if(w == nil || w->pid != pid || strstr(w->msg, expected[i]) == nil)
			sysfatal("error boundary %d: %s", i, w == nil ? "no child status" : w->msg);
		free(w);
	}
	print("PASS: unsupported operations and invalid API arguments\n");
}

static void
lifetime(void)
{
	int i;

	for(i=0; i<40; i++){
		tglresize(W/2, H/2);
		clear();
		tglresize(W, H);
		clear();
	}
	tglclose();
	for(i=0; i<40; i++){
		tglinit(device, W, H);
		clear();
		readback();
		pixel(32, 32, 0, 0, 255, "recreated context");
		tglclose();
	}
	print("PASS: target and context recreation beyond device resource limits\n");
}

static void
pixelops(void)
{
	uchar input[16] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
	uchar patch[8] = {255,255,0,255, 0,255,255,255}, out[64];
	uchar alpha[16] = {255,255,255,0, 255,255,255,17, 255,255,255,0, 255,255,255,91};
	uchar luma[16] = {0,0,0,255, 255,255,255,255, 0,0,0,255, 255,255,255,255};
	ushort packed[4] = {0xf800,0x07e0,0x001f,0xffff}, packedout[4];
	GLuint t, list;
	int i;

	tglclose();
	tglinit(device,W,H);
	glViewport(0,0,W,H);
	glClearColor(0,0,1,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_SCISSOR_TEST);
	glScissor(4,8,12,16);
	glClearColor(1,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT);
	readback();
	pixel(8,H-12,255,0,0,"scissored clear");
	pixel(20,H-12,0,0,255,"outside scissored clear");
	pixel(8,12,0,0,255,"scissor origin");
	glColorMask(0,1,0,0);
	glClearColor(0,1,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	readback();
	pixel(8,H-12,255,255,0,"masked scissored clear");
	glColorMask(1,1,1,1);
	glColor3f(0,1,0);
	texquad();
	readback();
	pixel(8,H-12,0,255,0,"scissored draw");
	pixel(20,H-12,0,0,255,"outside scissored draw");
	glDisable(GL_SCISSOR_TEST);
	glPixelStorei(GL_PACK_ALIGNMENT,1);
	memset(out,99,sizeof out);
	glPixelStorei(GL_PACK_ROW_LENGTH,4);
	glPixelStorei(GL_PACK_SKIP_ROWS,1);
	glPixelStorei(GL_PACK_SKIP_PIXELS,1);
	glReadPixels(8,12,2,2,GL_RGB,GL_UNSIGNED_BYTE,out);
	for(i=0; i<sizeof out; i++){
		int expected = 99;
		if((i>=15 && i<21) || (i>=27 && i<33)) expected = i%3 == 1 ? 255 : 0;
		if(out[i] != expected) sysfatal("packed readback offset %d: %d",i,out[i]);
	}
	glPixelStorei(GL_PACK_ROW_LENGTH,0);
	glPixelStorei(GL_PACK_SKIP_ROWS,0);
	glPixelStorei(GL_PACK_SKIP_PIXELS,0);
	glReadPixels(8,12,1,1,GL_LUMINANCE,GL_UNSIGNED_BYTE,out);
	if(out[0] != 255) sysfatal("framebuffer luminance sum");
	glGenTextures(1,&t);
	glBindTexture(GL_TEXTURE_2D,t);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,input);
	list = glGenLists(1);
	glNewList(list,GL_COMPILE);
	glTexSubImage2D(GL_TEXTURE_2D,0,1,0,1,2,GL_RGBA,GL_UNSIGNED_BYTE,patch);
	glEndList();
	memset(patch,0,sizeof patch);
	glCallList(list);
	glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
	input[4] = input[5] = 255; input[6] = 0;
	input[12] = 0; input[13] = input[14] = 255;
	if(memcmp(out,input,sizeof input)) sysfatal("texture subimage/readback mismatch");
	glDeleteLists(list,1);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,packed);
	glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,packedout);
	if(memcmp(packed,packedout,sizeof packed)) sysfatal("RGB565 roundtrip");
	glGetTexImage(GL_TEXTURE_2D,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,out);
	if(out[0] != 255 || out[1] != 0 || out[2] != 0 || out[3] != 255)
		sysfatal("texture luminance extraction");
	/* The same saved pixels must convert to each destination's format. */
	input[3] = 17;
	input[7] = 91;
	glNewList(list=glGenLists(1),GL_COMPILE);
	glTexSubImage2D(GL_TEXTURE_2D,0,1,0,1,2,GL_RGBA,GL_UNSIGNED_BYTE,input);
	glTexSubImage2D(GL_TEXTURE_2D,0,2,2,0,0,GL_RGBA,GL_UNSIGNED_BYTE,nil);
	glEndList();
	glCallList(list);
	glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
	if(memcmp(out+4,input,3) || memcmp(out+12,input+4,3) || out[7] != 255 || out[15] != 255)
		sysfatal("RGB texture subimage conversion");
	memset(out,0,sizeof out);
	glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
	glCallList(list);
	glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
	if(memcmp(out,alpha,sizeof alpha)) sysfatal("alpha texture subimage conversion");
	memset(out,0,sizeof out);
	glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
	glCallList(list);
	glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
	if(memcmp(out,luma,sizeof luma)) sysfatal("luminance texture subimage conversion");
	glDeleteLists(list,1);
	glDeleteTextures(1,&t);
	clear();
	glColor3f(1,0,0);
	glLineWidth(6);
	glBegin(GL_LINES);
	glVertex2f(-.75,0); glVertex2f(.75,0);
	glEnd();
	readback();
	pixel(32,30,255,0,0,"wide line");
	pixel(32,26,0,0,255,"wide line bounds");
	clear();
	glLineWidth(1);
	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	triangle(0);
	readback();
	pixel(32,32,0,0,255,"wireframe interior");
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	print("PASS: scissored drawing and clears, color masks, packed readback and texture subimages\n");
}

void
main(int argc, char **argv)
{
	int i;
	uint list;

	if(argc > 2) sysfatal("usage: gltest [gpu-directory]");
	device = argc == 2 ? argv[1] : nil;
	tglinit(device, W, H);
	clear();
	readback();
	pixel(0, 0, 0, 0, 255, "clear");
	glColor3f(1, 0, 0);
	triangle(0);
	readback();
	pixel(32, 32, 255, 0, 0, "triangle");
	pixel(10, 10, 0, 0, 255, "top-left origin");
	pixel(12, 52, 255, 0, 0, "bottom of triangle");
	print("PASS: clear, triangle and image orientation\n");

	glEnable(GL_DEPTH_TEST);
	for(i=0; i<2; i++){
		clear();
		glColor3f(1, 0, 0);
		triangle(-.5);
		glColor3f(0, 1, 0);
		triangle(.5);
		readback();
		pixel(32, 32, 255, 0, 0, "depth");
		clear();
		glColor3f(0, 1, 0);
		triangle(.5);
		glColor3f(1, 0, 0);
		triangle(-.5);
		readback();
		pixel(32, 32, 255, 0, 0, "reverse depth");
	}
	glDisable(GL_DEPTH_TEST);
	glColor3f(0, 1, 0);
	triangle(.75);
	readback();
	pixel(32, 32, 0, 255, 0, "depth disabled");
	print("PASS: depth and draw ordering\n");

	clear();
	glEnable(GL_CULL_FACE);
	glColor3f(1, 0, 0);
	triangle(0);
	readback();
	pixel(32, 32, 255, 0, 0, "CCW front face");
	clear();
	glFrontFace(GL_CW);
	triangle(0);
	readback();
	pixel(32, 32, 0, 0, 255, "CW front face");
	glFrontFace(GL_CCW);
	glDisable(GL_CULL_FACE);
	print("PASS: face culling\n");

	clear();
	glViewport(0, 0, W/2, H/2);
	triangle(0);
	readback();
	pixel(16, 48, 255, 0, 0, "viewport origin");
	pixel(16, 16, 0, 0, 255, "outside viewport");
	glViewport(0, 0, W, H);
	print("PASS: viewport\n");

	clear();
	list = glGenLists(1);
	if(list == 0) sysfatal("zero display list");
	glNewList(list, GL_COMPILE);
	glColor3f(0, 1, 0);
	triangle(0);
	glEndList();
	readback();
	pixel(32, 32, 0, 0, 255, "compile without execution");
	glCallList(list);
	readback();
	pixel(32, 32, 0, 255, 0, "display list");
	glDeleteLists(list, 1);
	if(glIsList(list)) sysfatal("list not deleted");
	print("PASS: display list compilation, execution and deletion\n");
	clipplanes();
	matrices();
	geometry();
	streaming();
	lists();
	polygons();
	arrays();
	buffers();
	textures();
	/* Exceed gpu(3)'s 4096 resources and 256 MiB backing budget. */
	texturepressure(4128, 1);
	texturepressure(68, 1024);
	fragmentstate();
	vertexlayouts();
	texgenpoints();
	lighting();
	primitives();
	pixelops();
	errors();
	lifetime();
	exits(nil);
}
