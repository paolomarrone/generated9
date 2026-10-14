/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */


#include "port.h"
#include "video.h"
#include "common.h"
#include "image.h"
#include "vec3.h"
#include "glext.h"
#include "config.h"
#include "gui.h"

extern const char TITLE[];
extern const char ICON[];

struct video video;

#include "port.h"
#include "video.h"
#include "common.h"
#include "vec3.h"
#include "glext.h"
#include "config.h"
#include "hmd.h"

void video_calc_view(float *M, const float *c,
                               const float *p,
                               const float *u)
{
    float x[3];
    float y[3];
    float z[3];

    v_sub(z, p, c);
    v_nrm(z, z);
    v_crs(x, u, z);
    v_nrm(x, x);
    v_crs(y, z, x);

    m_basis(M, x, y, z);
}

/*---------------------------------------------------------------------------*/

void video_push_persp(float fov, float n, float f)
{
    if (hmd_stat())
        hmd_persp(n, f);
    else
    {
        GLfloat m[4][4];

        GLfloat r = fov / 2 * V_PI / 180;
        GLfloat s = fsinf(r);
        GLfloat c = fcosf(r) / s;

        GLfloat a = ((GLfloat) video.device_w /
                     (GLfloat) video.device_h);

        glMatrixMode(GL_PROJECTION);
        {
            glLoadIdentity();

            m[0][0] = c / a;
            m[0][1] =  0.0f;
            m[0][2] =  0.0f;
            m[0][3] =  0.0f;
            m[1][0] =  0.0f;
            m[1][1] =     c;
            m[1][2] =  0.0f;
            m[1][3] =  0.0f;
            m[2][0] =  0.0f;
            m[2][1] =  0.0f;
            m[2][2] = -(f + n) / (f - n);
            m[2][3] = -1.0f;
            m[3][0] =  0.0f;
            m[3][1] =  0.0f;
            m[3][2] = -2.0f * n * f / (f - n);
            m[3][3] =  0.0f;

            glMultMatrixf(&m[0][0]);
        }
        glMatrixMode(GL_MODELVIEW);
        {
            glLoadIdentity();
        }
    }
}

void video_push_ortho(void)
{
    if (hmd_stat())
        hmd_ortho();
    else
    {
        GLfloat w = (GLfloat) video.device_w;
        GLfloat h = (GLfloat) video.device_h;

        glMatrixMode(GL_PROJECTION);
        {
            glLoadIdentity();
            glOrtho_(0.0, w, 0.0, h, -1.0, +1.0);
        }
        glMatrixMode(GL_MODELVIEW);
        {
            glLoadIdentity();
        }
    }
}

void video_pop_matrix(void)
{
}

void video_clear(void)
{
    if (config_get_d(CONFIG_REFLECTION))
        glClear(GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT |
                GL_STENCIL_BUFFER_BIT);
    else
        glClear(GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT);
}

/*---------------------------------------------------------------------------*/
