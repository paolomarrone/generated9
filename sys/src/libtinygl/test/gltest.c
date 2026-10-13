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
	print("PASS: byte strides, color arrays and display list ownership\n");
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
	char *expected[] = {
		"unsupported capability", "readback buffer too small",
		"target size must", "bad display list id", "invalid viewport",
		"invalid operation inside glBegin", "display list nesting too deep",
	};

	for(i=0; i<nelem(expected); i++){
		pid = fork();
		if(pid < 0) sysfatal("fork: %r");
		if(pid == 0){
			fd = open("/dev/null", OWRITE);
			if(fd >= 0){ dup(fd, 2); close(fd); }
			switch(i){
			case 0: glEnable(GL_TEXTURE_2D); break;
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
	matrices();
	geometry();
	lists();
	polygons();
	arrays();
	lighting();
	primitives();
	errors();
	lifetime();
	exits(nil);
}
