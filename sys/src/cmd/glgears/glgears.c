/* 3-D gears by Brian Paul, public domain; from TinyGL 0.4.1.
 * Native window integration uses the generated9 root LICENSE.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <event.h>
#include <keyboard.h>
#include <GL/gl.h>
#include <tinygl.h>

#define M_PI PI

/*
 * Draw a gear wheel.  You'll probably want to call this function when
 * building a display list since we do a lot of trig here.
 *
 * Input:  inner_radius - radius of hole at center
 *         outer_radius - radius at center of teeth
 *         width - width of gear
 *         teeth - number of teeth
 *         tooth_depth - depth of tooth
 */
static void gear( GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
		  GLint teeth, GLfloat tooth_depth )
{
   GLint i;
   GLfloat r0, r1, r2;
   GLfloat angle, da;
   GLfloat u, v, len;

   r0 = inner_radius;
   r1 = outer_radius - tooth_depth/2.0;
   r2 = outer_radius + tooth_depth/2.0;

   da = 2.0*M_PI / teeth / 4.0;

   glShadeModel( GL_FLAT );

   glNormal3f( 0.0, 0.0, 1.0 );

   /* draw front face */
   glBegin( GL_QUAD_STRIP );
   for (i=0;i<=teeth;i++) {
      angle = i * 2.0*M_PI / teeth;
      glVertex3f( r0*cos(angle), r0*sin(angle), width*0.5 );
      glVertex3f( r1*cos(angle), r1*sin(angle), width*0.5 );
      glVertex3f( r0*cos(angle), r0*sin(angle), width*0.5 );
      glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), width*0.5 );
   }
   glEnd();

   /* draw front sides of teeth */
   glBegin( GL_QUADS );
   da = 2.0*M_PI / teeth / 4.0;
   for (i=0;i<teeth;i++) {
      angle = i * 2.0*M_PI / teeth;

      glVertex3f( r1*cos(angle),      r1*sin(angle),      width*0.5 );
      glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),   width*0.5 );
      glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da), width*0.5 );
      glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), width*0.5 );
   }
   glEnd();


   glNormal3f( 0.0, 0.0, -1.0 );

   /* draw back face */
   glBegin( GL_QUAD_STRIP );
   for (i=0;i<=teeth;i++) {
      angle = i * 2.0*M_PI / teeth;
      glVertex3f( r1*cos(angle), r1*sin(angle), -width*0.5 );
      glVertex3f( r0*cos(angle), r0*sin(angle), -width*0.5 );
      glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5 );
      glVertex3f( r0*cos(angle), r0*sin(angle), -width*0.5 );
   }
   glEnd();

   /* draw back sides of teeth */
   glBegin( GL_QUADS );
   da = 2.0*M_PI / teeth / 4.0;
   for (i=0;i<teeth;i++) {
      angle = i * 2.0*M_PI / teeth;

      glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5 );
      glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da), -width*0.5 );
      glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),   -width*0.5 );
      glVertex3f( r1*cos(angle),      r1*sin(angle),      -width*0.5 );
   }
   glEnd();


   /* draw outward faces of teeth */
   glBegin( GL_QUAD_STRIP );
   for (i=0;i<teeth;i++) {
      angle = i * 2.0*M_PI / teeth;

      glVertex3f( r1*cos(angle),      r1*sin(angle),       width*0.5 );
      glVertex3f( r1*cos(angle),      r1*sin(angle),      -width*0.5 );
      u = r2*cos(angle+da) - r1*cos(angle);
      v = r2*sin(angle+da) - r1*sin(angle);
      len = sqrt( u*u + v*v );
      u /= len;
      v /= len;
      glNormal3f( v, -u, 0.0 );
      glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),    width*0.5 );
      glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),   -width*0.5 );
      glNormal3f( cos(angle), sin(angle), 0.0 );
      glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da),  width*0.5 );
      glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da), -width*0.5 );
      u = r1*cos(angle+3*da) - r2*cos(angle+2*da);
      v = r1*sin(angle+3*da) - r2*sin(angle+2*da);
      glNormal3f( v, -u, 0.0 );
      glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da),  width*0.5 );
      glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5 );
      glNormal3f( cos(angle), sin(angle), 0.0 );
   }

   glVertex3f( r1*cos(0), r1*sin(0), width*0.5 );
   glVertex3f( r1*cos(0), r1*sin(0), -width*0.5 );

   glEnd();


   glShadeModel( GL_SMOOTH );

   /* draw inside radius cylinder */
   glBegin( GL_QUAD_STRIP );
   for (i=0;i<=teeth;i++) {
      angle = i * 2.0*M_PI / teeth;
      glNormal3f( -cos(angle), -sin(angle), 0.0 );
      glVertex3f( r0*cos(angle), r0*sin(angle), -width*0.5 );
      glVertex3f( r0*cos(angle), r0*sin(angle), width*0.5 );
   }
   glEnd();

}


static GLfloat view_rotx=20.0, view_roty=30.0, view_rotz=0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static void scene(void)
{
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   glPushMatrix();
   glRotatef( view_rotx, 1.0, 0.0, 0.0 );
   glRotatef( view_roty, 0.0, 1.0, 0.0 );
   glRotatef( view_rotz, 0.0, 0.0, 1.0 );

   glPushMatrix();
   glTranslatef( -3.0, -2.0, 0.0 );
   glRotatef( angle, 0.0, 0.0, 1.0 );
   glCallList(gear1);
   glPopMatrix();

   glPushMatrix();
   glTranslatef( 3.1, -2.0, 0.0 );
   glRotatef( -2.0*angle-9.0, 0.0, 0.0, 1.0 );
   glCallList(gear2);
   glPopMatrix();

   glPushMatrix();
   glTranslatef( -3.1, 4.2, 0.0 );
   glRotatef( -2.0*angle-25.0, 0.0, 0.0, 1.0 );
   glCallList(gear3);
   glPopMatrix();

   glPopMatrix();

}

static void reshape( int width, int height )
{
   GLfloat w = width > height ? (GLfloat)width/height : 1;
   GLfloat h = height > width ? (GLfloat)height/width : 1;

   glViewport(0, 0, (GLint)width, (GLint)height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glFrustum( -w, w, -h, h, 5.0, 60.0 );
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef( 0.0, 0.0, -40.0 );
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}


static void gearinit(void)
{
   static GLfloat pos[4] = {5.0, 5.0, 10.0, 0.0 };
   static GLfloat red[4] = {0.8, 0.1, 0.0, 1.0 };
   static GLfloat green[4] = {0.0, 0.8, 0.2, 1.0 };
   static GLfloat blue[4] = {0.2, 0.2, 1.0, 1.0 };

   glLightfv( GL_LIGHT0, GL_POSITION, pos );
   glEnable( GL_CULL_FACE );
   glEnable( GL_LIGHTING );
   glEnable( GL_LIGHT0 );
   glEnable( GL_DEPTH_TEST );

   /* make the gears */
   gear1 = glGenLists(1);
   glNewList(gear1, GL_COMPILE);
   glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red );
   gear( 1.0, 4.0, 1.0, 20, 0.7 );
   glEndList();

   gear2 = glGenLists(1);
   glNewList(gear2, GL_COMPILE);
   glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green );
   gear( 0.5, 2.0, 2.0, 10, 0.7 );
   glEndList();

   gear3 = glGenLists(1);
   glNewList(gear3, GL_COMPILE);
   glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue );
   gear( 1.3, 2.0, 0.5, 10, 0.7 );
   glEndList();

   glEnable( GL_NORMALIZE );
}


enum { Header = 24 };
static int width = 640, height = 480, resized;
static uchar *pixels;
static Image *picture;
static char caption[128] = "TinyGL gears | VirGL | space: pause  arrows: rotate  q: quit";

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	resized = 1;
}

static void
target(int w, int h)
{
	tglresize(w, h);
	width = w;
	height = h;
	free(pixels);
	pixels = malloc(w*h*4);
	if(pixels == nil)
		sysfatal("allocate pixels: %r");
	reshape(w, h);
}

static void
resize(void)
{
	int w, h;

	w = Dx(screen->r);
	h = Dy(screen->r)-Header;
	w = w < 64 ? 64 : w > 2048 ? 2048 : w;
	h = h < 64 ? 64 : h > 2048 ? 2048 : h;
	if(picture == nil || w != width || h != height){
		target(w, h);
		freeimage(picture);
		picture = allocimage(display, Rect(0, 0, w, h), XRGB32, 0, DNofill);
		if(picture == nil)
			sysfatal("allocate picture: %r");
	}
	draw(screen, screen->r, display->black, nil, ZP);
	resized = 0;
}

static void
render(void)
{
	scene();
	tglread(pixels, width*height*4);
}

static void
present(void)
{
	Rectangle r;
	Point at;

	if(loadimage(picture, picture->r, pixels, width*height*4) < 0)
		sysfatal("load frame: %r");
	at = addpt(screen->r.min, Pt((Dx(screen->r)-width)/2, Header));
	drawop(screen, rectaddpt(picture->r, at), picture, nil, ZP, S);
	r = screen->r;
	r.max.y = r.min.y+Header;
	draw(screen, r, display->black, nil, ZP);
	string(screen, addpt(r.min, Pt(6, 4)), display->white, ZP, font, caption);
	if(flushimage(display, 1) < 0)
		sysfatal("flush frame: %r");
}

static void
save(char *path)
{
	Memimage *m;
	int fd;

	memimageinit();
	m = allocmemimage(Rect(0, 0, width, height), XRGB32);
	if(m == nil || loadmemimage(m, m->r, pixels, width*height*4) < 0)
		sysfatal("load snapshot: %r");
	fd = create(path, OWRITE, 0666);
	if(fd < 0 || writememimage(fd, m) < 0)
		sysfatal("save snapshot: %r");
	close(fd);
	freememimage(m);
}

void
main(int argc, char **argv)
{
	Event ev;
	ulong timer, e;
	vlong start, last, now, report;
	int headless, limit, frames, since, paused;
	char *out;

	headless = limit = paused = 0;
	out = nil;
	ARGBEGIN{
	case 'b': headless = 1; /* fall through */
	case 'n':
		limit = atoi(EARGF(sysfatal("missing frame count")));
		if(limit <= 0)
			sysfatal("frame count must be positive");
		break;
	case 'o': out = EARGF(sysfatal("missing image path")); headless = 1; break;
	default: sysfatal("usage: glgears [-b frames | -n frames] [-o image]");
	}ARGEND
	if(argc != 0)
		sysfatal("usage: glgears [-b frames | -n frames] [-o image]");
	tglinit(nil, width, height);
	gearinit();
	if(headless){
		target(width, height);
		render();	/* exclude initial shader compilation */
		if(limit == 0)
			limit = 1;
		start = uptime();
		for(frames=0; frames<limit; frames++){
			angle += 2;
			render();
		}

	}else{
		if(initdraw(nil, nil, "TinyGL gears — VirGL") < 0)
			sysfatal("initdraw: %r");
		einit(Emouse|Ekeyboard);
		timer = etimer(0, 17);
		resize();
		render();
		present();
		frames = since = 0;
		start = last = report = uptime();
		for(;;){
			e = event(&ev);
			now = uptime();
			if(e == Ekeyboard){
				switch(ev.kbdc){
				case 'q': case 0x7f: goto Done;
				case ' ': paused = !paused; break;
				case Kup: view_rotx += 5; break;
				case Kdown: view_rotx -= 5; break;
				case Kleft: view_roty += 5; break;
				case Kright: view_roty -= 5; break;
				}
			}else if(!resized && (e != timer || paused)){
				if(paused)
					last = now;
				continue;
			}
			if(resized)
				resize();
			if(!paused)
				angle = fmod(angle+(now-last)*60e-9, 360);
			last = now;
			render();
			present();
			frames++;
			since++;
			if(now-report >= 1000000000LL){
				snprint(caption, sizeof(caption), "TinyGL gears | %.1f fps | space: pause  arrows: rotate  q: quit", since*1e9/(now-report));
				since = 0;
				report = now;
			}
			if(limit != 0 && frames >= limit)
				break;
		}
	}
Done:
	if(frames != 0)
		fprint(2, "%dx%d, %d frames: %.1f fps\n", width, height, frames, frames*1e9/(uptime()-start));
	if(out != nil)
		save(out);
	tglclose();
	free(pixels);
	freeimage(picture);
	exits(nil);
}
