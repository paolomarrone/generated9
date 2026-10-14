/* Synchronous VirGL file client; see gpu(2). */
#pragma lib "libgpu.a"
enum {
	GpuCreate = 1, GpuBind = 2, GpuDestroy = 3, GpuViewport = 4,
	GpuFramebuffer = 5, GpuVertices = 6, GpuClearbuf = 7, GpuDraw = 8,
	GpuSamplerViews = 10, GpuScissor = 15, GpuSamplerStates = 18,
	GpuIndices = 11, GpuConstants = 12, GpuBindshader = 31,
	GpuBlend = 1, GpuRasterizer = 2, GpuDsa = 3, GpuShader = 4,
	GpuElements = 5, GpuSamplerView = 6, GpuSamplerState = 7, GpuSurface = 8,
	GpuBgra = 1, GpuZ32 = 18, GpuFloat4 = 31,
};

typedef struct Gpu Gpu;
struct Gpu {
	int ctl, cmd, slot;
	char *root;
	int ncommand;
	u32int command[2048];
};

void gpuinit(Gpu*, char*);
void gpufini(Gpu*);
u32int gpuresource(Gpu*, char*);
u32int gputryresource(Gpu*, char*);
int gpuopen(Gpu*, u32int, int);
u32int *gpuemit(Gpu*, int, int, int);
void gpusubmit(Gpu*);
void gpubind(Gpu*, int, int);
void gpushader(Gpu*, int, int, char*);
u32int gpufloat(float);
