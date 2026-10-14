#ifndef _tgl_zgl_h_
#define _tgl_zgl_h_

#include <u.h>
#include <libc.h>
#include <GL/gl.h>
#include <tinygl.h>

#define NULL nil
#define M_PI PI
#include "zmath.h"


enum {

#define ADD_OP(a,b,c) OP_ ## a ,

#include "opinfo.h"

};

/* Max # of specular light pow buffers */
#define MAX_SPECULAR_BUFFERS 8
/* # of entries in specular buffer */
#define SPECULAR_BUFFER_SIZE 1024
/* specular buffer granularity */
#define SPECULAR_BUFFER_RESOLUTION 1024


#define MAX_MODELVIEW_STACK_DEPTH  32
#define MAX_PROJECTION_STACK_DEPTH 8
#define MAX_TEXTURE_STACK_DEPTH    8
#define MAX_TEXTURE_UNITS          4
#define MAX_TEXTURE_SIZE           2048
#define MAX_TEXTURE_LEVELS         12
#define NMATRIX (2+MAX_TEXTURE_UNITS)
#define MAX_LIGHTS                 16


#define MAX_DISPLAY_LISTS 1024
#define OP_BUFFER_MAX_SIZE 512


typedef struct GLSpecBuf {
  int shininess_i;
  int last_used;
  float buf[SPECULAR_BUFFER_SIZE+1];
  struct GLSpecBuf *next;
} GLSpecBuf;

typedef struct GLLight {
  V4 ambient;
  V4 diffuse;
  V4 specular;
  V4 position;
  V3 spot_direction;
  float spot_exponent;
  float spot_cutoff;
  float attenuation[3];
  /* precomputed values */
  float cos_spot_cutoff;
  V3 norm_spot_direction;
  V3 norm_position;
  /* we use a linked list to know which are the enabled lights */
  int enabled;
  struct GLLight *next,*prev;
} GLLight;

typedef struct GLMaterial {
  V4 emission;
  V4 ambient;
  V4 diffuse;
  V4 specular;
  float shininess;

  /* computed values */
  int shininess_i;
} GLMaterial;


typedef struct GLViewport {
  int xmin,ymin,xsize,ysize;
} GLViewport;

typedef union {
  int op;
  float f;
  int i;
  unsigned int ui;
  void *p;
} GLParam;

typedef struct GLParamBuffer {
  GLParam ops[OP_BUFFER_MAX_SIZE];
  struct GLParamBuffer *next;
} GLParamBuffer;

typedef struct GLPixelstore GLPixelstore;
struct GLPixelstore {
  int alignment, row_length, skip_rows, skip_pixels;
};

typedef struct GLImage GLImage;
struct GLImage {
  GLImage *next;
  int width, height, format;
  uchar pixels[1];
};

typedef struct GLTexture GLTexture;
struct GLTexture {
  GLTexture *next;
  GLuint name;
  int defined, min, mag, wrap[2], generate;
  GLImage *image[MAX_TEXTURE_LEVELS];
  u32int resource, view;
  uvlong used;
};

typedef struct GLTexenv GLTexenv;
struct GLTexenv {
  int mode, combine[2], source[2][3], operand[2][3], scale[2];
  V4 color;
};

typedef struct GLTexunit GLTexunit;
struct GLTexunit {
  int enabled, replace, gen[4], genmode[4];
  V4 objectplane[4], eyeplane[4];
  GLTexture *texture;
  V4 coord;
  GLTexenv env;
};

typedef struct GLList {
  GLParamBuffer *first_op_buffer;
  GLImage *images;
  /* TODO: extensions for an hash table or a better allocating scheme */
} GLList;

#define MAX_CLIP_PLANES 6

typedef struct GLVertex {
  V3 normal;
  V4 coord;
  V4 color, back_color;
  float point_size;
  V4 texcoord[MAX_TEXTURE_UNITS];

  /* computed values */
  V4 ec;                /* eye coordinates */
  V4 pc;                /* coordinates in the normalized volume */
} GLVertex;

/* shared state */

typedef struct GLSharedState {
  GLList **lists;
} GLSharedState;

/* Client storage is retained on the CPU while transforms run there. */
typedef struct GLBuffer GLBuffer;
struct GLBuffer {
  GLBuffer *next;
  GLuint name;
  int defined, usage;
  uintptr size;
  uchar *data;
};

enum { ArrayVertex, ArrayColor, ArrayNormal, ArrayTexcoord, Narray=ArrayTexcoord+MAX_TEXTURE_UNITS };
typedef struct GLArray GLArray;
struct GLArray {
  int enabled, size, type, stride;
  const void *pointer;
  GLBuffer *buffer;
};

/* display context */

typedef struct GLContext {
  /* lights */
  GLLight lights[MAX_LIGHTS];
  GLLight *first_light;
  V4 ambient_light_model;
  int local_light_model;
  int lighting_enabled;
  int light_model_two_side;

  /* materials */
  GLMaterial materials[2];
  int color_material_enabled;
  int current_color_material_mode;
  int current_color_material_type;

  /* shared state */
  GLSharedState shared_state;

  /* current list */
  GLList *compile_list;
  int compile_list_id;
  GLParamBuffer *current_op_buffer;
  int current_op_buffer_index;
  int exec_flag,compile_flag,print_flag;
  int list_depth;

  /* matrix */

  int matrix_mode;
  M4 *matrix_stack[NMATRIX];
  M4 *matrix_stack_ptr[NMATRIX];
  int matrix_stack_depth_max[NMATRIX];

  M4 matrix_model_view_inv;
  M4 matrix_model_projection;
  int matrix_model_projection_updated;
  int matrix_model_projection_no_w_transform;

  /* viewport */
  GLViewport viewport;

  /* current state */
  int polygon_mode_back;
  int polygon_mode_front;

  int current_front_face;
  int current_shade_model;
  int current_cull_face;
  int cull_face_enabled;
  int normalize_enabled;
  int clip_enabled;
  V4 clip_plane[MAX_CLIP_PLANES];

  /* clear */
  float clear_depth;
  V4 clear_color;

  /* current vertex state */
  V4 current_color;
  V4 flat_color, flat_back_color;
  V4 current_normal;
  GLTexture *textures;
  GLTexunit texunit[MAX_TEXTURE_UNITS];
  int active_texture, client_texture;
  GLPixelstore pack, unpack;
  int current_edge_flag;

  /* glBegin / glEnd */
  int in_begin;
  int begin_type;
  int vertex_n,vertex_cnt;
  GLVertex vertex[4];

  GLArray array[Narray];
  GLBuffer *buffers, *array_buffer, *element_buffer;

  /* specular buffer. could probably be shared between contexts,
    but that wouldn't be 100% thread safe */
  GLSpecBuf *specbuf_first;
  int specbuf_used_counter;
  int specbuf_num_buffers;

  /* native rasterizer */
  void *opaque;
  /* depth test */
  int depth_test, depth_func, depth_mask;
  int blend_enabled, blend_src, blend_dst, alpha_enabled, alpha_func;
  float alpha_ref, offset_factor, offset_units;
  int scissor_enabled, scissor[4], color_mask;
  int line_smooth;
  float line_width;
  int offset_fill, need_eye;
  int point_sprite;
  float point_size, point_min, point_max, point_fade, point_atten[3];
} GLContext;

extern GLContext *gl_ctx;

void gl_add_op(GLParam *p);

GLImage *gl_unpack(GLPixelstore*, int, int, int, int, const void*);
void gl_pack(GLPixelstore*, int, int, int, int, const uchar*, int, void*);
void gl_pixel_format(uchar*, int, int);

/* virgl.c */
void gl_primitive(GLContext*, int, GLVertex**, int);
void gl_draw_triangle(GLContext*, GLVertex*, GLVertex*, GLVertex*);
void gl_draw_line(GLContext*, GLVertex*, GLVertex*);
void gl_draw_point(GLContext*, GLVertex*);

/* matrix.c */
void gl_print_matrix(const float *m);

/* light.c */
void gl_color_material(GLContext *c);
void gl_enable_disable_light(GLContext *c,int light,int v);
void gl_shade_vertex(GLContext*, GLVertex*, int, V4*);
void gl_texcoord(GLContext*, GLVertex*);

GLContext *gl_get_context(void);

void gl_fatal_error(char *format, ...);
void *gl_malloc(int);
void *gl_zalloc(int);
void gl_free(void*);
void gl_free_buffers(GLContext*);
void gl_init_textures(GLContext*);
void gl_free_textures(GLContext*);
void gl_release_texture(GLContext*, GLTexture*);
int gl_texture_levels(GLTexture*);


/* specular buffer "api" */
GLSpecBuf *specbuf_get_buffer(GLContext *c, const int shininess_i,
                              const float shininess);

/* glopXXX functions */

#define ADD_OP(a,b,c) void glop ## a (GLContext *,GLParam *);
#include "opinfo.h"

void glInit(int, int);
void glClose(void);
void gl_flush(GLContext*);
void gl_clear(GLContext*, int);

#endif
