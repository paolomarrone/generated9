/* Native scalar math support for raymath and the raylib sources. */
#ifndef RAYMATH9_H
#define RAYMATH9_H
#define cosf(x) ((float)cos(x))
#define sinf(x) ((float)sin(x))
#define tanf(x) ((float)tan(x))
#define acosf(x) ((float)acos(x))
#define asinf(x) ((float)asin(x))
#define atan2f(x,y) ((float)atan2(x,y))
#define sqrtf(x) ((float)sqrt(x))
#define floorf(x) ((float)floor(x))
#define ceilf(x) ((float)ceil(x))
#define fabsf(x) ((float)fabs(x))
#define fmodf(x,y) ((float)fmod(x,y))
#define powf(x,y) ((float)pow(x,y))
#define expf(x) ((float)exp(x))
#define logf(x) ((float)log(x))
static double round(double x) { return x == 0 ? x : x < 0 ? ceil(x-.5) : floor(x+.5); }
#define roundf(x) ((float)round(x))
static float truncf(float x) { return x == 0 ? x : x < 0 ? ceil(x) : floor(x); }
static double fmin(double x, double y) { return isNaN(y) || x < y ? x : y; }
static double fmax(double x, double y) { return isNaN(y) || x > y ? x : y; }
#define fminf(x,y) ((float)fmin(x,y))
#define fmaxf(x,y) ((float)fmax(x,y))
#define hypotf(x,y) ((float)hypot(x,y))
static int p9signbit(double x)
{
	union { double d; uvlong u; } v;
	v.d = x;
	return v.u >> 63;
}
#define signbit(x) p9signbit(x)
#define isnan(x) isNaN(x)
#define isinf(x) p9isinf(x)
static int p9isinf(double x) { return isInf(x,1) || isInf(x,-1); }
static int isfinite(double x) { return !isNaN(x) && !isInf(x,1) && !isInf(x,-1); }
#endif
