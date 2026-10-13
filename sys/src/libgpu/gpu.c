/* VirGL encoding adapted from virglrenderer; see LICENSE and UPSTREAM. */
#include <u.h>
#include <libc.h>
#include <gpu.h>

static u32int
info(Gpu *g)
{
	char buf[128], *f[3];
	int n;

	n = pread(g->ctl, buf, sizeof(buf)-1, 0);
	if(n <= 0)
		sysfatal("read gpu ctl: %r");
	buf[n] = 0;
	if(tokenize(buf, f, nelem(f)) != 3)
		sysfatal("bad gpu ctl");
	g->slot = atoi(f[0]);
	return strtoul(f[2], nil, 10);
}

void
gpuinit(Gpu *g, char *root)
{
	char *path;

	memset(g, 0, sizeof(*g));
	g->root = strdup(root == nil ? "#∇" : root);
	if(g->root == nil)
		sysfatal("gpu path: %r");
	path = smprint("%s/clone", g->root);
	if(path == nil)
		sysfatal("gpu path: %r");
	g->ctl = open(path, ORDWR);
	free(path);
	if(g->ctl < 0)
		sysfatal("open gpu: %r");
	info(g);
	path = smprint("%s/%d/cmd", g->root, g->slot);
	if(path == nil)
		sysfatal("gpu path: %r");
	g->cmd = open(path, OWRITE);
	free(path);
	if(g->cmd < 0)
		sysfatal("open gpu commands: %r");
}

void
gpufini(Gpu *g)
{
	close(g->cmd);
	close(g->ctl);
	g->cmd = g->ctl = -1;
	free(g->root);
	g->root = nil;
}

u32int
gpuresource(Gpu *g, char *desc)
{
	if(fprint(g->ctl, "resource %s", desc) < 0)
		sysfatal("gpu resource: %r");
	return info(g);
}

int
gpuopen(Gpu *g, u32int id, int mode)
{
	char *path;
	int fd;

	path = smprint("%s/%d/%ud", g->root, g->slot, id);
	if(path == nil)
		sysfatal("gpu path: %r");
	fd = open(path, mode);
	if(fd < 0)
		sysfatal("open %s: %r", path);
	free(path);
	return fd;
}

u32int
gpufloat(float f)
{
	union { float f; u32int u; } v;

	v.f = f;
	return v.u;
}

u32int*
gpuemit(Gpu *g, int op, int object, int n)
{
	u32int *p;

	if(n < 0 || n > nelem(g->command)-g->ncommand-1)
		sysfatal("gpu command buffer full");
	g->command[g->ncommand++] = op | object<<8 | n<<16;
	p = g->command+g->ncommand;
	memset(p, 0, n*4);
	g->ncommand += n;
	return p;
}

void
gpusubmit(Gpu *g)
{
	if(g->ncommand == 0)
		return;
	if(write(g->cmd, g->command, g->ncommand*4) != g->ncommand*4)
		sysfatal("gpu submit: %r");
	g->ncommand = 0;
}

void
gpubind(Gpu *g, int type, int id)
{
	gpuemit(g, GpuBind, type, 1)[0] = id;
}

void
gpushader(Gpu *g, int id, int type, char *text)
{
	u32int *p;
	int n;

	n = strlen(text)+1;
	p = gpuemit(g, GpuCreate, GpuShader, 5+(n+3)/4);
	p[0] = id;
	p[1] = type;
	p[2] = n;
	p[3] = 512;	/* maximum TGSI tokens, including declarations */
	memmove(p+5, text, n);
	p = gpuemit(g, GpuBindshader, 0, 2);
	p[0] = id;
	p[1] = type;
}
