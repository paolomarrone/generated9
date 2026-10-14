#include "zgl.h"

void glopNormal(GLContext * c, GLParam * p)
{
    V3 v;

    v.X = p[1].f;
    v.Y = p[2].f;
    v.Z = p[3].f;

    c->current_normal.X = v.X;
    c->current_normal.Y = v.Y;
    c->current_normal.Z = v.Z;
    c->current_normal.W = 0;
}

void glopTexCoord(GLContext * c, GLParam * p)
{
    c->texunit[0].coord.X = p[1].f;
    c->texunit[0].coord.Y = p[2].f;
    c->texunit[0].coord.Z = p[3].f;
    c->texunit[0].coord.W = p[4].f;
}

void glopMultiTexCoord(GLContext *c, GLParam *p)
{
    int i = p[1].i-GL_TEXTURE0;
    if(i < 0 || i >= MAX_TEXTURE_UNITS)
        gl_fatal_error("invalid texture unit");
    c->texunit[i].coord = gl_V4_New(p[2].f, p[3].f, p[4].f, p[5].f);
}

void glopEdgeFlag(GLContext * c, GLParam * p)
{
    c->current_edge_flag = p[1].i;
}

void glopColor(GLContext * c, GLParam * p)
{

    c->current_color.X = p[1].f;
    c->current_color.Y = p[2].f;
    c->current_color.Z = p[3].f;
    c->current_color.W = p[4].f;

    gl_color_material(c);
}


void glopBegin(GLContext * c, GLParam * p)
{
    int type, eye, i, j;
    M4 tmp;

    assert(c->in_begin == 0);

    type = p[1].i;
    if (type < GL_POINTS || type > GL_POLYGON) gl_fatal_error("invalid primitive");
    c->begin_type = type;
    c->in_begin = 1;
    c->vertex_n = 0;
    c->vertex_cnt = 0;

    eye = c->lighting_enabled || c->clip_enabled || type == GL_POINTS;
    for(i=0; i<MAX_TEXTURE_UNITS; i++)
        for(j=0; j<4; j++)
            if(c->texunit[i].enabled && c->texunit[i].gen[j]) eye=1;
    if(eye != c->need_eye) c->matrix_model_projection_updated=1;
    c->need_eye=eye;

    if (c->matrix_model_projection_updated) {

	if (c->need_eye) {
	    /* precompute inverse modelview */
	    gl_M4_Inv(&tmp, c->matrix_stack_ptr[0]);
	    gl_M4_Transpose(&c->matrix_model_view_inv, &tmp);
	} else {
	    float *m = &c->matrix_model_projection.m[0][0];
	    /* precompute projection matrix */
	    gl_M4_Mul(&c->matrix_model_projection,
		      c->matrix_stack_ptr[1],
		      c->matrix_stack_ptr[0]);
	    /* test to accelerate computation */
	    c->matrix_model_projection_no_w_transform = 0;
	    if (m[12] == 0.0 && m[13] == 0.0 && m[14] == 0.0)
		c->matrix_model_projection_no_w_transform = 1;
	}


	c->matrix_model_projection_updated = 0;
    }
    gl_begin(c);
}

/* Transform to eye and clip coordinates; VirGL clips and rasterizes. */
static void gl_vertex_transform(GLContext * c, GLVertex * v)
{
    float *m;
    V4 *n;

    if (c->need_eye) {
	/* eye coordinates needed for lighting */

	m = &c->matrix_stack_ptr[0]->m[0][0];
	v->ec.X = (v->coord.X * m[0] + v->coord.Y * m[1] +
		   v->coord.Z * m[2] + v->coord.W * m[3]);
	v->ec.Y = (v->coord.X * m[4] + v->coord.Y * m[5] +
		   v->coord.Z * m[6] + v->coord.W * m[7]);
	v->ec.Z = (v->coord.X * m[8] + v->coord.Y * m[9] +
		   v->coord.Z * m[10] + v->coord.W * m[11]);
	v->ec.W = (v->coord.X * m[12] + v->coord.Y * m[13] +
		   v->coord.Z * m[14] + v->coord.W * m[15]);

	/* projection coordinates */
	m = &c->matrix_stack_ptr[1]->m[0][0];
	v->pc.X = (v->ec.X * m[0] + v->ec.Y * m[1] +
		   v->ec.Z * m[2] + v->ec.W * m[3]);
	v->pc.Y = (v->ec.X * m[4] + v->ec.Y * m[5] +
		   v->ec.Z * m[6] + v->ec.W * m[7]);
	v->pc.Z = (v->ec.X * m[8] + v->ec.Y * m[9] +
		   v->ec.Z * m[10] + v->ec.W * m[11]);
	v->pc.W = (v->ec.X * m[12] + v->ec.Y * m[13] +
		   v->ec.Z * m[14] + v->ec.W * m[15]);

	m = &c->matrix_model_view_inv.m[0][0];
	n = &c->current_normal;

	v->normal.X = (n->X * m[0] + n->Y * m[1] + n->Z * m[2]);
	v->normal.Y = (n->X * m[4] + n->Y * m[5] + n->Z * m[6]);
	v->normal.Z = (n->X * m[8] + n->Y * m[9] + n->Z * m[10]);

	if (c->normalize_enabled) {
	    gl_V3_Norm(&v->normal);
	}
    } else {
	/* no eye coordinates needed, no normal */

	m = &c->matrix_model_projection.m[0][0];

	v->pc.X = (v->coord.X * m[0] + v->coord.Y * m[1] +
		   v->coord.Z * m[2] + v->coord.W * m[3]);
	v->pc.Y = (v->coord.X * m[4] + v->coord.Y * m[5] +
		   v->coord.Z * m[6] + v->coord.W * m[7]);
	v->pc.Z = (v->coord.X * m[8] + v->coord.Y * m[9] +
		   v->coord.Z * m[10] + v->coord.W * m[11]);
	if (c->matrix_model_projection_no_w_transform) {
	    v->pc.W = v->coord.W * m[15];
	} else {
	    v->pc.W = (v->coord.X * m[12] + v->coord.Y * m[13] +
		       v->coord.Z * m[14] + v->coord.W * m[15]);
	}
    }


}

void glopVertex(GLContext * c, GLParam * p)
{
    GLVertex *v;
    int n, i, cnt;

    assert(c->in_begin != 0);

    n = c->vertex_n;
    cnt = c->vertex_cnt;
    cnt++;
    c->vertex_cnt = cnt;

    /* new vertex entry */
    v = &c->vertex[n];
    n++;

    v->coord.X = p[1].f;
    v->coord.Y = p[2].f;
    v->coord.Z = p[3].f;
    v->coord.W = p[4].f;

    gl_vertex_transform(c, v);

    /* color */

    if (c->lighting_enabled) {
	gl_shade_vertex(c, v, 0, &v->color);
        if(c->light_model_two_side)
            gl_shade_vertex(c, v, 1, &v->back_color);
        else
            v->back_color=v->color;
    } else {
	v->color = c->current_color;
        v->back_color = v->color;
    }

    gl_texcoord(c,v);

    c->flat_color = v->color;
    c->flat_back_color = v->back_color;

    switch (c->begin_type) {
    case GL_POINTS:
	gl_draw_point(c, &c->vertex[0]);
	n = 0;
	break;

    case GL_LINES:
	if (n == 2) {
	    gl_draw_line(c, &c->vertex[0], &c->vertex[1]);
	    n = 0;
	}
	break;
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
	if (n == 1) {
	    c->vertex[2] = c->vertex[0];
	} else if (n == 2) {
	    gl_draw_line(c, &c->vertex[0], &c->vertex[1]);
	    c->vertex[0] = c->vertex[1];
	    n = 1;
	}
	break;

    case GL_TRIANGLES:
	if (n == 3) {
	    gl_draw_triangle(c, &c->vertex[0], &c->vertex[1], &c->vertex[2]);
	    n = 0;
	}
	break;
    case GL_TRIANGLE_STRIP:
	if (cnt >= 3) {
	    if (n == 3)
		n = 0;
            /* needed to respect triangle orientation */
            switch(cnt & 1) {
            case 0:
		gl_draw_triangle(c,&c->vertex[2],&c->vertex[1],&c->vertex[0]);
		break;
            default:
            case 1:
		gl_draw_triangle(c,&c->vertex[0],&c->vertex[1],&c->vertex[2]);
		break;
            }
	}
	break;
    case GL_POLYGON:
    case GL_TRIANGLE_FAN:
	if (n == 3) {
            if (c->begin_type == GL_POLYGON) {
                c->flat_color = c->vertex[0].color;
                c->flat_back_color = c->vertex[0].back_color;
            }
	    gl_draw_triangle(c, &c->vertex[0], &c->vertex[1], &c->vertex[2]);
	    c->vertex[1] = c->vertex[2];
	    n = 2;
	}
	break;

    case GL_QUADS:
	if (n == 4) {
	    gl_draw_triangle(c, &c->vertex[0], &c->vertex[1], &c->vertex[2]);
	    gl_draw_triangle(c, &c->vertex[0], &c->vertex[2], &c->vertex[3]);
	    n = 0;
	}
	break;

    case GL_QUAD_STRIP:
	if (n == 4) {
	    gl_draw_triangle(c, &c->vertex[0], &c->vertex[1], &c->vertex[2]);
	    gl_draw_triangle(c, &c->vertex[1], &c->vertex[3], &c->vertex[2]);
	    for (i = 0; i < 2; i++)
		c->vertex[i] = c->vertex[i + 2];
	    n = 2;
	}
	break;
    default:
	gl_fatal_error("glBegin: type %x not handled\n", c->begin_type);
    }

    c->vertex_n = n;
}

void glopEnd(GLContext * c, GLParam*)
{
    assert(c->in_begin == 1);

    if (c->begin_type == GL_LINE_LOOP) {
	if (c->vertex_cnt >= 2) {
            c->flat_color = c->vertex[2].color;
            c->flat_back_color = c->vertex[2].back_color;
	    gl_draw_line(c, &c->vertex[0], &c->vertex[2]);
	}
    }
    c->in_begin = 0;
}
