#include <u.h>
#include <libc.h>
#include <sqlite3.h>

enum { Maxpath = 4096 };

typedef struct File File;
struct File {
	sqlite3_file base;
	int fd;
	int lock;
	const char *name;	/* SQLite owns named paths until xClose. */
	Qid qid;
	ushort type;
	uint dev;
};

static int
fsclose(sqlite3_file *file)
{
	File *f = (File*)file;
	int rc;

	rc = close(f->fd);
	f->base.pMethods = nil;
	return rc < 0 ? SQLITE_IOERR_CLOSE : SQLITE_OK;
}

static int
fsread(sqlite3_file *file, void *buf, int n, sqlite3_int64 off)
{
	File *f = (File*)file;
	char *p = buf;
	long nr;

	while(n > 0){
		nr = pread(f->fd, p, n, off);
		if(nr < 0)
			return SQLITE_IOERR_READ;
		if(nr == 0){
			memset(p, 0, n);
			return SQLITE_IOERR_SHORT_READ;
		}
		p += nr;
		n -= nr;
		off += nr;
	}
	return SQLITE_OK;
}

static int
fswrite(sqlite3_file *file, const void *buf, int n, sqlite3_int64 off)
{
	File *f = (File*)file;
	const char *p = buf;
	long nw;

	while(n > 0){
		nw = pwrite(f->fd, p, n, off);
		if(nw <= 0)
			return SQLITE_IOERR_WRITE;
		p += nw;
		n -= nw;
		off += nw;
	}
	return SQLITE_OK;
}

static int
fstruncate(sqlite3_file *file, sqlite3_int64 size)
{
	File *f = (File*)file;
	Dir d;

	nulldir(&d);
	d.length = size;
	return dirfwstat(f->fd, &d) < 0 ? SQLITE_IOERR_TRUNCATE : SQLITE_OK;
}

static int
syncfd(int fd)
{
	Dir d;

	/* Request a stable-storage barrier, as in stat(5). */
	nulldir(&d);
	return dirfwstat(fd, &d);
}

static int
fssync(sqlite3_file *file, int)
{
	return syncfd(((File*)file)->fd) < 0 ? SQLITE_IOERR_FSYNC : SQLITE_OK;
}

static int
fssize(sqlite3_file *file, sqlite3_int64 *size)
{
	Dir *d;

	d = dirfstat(((File*)file)->fd);
	if(d == nil)
		return SQLITE_IOERR_FSTAT;
	*size = d->length;
	free(d);
	return SQLITE_OK;
}

/* DMEXCL holds the database for the entire connection, including idle time. */
static int
fslock(sqlite3_file *file, int level)
{
	File *f = (File*)file;

	if(level > f->lock)
		f->lock = level;
	return SQLITE_OK;
}

static int
fsunlock(sqlite3_file *file, int level)
{
	((File*)file)->lock = level;
	return SQLITE_OK;
}

static int
fsreserved(sqlite3_file *file, int *reserved)
{
	*reserved = ((File*)file)->lock >= SQLITE_LOCK_RESERVED;
	return SQLITE_OK;
}

static int
fscontrol(sqlite3_file *file, int op, void *arg)
{
	File *f = (File*)file;
	Dir *d;

	switch(op){
	case SQLITE_FCNTL_LOCKSTATE:
		*(int*)arg = f->lock;
		return SQLITE_OK;
	case SQLITE_FCNTL_HAS_MOVED:
		if(f->name == nil)
			return SQLITE_NOTFOUND;
		d = dirstat(f->name);
		*(int*)arg = d == nil || d->qid.path != f->qid.path
			|| d->type != f->type || d->dev != f->dev;
		free(d);
		return SQLITE_OK;
	}
	return SQLITE_NOTFOUND;
}

static int
fssector(sqlite3_file*)
{
	return 4096;
}

static int
fscharacteristics(sqlite3_file*)
{
	return 0;
}

static sqlite3_io_methods methods = {
	1, fsclose, fsread, fswrite, fstruncate, fssync, fssize,
	fslock, fsunlock, fsreserved, fscontrol, fssector, fscharacteristics,
};

static int
fsopen(sqlite3_vfs*, const char *name, sqlite3_file *file, int flags, int *out)
{
	File *f = (File*)file;
	Dir *d;
	char tmp[128], *err;
	uvlong random;
	int fd, mode, immutable, i;
	ulong perm;

	f->base.pMethods = nil;
	immutable = name != nil && (flags & SQLITE_OPEN_MAIN_DB)
		&& sqlite3_uri_boolean(name, "immutable", 0);
	mode = (flags & SQLITE_OPEN_READWRITE) && !immutable ? ORDWR : OREAD;
	mode |= OCEXEC;
	if(flags & SQLITE_OPEN_DELETEONCLOSE)
		mode |= ORCLOSE;
	perm = flags & SQLITE_OPEN_MAIN_DB ? DMEXCL|0666 : 0600;
	fd = -1;
	if(name == nil){
		for(i = 0; i < 16 && fd < 0; i++){
			sqlite3_randomness(sizeof(random), &random);
			snprint(tmp, sizeof(tmp), "/tmp/sqlite-%16.16llux", random);
			fd = create(tmp, mode|OEXCL, perm);
		}
	}else{
		if(!(flags & SQLITE_OPEN_EXCLUSIVE)){
			fd = open(name, mode);
			if(fd < 0 && (mode & 3) == ORDWR){
				fd = open(name, (mode & ~3) | OREAD);
				if(fd >= 0)
					mode = (mode & ~3) | OREAD;
			}
		}
		if(fd < 0 && (flags & SQLITE_OPEN_CREATE))
			fd = create(name, mode|OEXCL, perm);
	}
	if(fd < 0)
		return SQLITE_CANTOPEN;
	d = dirfstat(fd);
	if(d == nil){
		close(fd);
		return SQLITE_IOERR_FSTAT;
	}
	if((d->mode & (DMDIR|DMAPPEND)) ||
	   ((flags & SQLITE_OPEN_MAIN_DB) && !immutable && !(d->mode & DMEXCL))){
		err = d->mode & (DMDIR|DMAPPEND) ? "sqlite: directory or append-only file"
			: "sqlite: database requires an exclusive-use file (chmod +l)";
		free(d);
		close(fd);
		werrstr("%s", err);
		return SQLITE_CANTOPEN;
	}
	f->qid = d->qid;
	f->type = d->type;
	f->dev = d->dev;
	free(d);
	f->name = name;
	f->fd = fd;
	f->lock = SQLITE_LOCK_NONE;
	f->base.pMethods = &methods;
	if(out != nil)
		*out = (flags & ~(SQLITE_OPEN_READONLY|SQLITE_OPEN_READWRITE))
			| ((mode & 3) == OREAD ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE);
	return SQLITE_OK;
}

static int
fsdelete(sqlite3_vfs*, const char *name, int syncdir)
{
	char *path, *p;
	int fd, rc;

	if(remove(name) < 0)
		return SQLITE_IOERR_DELETE;
	if(!syncdir)
		return SQLITE_OK;
	path = strdup(name);
	if(path == nil)
		return SQLITE_NOMEM;
	p = strrchr(path, '/');
	if(p == nil){
		free(path);
		return SQLITE_IOERR_DIR_FSYNC;
	}
	if(p == path)
		p++;
	*p = 0;
	fd = open(path, OREAD|OCEXEC);
	free(path);
	if(fd < 0)
		return SQLITE_IOERR_DIR_FSYNC;
	rc = syncfd(fd);
	close(fd);
	return rc < 0 ? SQLITE_IOERR_DIR_FSYNC : SQLITE_OK;
}

static int
fsaccess(sqlite3_vfs*, const char *name, int flags, int *result)
{
	int mode;

	mode = AEXIST;
	if(flags == SQLITE_ACCESS_READ)
		mode = AREAD;
	else if(flags == SQLITE_ACCESS_READWRITE)
		mode = AREAD|AWRITE;
	*result = access(name, mode) == 0;
	return SQLITE_OK;
}

static int
fsfullpath(sqlite3_vfs*, const char *name, int n, char *path)
{
	int len;

	path[0] = 0;
	if(name[0] != '/' && name[0] != '#'){
		if(getwd(path, n) == nil)
			return SQLITE_CANTOPEN;
	}
	len = strlen(path);
	if(len + (len != 0) + strlen(name) >= n)
		return SQLITE_CANTOPEN;
	if(len > 0)
		path[len++] = '/';
	strcpy(path+len, name);
	cleanname(path);
	return SQLITE_OK;
}

static int
fsrandom(sqlite3_vfs*, int n, char *buf)
{
	int fd, nr;

	fd = open("/dev/random", OREAD|OCEXEC);
	if(fd < 0)
		return 0;
	nr = readn(fd, buf, n);
	close(fd);
	return nr < 0 ? 0 : nr;
}

static int
fssleep(sqlite3_vfs*, int us)
{
	int ms;

	ms = us/1000 + (us%1000 != 0);
	sleep(ms);
	return us;
}

static int
fstime64(sqlite3_vfs*, sqlite3_int64 *t)
{
	*t = nsec()/1000000 + 210866760000000LL;
	return SQLITE_OK;
}

static int
fstime(sqlite3_vfs *v, double *t)
{
	sqlite3_int64 ms;

	fstime64(v, &ms);
	*t = ms/86400000.0;
	return SQLITE_OK;
}

static int
fserror(sqlite3_vfs*, int n, char *buf)
{
	rerrstr(buf, n);
	return 0;
}

static sqlite3_vfs vfs = {
	2, sizeof(File), Maxpath, nil, "plan9", nil,
	fsopen, fsdelete, fsaccess, fsfullpath,
	nil, nil, nil, nil,
	fsrandom, fssleep, fstime, fserror, fstime64,
};

int
sqlite3_os_init(void)
{
	setfcr(0);
	return sqlite3_vfs_register(&vfs, 1);
}

int
sqlite3_os_end(void)
{
	return sqlite3_vfs_unregister(&vfs);
}
