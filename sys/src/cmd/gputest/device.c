#include <u.h>
#include <libc.h>

static void
check(int ok, char *what)
{
	if(!ok)
		sysfatal("%s: %r", what);
}

static void
info(int ctl, int *slot, u32int *id)
{
	char buf[80], *f[3];
	int n;

	n = pread(ctl, buf, sizeof(buf)-1, 0);
	check(n > 0, "read ctl");
	buf[n] = 0;
	check(tokenize(buf, f, 3) == 3, "parse ctl");
	*slot = atoi(f[0]);
	*id = strtoul(f[2], nil, 10);
}

void
main(void)
{
	int ctl, other, data, cmd, i, slot, fds[32];
	Dir *d;
	u32int id, fresh, word;
	char path[80];
	uchar a[64], b[64];

	data = open("#∇/caps", OREAD);
	check(data >= 0, "open capabilities");
	d = dirfstat(data);
	check(d != nil && d->length >= sizeof(word), "capability size");
	check(pread(data, &word, sizeof(word), 0) == sizeof(word) && word != 0, "read capability version");
	check(pread(data, b, 1, d->length) == 0, "capability EOF");
	free(d);
	close(data);
	check(open("#∇/caps", OWRITE) < 0, "capabilities are read only");
	ctl = open("#∇/clone", ORDWR);
	check(ctl >= 0, "clone");
	check(write(ctl, "resource", 8) < 0, "reject incomplete resource");
	check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 0") < 0, "reject zero backing");
	check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 67108865") < 0, "reject excess backing");
	check(fprint(ctl, "resource 99 64 16 64 1 1 1 0 0 0 64") < 0, "reject invalid target");
	check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 64") > 0, "create buffer");
	info(ctl, &slot, &id);
	snprint(path, sizeof(path), "#∇/%d/%ud", slot, id);
	data = open(path, ORDWR);
	check(data >= 0, "open buffer");
	memset(a, 0, sizeof(a));
	check(pread(data, b, sizeof(b), 0) == sizeof(b) && memcmp(a, b, sizeof(a)) == 0, "zeroed backing");
	check(pread(data, b, 1, 64) == 0, "buffer EOF");
	check(pwrite(data, b, 2, 63) < 0, "reject buffer overflow");
	for(i=0; i<sizeof(a); i++)
		a[i] = i*3+1;
	check(pwrite(data, a, sizeof(a), 0) == sizeof(a), "write backing");
	check(fprint(ctl, "upload %ud 0 0 0 0 64 1 1 0 0 0", id) > 0, "upload buffer");
	memset(b, 0, sizeof(b));
	check(pwrite(data, b, sizeof(b), 0) == sizeof(b), "clear backing");
	check(fprint(ctl, "download %ud 0 0 0 0 64 1 1 0 0 0", id) > 0, "download buffer");
	check(pread(data, b, sizeof(b), 0) == sizeof(b) && memcmp(a, b, sizeof(a)) == 0, "GPU buffer round trip");
	check(fprint(ctl, "download %ud 0 0 0 0 64 1 1 64 0 0", id) < 0, "reject transfer offset");
	other = open("#∇/clone", ORDWR);
	check(other >= 0, "second context");
	check(fprint(other, "unref %ud", id) < 0, "resource ownership");
	close(other);
	snprint(path, sizeof(path), "#∇/%d/cmd", slot);
	cmd = open(path, OWRITE);
	check(cmd >= 0, "open commands");
	word = 0;
	check(write(cmd, &word, 3) < 0, "reject unaligned commands");
	check(write(cmd, &word, sizeof(word)) == sizeof(word), "submit NOP");
	close(cmd);
	check(fprint(ctl, "unref %ud", id) > 0, "unref buffer");
	check(pread(data, b, 1, 0) < 0, "reject stale resource");
	check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 64") > 0, "reuse resource slot");
	info(ctl, &slot, &fresh);
	check(fresh != id, "unique resource identifier");
	check(pwrite(data, a, 1, 0) < 0, "stale descriptor cannot write replacement");
	close(data);
	snprint(path, sizeof(path), "#∇/%d/%ud", slot, fresh);
	data = open(path, OREAD);
	check(data >= 0, "open replacement");
	close(ctl);
	check(read(data, b, sizeof(b)) == sizeof(b), "resource keeps context alive");
	close(data);
	check(open(path, OREAD) < 0, "last close removes context");
	ctl = open("#∇/clone", ORDWR);
	check(ctl >= 0, "clone for resource limit");
	for(i=0; i<256; i++)
		check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 64") > 0, "allocate resource table");
	check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 64") < 0, "resource limit");
	info(ctl, &slot, &id);
	check(fprint(ctl, "unref %ud", id) > 0, "release full table entry");
	check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 64") > 0, "reuse full table entry");
	close(ctl);
	for(i=0; i<nelem(fds); i++){
		fds[i] = open("#∇/clone", ORDWR);
		check(fds[i] >= 0, "allocate context table");
	}
	check(open("#∇/clone", ORDWR) < 0, "context limit");
	for(i=0; i<nelem(fds); i++)
		close(fds[i]);
	for(i=0; i<100; i++){
		ctl = open("#∇/clone", ORDWR);
		check(ctl >= 0, "reuse context slot");
		check(fprint(ctl, "resource 0 64 16 64 1 1 1 0 0 0 64") > 0, "create owned resource");
		close(ctl);
	}
	print("PASS: GPU transfers, bounds, ownership, stale descriptors, limits and lifetime\n");
	exits(nil);
}
