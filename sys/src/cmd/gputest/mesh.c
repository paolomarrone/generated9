/*
 * Copyright (c) 1999-2000 Pawel W. Olszta. All Rights Reserved.
 * Written by Pawel W. Olszta, <olszta@sourceforge.net>
 * Creation date: Fri Dec 24 1999
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PAWEL W. OLSZTA BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* Native tessellation of the freeglut patches; see UPSTREAM. */
#include <u.h>
#include <libc.h>
#include "teapot.h"

enum { Steps = 12, Side = Steps+1 };

static void
basis(float t, float *b, float *d)
{
	float s;

	s = 1-t;
	b[0] = s*s*s;
	b[1] = 3*t*s*s;
	b[2] = 3*t*t*s;
	b[3] = t*t*t;
	d[0] = -3*s*s;
	d[1] = 3*s*s-6*t*s;
	d[2] = 6*t*s-3*t*t;
	d[3] = 3*t*t;
}

/* Expand the ten input patches into 32, retaining analytic smooth normals. */
void
teapotmesh(float (**vertices)[8], ushort **indices, int *nv, int *ni)
{
	float b[Side][4], d[Side][4], base[Side*Side][8];
	float (*v)[8], *q, p[3], du[3], dv[3], n[3], cp[3], len, x, z;
	ushort *idx;
	int patchno, copy, copies, u, w, i, j, k, a, c, off, at, reverse;

	*nv = 32*Side*Side;
	*ni = 32*Steps*Steps*6;
	v = malloc(*nv*sizeof(*v));
	idx = malloc(*ni*sizeof(*idx));
	if(v == nil || idx == nil)
		sysfatal("allocate mesh: %r");
	for(i=0; i<Side; i++)
		basis((float)i/Steps, b[i], d[i]);
	off = at = 0;
	for(patchno=0; patchno<nelem(patch); patchno++){
		for(u=0; u<Side; u++)
			for(w=0; w<Side; w++){
				memset(p, 0, sizeof(p));
				memset(du, 0, sizeof(du));
				memset(dv, 0, sizeof(dv));
				for(i=0; i<4; i++)
					for(j=0; j<4; j++){
						q = controlpoint[patch[patchno][4*i+j]];
						cp[0] = q[0]*.5;
						cp[1] = (q[2]-1.575)*.5;
						cp[2] = -q[1]*.5;
						for(k=0; k<3; k++){
							p[k] += b[u][i]*b[w][j]*cp[k];
							du[k] += d[u][i]*b[w][j]*cp[k];
							dv[k] += b[u][i]*d[w][j]*cp[k];
						}
					}
				for(k=0; k<3; k++)
					n[k] = dv[(k+1)%3]*du[(k+2)%3]-dv[(k+2)%3]*du[(k+1)%3];
				/* The collapsed rows at the lid and base are poles. */
				if(u == 0 && (patchno == 3 || patchno == 5)){
					n[0] = n[2] = 0;
					n[1] = patchno == 3 ? 1 : -1;
				}
				len = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
				if(len < 1e-12)
					sysfatal("degenerate teapot normal");
				q = base[u*Side+w];
				for(k=0; k<3; k++){
					q[k] = p[k];
					q[k+4] = n[k]/len;
				}
				q[3] = 1;
				q[7] = 0;
			}
		copies = patchno < 6 ? 4 : 2;
		for(copy=0; copy<copies; copy++){
			for(i=0; i<Side*Side; i++){
				q = v[off+i];
				memmove(q, base[i], sizeof(*v));
				for(k=0; k<8; k+=4){
					x = q[k];
					z = q[k+2];
					if(copies == 2){
						if(copy == 1)
							q[k+2] = -z;
					}else switch(copy){
					case 1: q[k] = z; q[k+2] = -x; break;
					case 2: q[k] = -x; q[k+2] = -z; break;
					case 3: q[k] = -z; q[k+2] = x; break;
					}
				}
			}
			reverse = copies == 2 && copy == 1;
			for(u=0; u<Steps; u++)
				for(w=0; w<Steps; w++){
					a = off+u*Side+w;
					c = a+Side+1;
					idx[at++] = a;
					idx[at++] = reverse ? a+Side : c;
					idx[at++] = reverse ? c : a+Side;
					idx[at++] = a;
					idx[at++] = reverse ? c : a+1;
					idx[at++] = reverse ? a+1 : c;
				}
			off += Side*Side;
		}
	}
	*vertices = v;
	*indices = idx;
}
