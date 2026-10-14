typedef struct VIODev VIODev;
typedef struct VIOQueue VIOQueue;
typedef struct VIOBuf VIOBuf;
typedef struct VIONetDev VIONetDev;
typedef struct VIOBlkDev VIOBlkDev;

enum {
	BUFCHAIN = 1,
	BUFWR = 2,

	USEDNOIRQ = 1,

	DRIVEROK = 4, /* devstat */
};

struct VIOBuf {
	u32int flags;
	VIOQueue *qu;
	void *p;
	u64int addr;
	u32int len;
	u32int idx;
	VIOBuf *next, *head;
	u32int rptr, wptr;
};

struct VIOQueue {
	QLock;
	Rendez;
	VIODev *d;
	u8int (*desc)[16], *avail, *used;
	u16int size, maxsize, enabled;
	u64int descaddr, availaddr, usedaddr;
	u32int addr;
	u16int availidx, usedidx;
	void (*notify)(VIOQueue*);
	int livebuf;
	Rendez livebufrend;
};

struct VIONetDev {
	int readfd, writefd;
	u8int mac[6];
	enum {
		VNETPROMISC = 1,
		VNETALLMULTI = 2,
		VNETALLUNI = 4,
		VNETNOMULTI = 8,
		VNETNOUNI = 16,
		VNETNOBCAST = 32,

		VNETHEADER = 1<<31,
	} flags;
	u64int macbloom, multibloom;
};

struct VIOBlkDev {
	int fd;
	uvlong size;
};

struct VIODev {
	PCIDev *pci;
	u64int devfeat, guestfeat;
	u32int dfsel, gfsel;
	void *aux;
	u16int qsel;
	u8int devstat, isrstat;
	VIOQueue *qu;
	int nqu, allocqu;
	u32int (*io)(int, u16int, u32int, int, VIODev *);
	void (*reset)(VIODev *);
	union {
		VIONetDev net;
		VIOBlkDev blk;
	};
};

VIODev *mkviodev(u16int, u32int, u32int, int);
void viomodern(VIODev *);
VIOQueue *mkvioqueue(VIODev *, int, void (*)(VIOQueue*));
void viowakeup(VIOQueue *);
VIOBuf *viogetbuf(VIOQueue *, int);
void vioputbuf(VIOBuf *);
ulong vioqread(VIOBuf *, void *, ulong);
ulong vioqwrite(VIOBuf *, void *, ulong);
ulong vioqrem(VIOBuf *, int);
