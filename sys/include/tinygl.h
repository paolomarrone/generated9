/* One current context per process.  Errors are fatal; see tinygl(2). */
#pragma lib "libtinygl.a"
void tglinit(char *device, int width, int height);
void tglresize(int width, int height);
void tglread(void *pixels, int bytes);
void tglclose(void);
