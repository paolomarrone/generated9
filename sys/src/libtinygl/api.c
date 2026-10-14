#include "zgl.h"
/* glVertex */

void glVertex4f(float x,float y,float z,float w)
{
  GLParam p[5];

  p[0].op=OP_Vertex;
  p[1].f=x;
  p[2].f=y;
  p[3].f=z;
  p[4].f=w;

  gl_add_op(p);
}

void glVertex2f(float x,float y)
{
  glVertex4f(x,y,0,1);
}

void glVertex3f(float x,float y,float z)
{
  glVertex4f(x,y,z,1);
}

void glVertex3fv(float *v)
{
  glVertex4f(v[0],v[1],v[2],1);
}

/* glNormal */

void glNormal3f(float x,float y,float z)
{
  GLParam p[4];

  p[0].op=OP_Normal;
  p[1].f=x;
  p[2].f=y;
  p[3].f=z;

  gl_add_op(p);
}

void glNormal3fv(float *v)
{
  glNormal3f(v[0],v[1],v[2]);
}

/* glColor */

void glColor4f(float r,float g,float b,float a)
{
  GLParam p[5];

  p[0].op=OP_Color;
  p[1].f=r;
  p[2].f=g;
  p[3].f=b;
  p[4].f=a;
  gl_add_op(p);
}

void glColor4fv(float *v)
{
  GLParam p[5];

  p[0].op=OP_Color;
  p[1].f=v[0];
  p[2].f=v[1];
  p[3].f=v[2];
  p[4].f=v[3];
  gl_add_op(p);
}

void glColor3f(float x,float y,float z)
{
  glColor4f(x,y,z,1);
}

void glColor3fv(float *v)
{
  glColor4f(v[0],v[1],v[2],1);
}


/* TexCoord */

void glTexCoord4f(float s,float t,float r,float q)
{
  GLParam p[5];

  p[0].op=OP_TexCoord;
  p[1].f=s;
  p[2].f=t;
  p[3].f=r;
  p[4].f=q;

  gl_add_op(p);
}

void glTexCoord2f(float s,float t)
{
  glTexCoord4f(s,t,0,1);
}

void glTexCoord2fv(float *v)
{
  glTexCoord4f(v[0],v[1],0,1);
}

void glEdgeFlag(int flag)
{
  GLParam p[2];

  p[0].op=OP_EdgeFlag;
  p[1].i=flag;

  gl_add_op(p);
}

/* misc */

void glShadeModel(int mode)
{
  GLParam p[2];

  assert(mode == GL_FLAT || mode == GL_SMOOTH);

  p[0].op=OP_ShadeModel;
  p[1].i=mode;

  gl_add_op(p);
}

void glCullFace(int mode)
{
  GLParam p[2];

  assert(mode == GL_BACK ||
         mode == GL_FRONT ||
         mode == GL_FRONT_AND_BACK);

  p[0].op=OP_CullFace;
  p[1].i=mode;

  gl_add_op(p);
}

void glFrontFace(int mode)
{
  GLParam p[2];

  assert(mode == GL_CCW || mode == GL_CW);

  mode = (mode != GL_CCW);

  p[0].op=OP_FrontFace;
  p[1].i=mode;

  gl_add_op(p);
}

void glPolygonMode(int face,int mode)
{
  GLParam p[3];

  assert(face == GL_BACK ||
         face == GL_FRONT ||
         face == GL_FRONT_AND_BACK);
  assert(mode == GL_POINT || mode == GL_LINE || mode==GL_FILL);

  p[0].op=OP_PolygonMode;
  p[1].i=face;
  p[2].i=mode;

  gl_add_op(p);
}


/* glEnable / glDisable */

void glEnable(int cap)
{
  GLParam p[3];

  p[0].op=OP_EnableDisable;
  p[1].i=cap;
  p[2].i=1;

  gl_add_op(p);
}

void glDisable(int cap)
{
  GLParam p[3];

  p[0].op=OP_EnableDisable;
  p[1].i=cap;
  p[2].i=0;

  gl_add_op(p);
}

/* glBegin / glEnd */

void glBegin(int mode)
{
  GLParam p[2];

  p[0].op=OP_Begin;
  p[1].i=mode;

  gl_add_op(p);
}

void glEnd(void)
{
  GLParam p[1];

  p[0].op=OP_End;

  gl_add_op(p);
}

/* matrix */

void glMatrixMode(int mode)
{
  GLParam p[2];

  p[0].op=OP_MatrixMode;
  p[1].i=mode;

  gl_add_op(p);
}

void glLoadMatrixf(const float *m)
{
  GLParam p[17];
  int i;

  p[0].op=OP_LoadMatrix;
  for(i=0;i<16;i++) p[i+1].f=m[i];

  gl_add_op(p);
}

void glLoadIdentity(void)
{
  GLParam p[1];

  p[0].op=OP_LoadIdentity;

  gl_add_op(p);
}

void glMultMatrixf(const float *m)
{
  GLParam p[17];
  int i;

  p[0].op=OP_MultMatrix;
  for(i=0;i<16;i++) p[i+1].f=m[i];

  gl_add_op(p);
}

void glPushMatrix(void)
{
  GLParam p[1];

  p[0].op=OP_PushMatrix;

  gl_add_op(p);
}

void glPopMatrix(void)
{
  GLParam p[1];

  p[0].op=OP_PopMatrix;

  gl_add_op(p);
}

void glRotatef(float angle,float x,float y,float z)
{
  GLParam p[5];

  p[0].op=OP_Rotate;
  p[1].f=angle;
  p[2].f=x;
  p[3].f=y;
  p[4].f=z;

  gl_add_op(p);
}

void glTranslatef(float x,float y,float z)
{
  GLParam p[4];

  p[0].op=OP_Translate;
  p[1].f=x;
  p[2].f=y;
  p[3].f=z;

  gl_add_op(p);
}

void glScalef(float x,float y,float z)
{
  GLParam p[4];

  p[0].op=OP_Scale;
  p[1].f=x;
  p[2].f=y;
  p[3].f=z;

  gl_add_op(p);
}


void glViewport(int x,int y,int width,int height)
{
  GLParam p[5];

  p[0].op=OP_Viewport;
  p[1].i=x;
  p[2].i=y;
  p[3].i=width;
  p[4].i=height;

  gl_add_op(p);
}

void glFrustum(double left,double right,double bottom,double top,
               double near,double farv)
{
  GLParam p[7];

  p[0].op=OP_Frustum;
  p[1].f=left;
  p[2].f=right;
  p[3].f=bottom;
  p[4].f=top;
  p[5].f=near;
  p[6].f=farv;

  gl_add_op(p);
}

/* lightening */

void glMaterialfv(int mode,int type,float *v)
{
  GLParam p[7];
  int i,n;

  assert(mode == GL_FRONT  || mode == GL_BACK || mode==GL_FRONT_AND_BACK);

  p[0].op=OP_Material;
  p[1].i=mode;
  p[2].i=type;
  n=4;
  if (type == GL_SHININESS) n=1;
  for(i=0;i<n;i++) p[3+i].f=v[i];
  for(i=n;i<4;i++) p[3+i].f=0;

  gl_add_op(p);
}

void glMaterialf(int mode,int type,float v)
{
  GLParam p[7];
  int i;

  p[0].op=OP_Material;
  p[1].i=mode;
  p[2].i=type;
  p[3].f=v;
  for(i=0;i<3;i++) p[4+i].f=0;

  gl_add_op(p);
}

void glColorMaterial(int mode,int type)
{
  GLParam p[3];

  p[0].op=OP_ColorMaterial;
  p[1].i=mode;
  p[2].i=type;

  gl_add_op(p);
}

void glLightfv(int light,int type,float *v)
{
  GLParam p[7];
  int i,n;

  p[0].op=OP_Light;
  p[1].i=light;
  p[2].i=type;
  n=4;
  if (type == GL_SPOT_DIRECTION) n=3;
  else if (type == GL_SPOT_EXPONENT || type == GL_SPOT_CUTOFF ||
           type == GL_CONSTANT_ATTENUATION || type == GL_LINEAR_ATTENUATION ||
           type == GL_QUADRATIC_ATTENUATION) n=1;
  for(i=0;i<n;i++) p[3+i].f=v[i];
  for(i=n;i<4;i++) p[3+i].f=0;

  gl_add_op(p);
}


void glLightf(int light,int type,float v)
{
  GLParam p[7];
  int i;

  p[0].op=OP_Light;
  p[1].i=light;
  p[2].i=type;
  p[3].f=v;
  for(i=0;i<3;i++) p[4+i].f=0;

  gl_add_op(p);
}

void glLightModeli(int pname,int param)
{
  GLParam p[6];
  int i;

  p[0].op=OP_LightModel;
  p[1].i=pname;
  p[2].f=(float)param;
  for(i=0;i<3;i++) p[3+i].f=0;

  gl_add_op(p);
}

void glLightModelfv(int pname,float *param)
{
  GLParam p[6];
  int i,n;

  p[0].op=OP_LightModel;
  p[1].i=pname;
  n=pname == GL_LIGHT_MODEL_AMBIENT ? 4 : 1;
  for(i=0;i<n;i++) p[2+i].f=param[i];
  for(i=n;i<4;i++) p[2+i].f=0;

  gl_add_op(p);
}

/* clear */

void glClear(int mask)
{
  GLParam p[2];

  p[0].op=OP_Clear;
  p[1].i=mask;

  gl_add_op(p);
}

void glClearColor(float r,float g,float b,float a)
{
  GLParam p[5];

  p[0].op=OP_ClearColor;
  p[1].f=r;
  p[2].f=g;
  p[3].f=b;
  p[4].f=a;

  gl_add_op(p);
}

void glClearDepth(double depth)
{
  GLParam p[2];

  p[0].op=OP_ClearDepth;
  p[1].f=depth;

  gl_add_op(p);
}


/* textures */

void glBindTexture(int target,int texture)
{
  GLParam p[3];

  p[0].op=OP_BindTexture;
  p[1].i=target;
  p[2].i=texture;

  gl_add_op(p);
}

void glTexEnvi(int target,int pname,int param)
{
  GLParam p[8];

  p[0].op=OP_TexEnv;
  p[1].i=target;
  p[2].i=pname;
  p[3].i=param;
  p[4].f=0;
  p[5].f=0;
  p[6].f=0;
  p[7].f=0;

  gl_add_op(p);
}

void glTexParameteri(int target,int pname,int param)
{
  GLParam p[8];

  p[0].op=OP_TexParameter;
  p[1].i=target;
  p[2].i=pname;
  p[3].i=param;
  p[4].f=0;
  p[5].f=0;
  p[6].f=0;
  p[7].f=0;

  gl_add_op(p);
}

void glPixelStorei(int pname,int param)
{
  GLParam p[3];

  p[0].op=OP_PixelStore;
  p[1].i=pname;
  p[2].i=param;

  glopPixelStore(gl_get_context(), p);
}

/* selection */

void glInitNames(void)
{
  GLParam p[1];

  p[0].op=OP_InitNames;

  gl_add_op(p);
}

void glPushName(unsigned int name)
{
  GLParam p[2];

  p[0].op=OP_PushName;
  p[1].i=name;

  gl_add_op(p);
}

void glPopName(void)
{
  GLParam p[1];

  p[0].op=OP_PopName;

  gl_add_op(p);
}

void glLoadName(unsigned int name)
{
  GLParam p[2];

  p[0].op=OP_LoadName;
  p[1].i=name;

  gl_add_op(p);
}

void
glPolygonOffset(GLfloat factor, GLfloat units)
{
  GLParam p[3];
  p[0].op = OP_PolygonOffset;
  p[1].f = factor;
  p[2].f = units;

  gl_add_op(p);
}

/* Special Functions */

void glCallList(unsigned int list)
{
  GLParam p[2];

  p[0].op=OP_CallList;
  p[1].i=list;

  gl_add_op(p);
}

void glHint(int target,int mode)
{
  GLParam p[3];

  p[0].op=OP_Hint;
  p[1].i=target;
  p[2].i=mode;

  gl_add_op(p);
}

/* Non standard functions */

void glDebug(int mode)
{
  GLContext *c=gl_get_context();
  c->print_flag=mode;
}

void
glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
	GLdouble near, GLdouble far)
{
	float m[16];

	if(left == right || bottom == top || near == far)
		gl_fatal_error("invalid orthographic projection");
	memset(m, 0, sizeof(m));
	m[0] = 2/(right-left);
	m[5] = 2/(top-bottom);
	m[10] = -2/(far-near);
	m[12] = -(right+left)/(right-left);
	m[13] = -(top+bottom)/(top-bottom);
	m[14] = -(far+near)/(far-near);
	m[15] = 1;
	glMultMatrixf(m);
}

void
glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	glColor4f(r/255.f, g/255.f, b/255.f, a/255.f);
}

void
glLightModelf(GLenum name, GLfloat v)
{
	GLParam p[6];

	memset(p, 0, sizeof(p));
	p[0].op = OP_LightModel;
	p[1].i = name;
	p[2].f = v;
	gl_add_op(p);
}

void
glVertex2i(int x, int y)
{
	glVertex2f(x, y);
}
