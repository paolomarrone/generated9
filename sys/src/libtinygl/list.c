#include "zgl.h"

static char *op_table_str[]=
{
#define ADD_OP(a,b,c) "gl" #a " " c,

#include "opinfo.h"
};

static void (*op_table_func[])(GLContext *,GLParam *)=
{
#define ADD_OP(a,b,c) glop ## a ,

#include "opinfo.h"
};

static int op_table_size[]=
{
#define ADD_OP(a,b,c) b + 1 ,

#include "opinfo.h"
};


GLContext *gl_get_context(void)
{
  if (gl_ctx == NULL) gl_fatal_error("no current context");
  return gl_ctx;
}

static GLList *find_list(GLContext *c,unsigned int list)
{
  if (list >= MAX_DISPLAY_LISTS) return NULL;
  return c->shared_state.lists[list];
}

static void delete_list(GLContext *c,int list)
{
  GLParamBuffer *pb,*pb1;
  GLList *l;
  GLImage *image, *next;

  l=find_list(c,list);
  assert(l != NULL);

  /* free param buffer */
  pb=l->first_op_buffer;
  while (pb!=NULL) {
    pb1=pb->next;
    gl_free(pb);
    pb=pb1;
  }

  for(image=l->images; image!=nil; image=next) {
    next=image->next;
    free(image);
  }
  gl_free(l);
  c->shared_state.lists[list]=NULL;
}

void gl_print_op(int f,GLParam *p)
{
  int op;
  char *s;

  op=p[0].op;
  p++;
  s=op_table_str[op];
  while (*s != 0) {
    if (*s == '%') {
      s++;
      switch (*s++) {
      case 'f':
	fprint(f,"%g",p[0].f);
	break;
      case 'p':
	fprint(f,"%p",p[0].p);
	break;
      default:
	fprint(f,"%d",p[0].i);
	break;
      }
      p++;
    } else {
      fprint(f,"%c",*s);
      s++;
    }
  }
  fprint(f,"\n");
}


void gl_compile_op(GLContext *c,GLParam *p)
{
  int op,op_size;
  GLParamBuffer *ob,*ob1;
  int index,i;

  op=p[0].op;
  op_size=op_table_size[op];
  index=c->current_op_buffer_index;
  ob=c->current_op_buffer;

  /* we should be able to add a NextBuffer opcode */
  if ((index + op_size) > (OP_BUFFER_MAX_SIZE-2)) {

    ob1=gl_zalloc(sizeof(GLParamBuffer));
    ob1->next=NULL;

    ob->next=ob1;
    ob->ops[index].op=OP_NextBuffer;
    ob->ops[index+1].p=(void *)ob1;

    c->current_op_buffer=ob1;
    ob=ob1;
    index=0;
  }

  for(i=0;i<op_size;i++) {
    ob->ops[index]=p[i];
    index++;
  }
  c->current_op_buffer_index=index;
}

static void execute_op(GLContext *c,GLParam *p)
{
  if (c->in_begin) {
    switch(p[0].op) {
    case OP_Vertex: case OP_Color: case OP_Normal: case OP_TexCoord: case OP_MultiTexCoord:
    case OP_EdgeFlag: case OP_Material: case OP_CallList: case OP_End:
      break;
    default:
      gl_fatal_error("invalid operation inside glBegin");
    }
  }
  op_table_func[p[0].op](c,p);
}

void gl_add_op(GLParam *p)
{
  GLContext *c=gl_get_context();

  if (c->exec_flag) {
    execute_op(c,p);
  }
  if (c->compile_flag) {
    gl_compile_op(c,p);
  }
  if (c->print_flag) {
    gl_print_op(2,p);
  }
}

/* this opcode is never called directly */
void glopEndList(GLContext*,GLParam*)
{
  assert(0);
}

/* this opcode is never called directly */
void glopNextBuffer(GLContext*,GLParam*)
{
  assert(0);
}


void glopCallList(GLContext *c,GLParam *p)
{
  GLList *l;
  int list,op;

  list=p[1].ui;
  l=find_list(c,list);
  if (l == NULL || l->first_op_buffer == NULL) return;
  if (++c->list_depth > 64) gl_fatal_error("display list nesting too deep");
  p=l->first_op_buffer->ops;

  while (1) {
    op=p[0].op;
    if (op == OP_EndList) break;
    if (op == OP_NextBuffer) {
      p=(GLParam *)p[1].p;
    } else {
      execute_op(c,p);
      p+=op_table_size[op];
    }
  }
  c->list_depth--;
}



void glNewList(unsigned int list,int mode)
{
  GLList *l;
  GLContext *c=gl_get_context();

  if (list == 0 || list >= MAX_DISPLAY_LISTS) gl_fatal_error("bad display list id");
  assert(mode == GL_COMPILE || mode == GL_COMPILE_AND_EXECUTE);
  assert(c->compile_flag == 0 && !c->in_begin);

  /* Publish only at glEndList: calls and deletion still see the old list. */
  l=gl_zalloc(sizeof(GLList));
  l->first_op_buffer=gl_zalloc(sizeof(GLParamBuffer));
  c->compile_list=l;
  c->compile_list_id=list;

  c->current_op_buffer=l->first_op_buffer;
  c->current_op_buffer_index=0;

  c->compile_flag=1;
  c->exec_flag=(mode == GL_COMPILE_AND_EXECUTE);
}

void glEndList(void)
{
  GLContext *c=gl_get_context();
  GLParam p[1];

  assert(c->compile_flag == 1 && !c->in_begin);

  /* end of list */
  p[0].op=OP_EndList;
  gl_compile_op(c,p);

  if (find_list(c,c->compile_list_id)) delete_list(c,c->compile_list_id);
  c->shared_state.lists[c->compile_list_id]=c->compile_list;
  c->compile_list=NULL;
  c->current_op_buffer=NULL;
  c->compile_flag=0;
  c->exec_flag=1;
}

int glIsList(unsigned int list)
{
  GLContext *c=gl_get_context();
  GLList *l;
  assert(!c->in_begin);
  l=find_list(c,list);
  return (l != NULL && l->first_op_buffer != NULL);
}

unsigned int glGenLists(int range)
{
  GLContext *c=gl_get_context();
  int count,i,list;
  GLList **lists;

  assert(!c->in_begin);
  lists=c->shared_state.lists;
  count=0;
  if (range <= 0 || range >= MAX_DISPLAY_LISTS) return 0;
  for(i=1;i<MAX_DISPLAY_LISTS;i++) {
    if (lists[i]==NULL && (!c->compile_flag || i!=c->compile_list_id)) {
      count++;
      if (count == range) {
	list=i-range+1;
	for(i=0;i<range;i++) {
	  lists[list+i]=gl_zalloc(sizeof(GLList));
	}
	return list;
      }
    } else {
      count=0;
    }
  }
  return 0;
}


void glDeleteLists(unsigned int list, int range)
{
  GLContext *c=gl_get_context();
  unsigned int end;

  assert(!c->in_begin);
  if (range < 0) gl_fatal_error("negative display list range");
  if (list >= MAX_DISPLAY_LISTS) return;
  end=range > MAX_DISPLAY_LISTS-list ? MAX_DISPLAY_LISTS : list+range;
  for(;list<end;list++)
    if (find_list(c,list)) delete_list(c,list);
}
