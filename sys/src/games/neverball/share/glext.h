/*
 * Copyright (C) 2003-2011 Neverball authors
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
#ifndef GLEXT_H
#define GLEXT_H

#include <GL/gl.h>

#define glClientActiveTexture_ glClientActiveTexture
#define glActiveTexture_ glActiveTexture
#define glGenBuffers_ glGenBuffers
#define glBindBuffer_ glBindBuffer
#define glBufferData_ glBufferData
#define glBufferSubData_ glBufferSubData
#define glDeleteBuffers_ glDeleteBuffers
#define glIsBuffer_ glIsBuffer
#define glPointParameterf_ glPointParameterf
#define glPointParameterfv_ glPointParameterfv
#define glOrtho_ glOrtho
#define glStringMarker_(s) ((void)(s))

void glClipPlane4f_(GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
int glext_init(void);
struct gl_info {
    GLint max_texture_units, max_texture_size;
};
extern struct gl_info gli;
#endif
