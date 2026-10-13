/* Private VirGL helpers shared by the native GPU examples; see UPSTREAM. */
enum {
	Create = 1, Bind = 2, Destroy = 3, Viewport = 4,
	Framebuffer = 5, Vertices = 6, Clearbuf = 7, Draw = 8,
	Indices = 11, Constants = 12, Bindshader = 31,
	Blend = 1, Rasterizer = 2, Dsa = 3, Shader = 4,
	Elements = 5, Surface = 8,
	Bgra = 1, Z32 = 18, Float4 = 31,
};

typedef struct Gpu Gpu;
struct Gpu {
	int ctl, cmd, slot;
	int ncommand;
	u32int command[2048];
};

void gpuinit(Gpu*);
void gpufini(Gpu*);
u32int gpuresource(Gpu*, char*);
int gpuopen(Gpu*, u32int, int);
u32int *gpuemit(Gpu*, int, int, int);
void gpusubmit(Gpu*);
void gpubind(Gpu*, int, int);
void gpushader(Gpu*, int, int, char*);
u32int gpufloat(float);
