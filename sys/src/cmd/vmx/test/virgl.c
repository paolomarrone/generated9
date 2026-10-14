/* Run in a Linux graphical guest: reuse one timestamp query across submissions. */
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>

int
main(void)
{
	Display *d;
	XVisualInfo *v;
	XSetWindowAttributes a = {0};
	Window w;
	GLXContext c;
	GLuint q;
	GLuint64 previous, stamp;
	int i, stale;
	int attr[] = {GLX_RGBA, GLX_DOUBLEBUFFER, None};

	d = XOpenDisplay(NULL);
	if(d == NULL || (v = glXChooseVisual(d, DefaultScreen(d), attr)) == NULL){
		fprintf(stderr, "an X display with GLX is required\n");
		return 1;
	}
	a.colormap = XCreateColormap(d, RootWindow(d, v->screen), v->visual, AllocNone);
	w = XCreateWindow(d, RootWindow(d, v->screen), 0, 0, 320, 240, 0,
		v->depth, InputOutput, v->visual, CWColormap, &a);
	c = glXCreateContext(d, v, NULL, True);
	if(c == NULL || !glXMakeCurrent(d, w, c)){
		fprintf(stderr, "cannot create GLX context\n");
		return 1;
	}
	XMapWindow(d, w);
	printf("renderer: %s\n", glGetString(GL_RENDERER));
	glGenQueries(1, &q);
	previous = 0;
	stale = 0;
	for(i = 0; i < 300; i++){
		glClearColor((i%60)/60.0f, 0.3f, 0.6f, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glQueryCounter(q, GL_TIMESTAMP);
		stamp = 0;
		glGetQueryObjectui64v(q, GL_QUERY_RESULT, &stamp);
		if(stamp <= previous){
			if(stale < 8)
				fprintf(stderr, "query %d: %llu after %llu\n", i,
					(unsigned long long)stamp, (unsigned long long)previous);
			stale++;
		}
		previous = stamp;
		glXSwapBuffers(d, w);
	}
	if(glGetError() != GL_NO_ERROR){
		fprintf(stderr, "OpenGL error\n");
		return 1;
	}
	glDeleteQueries(1, &q);
	glXMakeCurrent(d, None, NULL);
	glXDestroyContext(d, c);
	XDestroyWindow(d, w);
	XFreeColormap(d, a.colormap);
	XFree(v);
	XCloseDisplay(d);
	printf("%s: 300 reused timestamp queries, %d stale results\n",
		stale == 0 ? "PASS" : "FAIL", stale);
	return stale != 0;
}
