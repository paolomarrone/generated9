/* Included by unit.c: exercise the sound device through real split queues. */
static uchar *sndmem;
static uvlong sndbase;

static void
sndsubmit(VIODev *v, int qi, void *data, int n, int nr)
{
	VIOQueue *q;
	uchar *p;
	uvlong base;
	int idx;

	q = v->qu+qi;
	p = sndmem+qi*BY2PG;
	base = sndbase+qi*BY2PG;
	idx = 2*(GET16(q->avail,2) % (q->size/2));
	assert(n <= 64 && nr <= 64);
	memmove(p+1024+idx*64,data,n);
	memset(p+1024+(idx+1)*64,0xff,nr);
	PUT64(p,idx*16,base+1024+idx*64);
	PUT32(p,idx*16+8,n);
	PUT16(p,idx*16+12,BUFCHAIN);
	PUT16(p,idx*16+14,idx+1);
	PUT64(p,(idx+1)*16,base+1024+(idx+1)*64);
	PUT32(p,(idx+1)*16+8,nr);
	PUT16(p,(idx+1)*16+12,BUFWR);
	PUT16(q->avail,4+2*(GET16(q->avail,2)%q->size),idx);
	coherence();
	PUT16(q->avail,2,GET16(q->avail,2)+1);
	q->notify(q);
}

static void
sndwait(VIOQueue *q, int count)
{
	int i;

	for(i = 0; GET16(q->used,2) != count && i < 5000; i++) sleep(1);
	assert(GET16(q->used,2) == count);
	coherence();
}

static uchar *
sndcommand(VIODev *v, void *h, int n, u32int status)
{
	uchar *r;
	int idx, count;

	count = GET16(v->qu[0].avail,2);
	idx = 2*(count % (v->qu[0].size/2));
	r = sndmem+1024+(idx+1)*64;
	sndsubmit(v,0,h,n,64);
	sndwait(v->qu,count+1);
	assert(GET32(r,0) == status);
	return r;
}

static void
sndsimple(VIODev *v, u32int cmd, u32int status)
{
	uchar h[8];

	PUT32(h,0,cmd);
	PUT32(h,4,0);
	sndcommand(v,h,sizeof h,status);
}

static void
sndsetup(VIODev *v)
{
	int i;

	memset(sndmem,0,4*BY2PG);
	vioreg(v,0,8,1,4);
	vioreg(v,0,12,1,4);
	vioreg(v,0,20,11,1);
	for(i = 0; i < 4; i++){
		vioreg(v,0,22,i,2);
		vioreg(v,0,24,32,2);
		vioreg(v,0,32,sndbase+i*BY2PG,4);
		vioreg(v,0,40,sndbase+i*BY2PG+512,4);
		vioreg(v,0,48,sndbase+i*BY2PG+640,4);
		vioreg(v,0,28,1,2);
		PUT16(v->qu[i].avail,0,USEDNOIRQ);
	}
	vioreg(v,0,20,15,1);
}

static void
sound(void)
{
	extern PCIDev *pcidevs;
	VIODev *v;
	Region *reg;
	uchar h[24], pcm[20], data[16], *r;
	char path[64];
	int fd, i, n;
	VmxNotif note;

	snprint(path,sizeof path,"/tmp/vmx-sound.%d",getpid());
	fd = create(path,ORDWR|ORCLOSE,0600);
	assert(fd >= 0 && mkviosnd(path) == 0);
	v = pcidevs->bar[0].aux;
	v->pci->irqno = 255;
	assert(v->pci->viddid == 0x10591af4);
	assert(vioreg(v,1,132,0,4) == 1 && vioreg(v,1,136,0,1) == 1);
	reg = mkregion(0x80010000,0x80014000,REGRWX);
	sndmem = reg->v = emalloc(4*BY2PG);
	reg->ve = sndmem+4*BY2PG;
	sndbase = reg->start;
	sndsetup(v);
	memset(h,0,sizeof h);
	PUT32(h,0,0x100); PUT32(h,8,1); PUT32(h,12,32);
	r = sndcommand(v,h,16,0x8000);
	assert(GET64(r,12) == 1ULL<<5 && GET64(r,20) == 1ULL<<6);
	assert(r[28] == 0 && r[29] == 2 && r[30] == 2);
	PUT32(h,4,0xffffffff);
	sndcommand(v,h,16,0x8001);
	PUT32(h,4,0); PUT32(h,0,0x200); PUT32(h,12,24);
	r = sndcommand(v,h,16,0x8000);
	assert(r[9] == 2 && r[10] == 3 && r[11] == 4);
	sndcommand(v,h,3,0x8001);
	sndsimple(v,0xdead,0x8002);
	sndsimple(v,0x104,0x8001);	/* START before PREPARE */
	memset(h,0,sizeof h);
	PUT32(h,0,0x101); PUT32(h,8,64); PUT32(h,12,16);
	h[20] = 2; h[21] = 5; h[22] = 7;
	sndcommand(v,h,24,0x8002);
	h[22] = 6; PUT32(h,12,15);
	sndcommand(v,h,24,0x8001);
	PUT32(h,12,16);
	sndcommand(v,h,24,0x8000);
	sndsimple(v,0x102,0x8000);
	PUT32(pcm,0,0);
	for(i = 4; i < sizeof pcm; i++) pcm[i] = i;
	sndsubmit(v,2,pcm,sizeof pcm,8);
	sleep(10);
	assert(GET16(v->qu[2].used,2) == 0);
	sndsimple(v,0x104,0x8000);
	sndwait(v->qu+2,1);
	assert(pread(fd,data,sizeof data,0) == sizeof data);
	assert(memcmp(data,pcm+4,sizeof data) == 0);
	sndcommand(v,h,24,0x8001);	/* parameters cannot change while running */
	sndsimple(v,0x105,0x8000);
	sndsubmit(v,2,pcm,sizeof pcm,8);
	sleep(10);
	assert(GET16(v->qu[2].used,2) == 1);
	sndsimple(v,0x104,0x8000);
	sndwait(v->qu+2,2);
	sndsubmit(v,2,pcm,19,8);	/* incomplete frame */
	sndwait(v->qu+2,3);
	r = sndmem+2*BY2PG+1024+5*64;
	assert(GET32(r,0) == 0x8003);
	sndsimple(v,0x105,0x8000);
	/* Fill the monitor channel: RELEASE must publish every completion first. */
	notifch = chancreate(sizeof(VmxNotif),16);
	memset(&note,0,sizeof note);
	for(i = 0; i < 16; i++) send(notifch,&note);
	PUT16(v->qu[2].avail,0,0);
	for(i = 0; i < 16; i++) sndsubmit(v,2,pcm,sizeof pcm,8);
	sndsimple(v,0x103,0x8000);
	sndwait(v->qu+2,19);
	/* Reset must not wait for the worker blocked on that notification. */
	vioreg(v,0,20,0,1);
	assert(v->qu[2].desc == nil && v->qu[2].livebuf == 0);
	n = 0;
	while(n < 17){
		recv(notifch,&note);
		if(note.f != nil) note.f(note.arg);
		n++;
	}
	assert(v->isrstat == 0);
	/* A failed host open is reported, and START can be retried. */
	close(fd);
	sndsetup(v);
	sndsimple(v,0x104,0x8001);
	sndcommand(v,h,24,0x8000);
	sndsimple(v,0x102,0x8000);
	sndsimple(v,0x104,0x8003);
	fd = create(path,ORDWR|ORCLOSE,0600);
	assert(fd >= 0);
	sndsimple(v,0x104,0x8000);
	sndsimple(v,0x105,0x8000);
	sndsubmit(v,2,pcm,sizeof pcm,8);
	vioreg(v,0,20,0,1);
	assert(seek(fd,0,2) == 0);
	close(fd);
	print("PASS: VirtIO sound discovery, PCM bytes, stop/start, release and reset with a full notification channel\n");
}
