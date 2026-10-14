#include "zgl.h"

void
glClipPlane(GLenum plane, const GLdouble *equation)
{
	GLParam p[6];
	int i;

	p[0].op = OP_ClipPlane;
	p[1].i = plane;
	for(i=0; i<4; i++) p[i+2].f = equation[i];
	gl_add_op(p);
}

void
glopClipPlane(GLContext *c, GLParam *p)
{
	M4 inverse;
	V4 *v;
	int i, j, plane;

	plane = p[1].i-GL_CLIP_PLANE0;
	if(plane < 0 || plane >= MAX_CLIP_PLANES)
		gl_fatal_error("invalid clip plane");
	/* Equations are captured in eye space when the command executes. */
	gl_M4_Inv(&inverse,c->matrix_stack_ptr[0]);
	v = &c->clip_plane[plane];
	for(i=0; i<4; i++){
		v->v[i] = 0;
		for(j=0; j<4; j++) v->v[i] += inverse.m[j][i]*p[j+2].f;
	}
}

void
glGetClipPlane(GLenum plane, GLdouble *equation)
{
	GLContext *c;
	int i;

	c = gl_get_context();
	assert(!c->in_begin);
	if(plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0+MAX_CLIP_PLANES)
		gl_fatal_error("invalid clip plane");
	for(i=0; i<4; i++) equation[i] = c->clip_plane[plane-GL_CLIP_PLANE0].v[i];
}

static float
distance(V4 *plane, GLVertex *v)
{
	return plane->X*v->ec.X + plane->Y*v->ec.Y
		+ plane->Z*v->ec.Z + plane->W*v->ec.W;
}

static V4
lerp(V4 a, V4 b, float t)
{
	V4 v;
	int i;

	for(i=0; i<4; i++) v.v[i] = a.v[i]+t*(b.v[i]-a.v[i]);
	return v;
}

static GLVertex
intersect(GLVertex *a, GLVertex *b, float da, float db)
{
	GLVertex v;
	float t;
	int i;

	t = da/(da-db);
	v = *a;
	v.ec = lerp(a->ec,b->ec,t);
	v.coord = lerp(a->coord,b->coord,t);
	v.color = lerp(a->color,b->color,t);
	v.back_color = lerp(a->back_color,b->back_color,t);
	for(i=0; i<MAX_TEXTURE_UNITS; i++)
		v.texcoord[i] = lerp(a->texcoord[i],b->texcoord[i],t);
	return v;
}

void
gl_draw_triangle(GLContext *c, GLVertex *a, GLVertex *b, GLVertex *d)
{
	/* Clipping a triangle against N planes adds at most N vertices. */
	GLVertex buf[2][3+MAX_CLIP_PLANES], *in, *out, *v[3], *prev;
	float dp, dq;
	int n, m, i, j, side;

	v[0] = a; v[1] = b; v[2] = d;
	if(c->clip_enabled == 0){
		gl_primitive(c,v,3);
		return;
	}
	buf[0][0] = *a; buf[0][1] = *b; buf[0][2] = *d;
	n = 3;
	side = 0;
	for(i=0; i<MAX_CLIP_PLANES && n; i++){
		if(!(c->clip_enabled & (1<<i))) continue;
		in = buf[side];
		out = buf[side^1];
		prev = &in[n-1];
		dp = distance(&c->clip_plane[i],prev);
		m = 0;
		for(j=0; j<n; j++){
			dq = distance(&c->clip_plane[i],&in[j]);
			if((dp < 0) != (dq < 0))
				out[m++] = intersect(prev,&in[j],dp,dq);
			if(dq >= 0) out[m++] = in[j];
			prev = &in[j];
			dp = dq;
		}
		n = m;
		side ^= 1;
	}
	v[0] = &buf[side][0];
	for(i=1; i+1<n; i++){
		v[1] = &buf[side][i]; v[2] = &buf[side][i+1];
		gl_primitive(c,v,3);
	}
}

void
gl_draw_line(GLContext *c, GLVertex *a, GLVertex *b)
{
	GLVertex ends[2], *v[2];
	float da, db;
	int i;

	if(c->clip_enabled == 0){
		v[0] = a; v[1] = b;
		gl_primitive(c,v,2);
		return;
	}
	ends[0] = *a; ends[1] = *b;
	for(i=0; i<MAX_CLIP_PLANES; i++){
		if(!(c->clip_enabled & (1<<i))) continue;
		da = distance(&c->clip_plane[i],&ends[0]);
		db = distance(&c->clip_plane[i],&ends[1]);
		if(da < 0 && db < 0) return;
		if((da < 0) != (db < 0))
			ends[da < 0 ? 0 : 1] = intersect(&ends[0],&ends[1],da,db);
	}
	v[0] = &ends[0]; v[1] = &ends[1];
	gl_primitive(c,v,2);
}

void
gl_draw_point(GLContext *c, GLVertex *a)
{
	int i;

	for(i=0; i<MAX_CLIP_PLANES; i++)
		if((c->clip_enabled & (1<<i)) && distance(&c->clip_plane[i],a) < 0)
			return;
	gl_primitive(c,&a,1);
}
