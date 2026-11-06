#include <u.h>
#include <libc.h>
#include <sqlite3.h>

static sqlite3_vfs *native;
static sqlite3_io_methods faultmethods;
static const sqlite3_io_methods *realmethods;
static int fault, cut, faultfd;

static void
check(int ok, char *what)
{
	if(!ok)
		sysfatal("FAIL: %s: %r", what);
}

static sqlite3*
dbopen(char *name, char *vfs)
{
	sqlite3 *db;

	if(sqlite3_open_v2(name, &db, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, vfs) != SQLITE_OK)
		sysfatal("open %s: %s: %r", name, sqlite3_errmsg(db));
	return db;
}

static void
sql(sqlite3 *db, char *text)
{
	char *error;

	if(sqlite3_exec(db, text, nil, nil, &error) != SQLITE_OK)
		sysfatal("%s: %s: %r", text, error);
}

static void
expect(sqlite3 *db, char *text, char *want)
{
	sqlite3_stmt *s;
	const uchar *value;

	check(sqlite3_prepare_v2(db, text, -1, &s, nil) == SQLITE_OK, text);
	check(sqlite3_step(s) == SQLITE_ROW, text);
	value = sqlite3_column_text(s, 0);
	if(value == nil || strcmp((char*)value, want) != 0)
		sysfatal("%s: got %s, expected %s", text, (char*)value, want);
	check(sqlite3_step(s) == SQLITE_DONE, "single result row");
	check(sqlite3_finalize(s) == SQLITE_OK, "finalize");
}

static void
memory(void)
{
	sqlite3 *db;
	sqlite3_stmt *s;
	uchar blob[] = {0, 1, 0xff, 0, 127};
	vlong largest = 0x7fffffffffffffffLL;
	char now[96];

	db = dbopen(":memory:", nil);
	expect(db, "select typeof(6*7)||':'||(6*7)", "integer:42");
	expect(db, "select typeof(-6*7)||':'||(-6*7)", "integer:-42");
	expect(db, "select -9223372036854775808 * 1", "-9223372036854775808");
	expect(db, "select 9223372036854775807 * 1", "9223372036854775807");
	expect(db, "select typeof(9223372036854775807+1)", "real");
	expect(db, "select typeof(-9223372036854775808-1)", "real");
	expect(db, "select typeof(-9223372036854775808*-1)", "real");
	expect(db, "select 3037000499 * 3037000499", "9223372030926249001");
	expect(db, "select (17/3)||':'||(-17%3)", "5:-2");
	expect(db, "select typeof(1/0)", "null");
	expect(db, "select hex(cast('caffè ☃' as blob))", "63616666C3A820E29883");
	expect(db, "select json_extract('{\"a\":[1,2,3]}','$.a[2]')", "3");
	expect(db, "select datetime(0,'unixepoch')", "1970-01-01 00:00:00");
	expect(db, "select julianday('2000-01-01 12:00:00')", "2451545.0");
	snprint(now, sizeof(now), "select abs(unixepoch('now')-%lud)<5", time(nil));
	expect(db, now, "1");
	expect(db, "with recursive t(x) as (values(1) union all select x+1 from t where x<100) select sum(x) from t", "5050");
	sql(db, "create table t(id integer primary key, s text, b blob); begin;");
	check(sqlite3_prepare_v2(db, "insert into t values(?1,?2,?3)", -1, &s, nil) == SQLITE_OK, "prepare bindings");
	check(sqlite3_bind_int64(s, 1, largest) == SQLITE_OK, "bind int64");
	check(sqlite3_bind_text(s, 2, "caffè ☃", -1, SQLITE_STATIC) == SQLITE_OK, "bind UTF-8");
	check(sqlite3_bind_blob(s, 3, blob, sizeof(blob), SQLITE_STATIC) == SQLITE_OK, "bind blob");
	check(sqlite3_step(s) == SQLITE_DONE, "insert bindings");
	check(sqlite3_finalize(s) == SQLITE_OK, "finalize bindings");
	sql(db, "savepoint a; delete from t; rollback to a; release a; commit;");
	expect(db, "select id from t", "9223372036854775807");
	expect(db, "select s from t", "caffè ☃");
	expect(db, "select hex(b) from t", "0001FF007F");
	check(sqlite3_exec(db, "insert into t(id) values(9223372036854775807)", nil, nil, nil) == SQLITE_CONSTRAINT, "constraint error");
	expect(db, "pragma integrity_check", "ok");
	check(sqlite3_close(db) == SQLITE_OK, "close memory");
	print("PASS: numeric boundaries, SQL, JSON, UTC dates, bindings and rollback\n");
}

static void
files(char *name)
{
	sqlite3 *db, *other;
	Dir *d;
	int rc;

	db = dbopen(name, nil);
	sql(db, "create table t(id integer primary key, s text); create index ts on t(s);"
		"begin; with recursive x(n) as (values(1) union all select n+1 from x where n<500)"
		"insert into t select n, printf('%04d',n) from x; commit;");
	expect(db, "select sum(id) from t", "125250");
	expect(db, "select count(*) from t where s between '0100' and '0199'", "100");
	expect(db, "pragma integrity_check", "ok");
	d = dirstat(name);
	check(d != nil && (d->mode & DMEXCL), "exclusive-use database");
	free(d);
	rc = sqlite3_open(name, &other);
	check(rc != SQLITE_OK, "reject second connection");
	sqlite3_close(other);
	sql(db, "pragma journal_mode=persist; begin; delete from t; rollback;");
	expect(db, "select count(*) from t", "500");
	sql(db, "pragma journal_mode=truncate; update t set s='changed' where id=1;");
	sql(db, "pragma journal_mode=delete; vacuum;");
	expect(db, "pragma integrity_check", "ok");
	check(sqlite3_close(db) == SQLITE_OK, "close persistent database");
	db = dbopen(name, nil);
	expect(db, "select s from t where id=1", "changed");
	expect(db, "select count(*) from t", "500");
	check(sqlite3_close(db) == SQLITE_OK, "reopen persistent database");
	check(sqlite3_open_v2(name, &db, SQLITE_OPEN_READONLY, nil) == SQLITE_OK, "read-only database");
	expect(db, "select count(*) from t", "500");
	check(sqlite3_exec(db, "delete from t", nil, nil, nil) == SQLITE_READONLY, "read-only write denied");
	check(sqlite3_open_v2(name, &other, SQLITE_OPEN_READONLY, nil) == SQLITE_CANTOPEN, "exclusive reader");
	sqlite3_close(other);
	check(sqlite3_close(db) == SQLITE_OK, "close read-only database");
	print("PASS: file persistence, indexes, journal modes, vacuum and exclusivity\n");
}

static void
renamed(char *dir)
{
	sqlite3 *db;
	Dir d;
	char *old, *new;

	old = smprint("%s/move.db", dir);
	new = smprint("%s/moved.db", dir);
	check(old != nil && new != nil, "rename paths");
	db = dbopen(old, nil);
	sql(db, "create table t(n); insert into t values(1);");
	nulldir(&d);
	d.name = "moved.db";
	check(dirwstat(old, &d) >= 0, "rename open database");
	check(sqlite3_exec(db, "insert into t values(2)", nil, nil, nil) == SQLITE_READONLY,
		"reject write to renamed database");
	check(sqlite3_extended_errcode(db) == SQLITE_READONLY_DBMOVED, "renamed database error");
	check(sqlite3_close(db) == SQLITE_OK, "close renamed database");
	db = dbopen(new, nil);
	expect(db, "select count(*) from t", "1");
	expect(db, "pragma integrity_check", "ok");
	check(sqlite3_close(db) == SQLITE_OK, "reopen renamed database");
	check(remove(new) == 0, "remove renamed database");
	free(old);
	free(new);
	print("PASS: reject writes after database rename\n");
}

static void
io(char *dir)
{
	sqlite3_vfs *v;
	sqlite3_file *f;
	sqlite3 *db;
	sqlite3_int64 size;
	char *name, *uri;
	uchar buf[16], want[16], *large, *copy;
	int flags, fd, i, n;

	v = sqlite3_vfs_find(nil);
	f = mallocz(v->szOsFile, 1);
	check(f != nil, "I/O test allocation");
	flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_DELETEONCLOSE|SQLITE_OPEN_TEMP_DB;
	check(v->xOpen(v, nil, f, flags, nil) == SQLITE_OK, "temporary file");
	memset(buf, 0xff, sizeof(buf));
	memset(want, 0, sizeof(want));
	check(f->pMethods->xRead(f, buf, sizeof(buf), 0) == SQLITE_IOERR_SHORT_READ, "short read status");
	check(memcmp(buf, want, sizeof(buf)) == 0, "zero-fill short read");
	check(f->pMethods->xWrite(f, "abcd", 4, 4) == SQLITE_OK, "write at offset");
	check(f->pMethods->xFileSize(f, &size) == SQLITE_OK && size == 8, "extended length");
	check(f->pMethods->xRead(f, buf, sizeof(buf), 0) == SQLITE_IOERR_SHORT_READ, "partial read status");
	memmove(want+4, "abcd", 4);
	check(memcmp(buf, want, sizeof(buf)) == 0, "partial read and zero tail");
	check(f->pMethods->xTruncate(f, 6) == SQLITE_OK, "truncate");
	check(f->pMethods->xFileSize(f, &size) == SQLITE_OK && size == 6, "truncated length");
	n = 128*1024;
	large = malloc(n);
	copy = malloc(n);
	check(large != nil && copy != nil, "large I/O allocation");
	for(i = 0; i < n; i++)
		large[i] = i ^ (i >> 8);
	check(f->pMethods->xWrite(f, large, n, 17) == SQLITE_OK, "write across 9P messages");
	check(f->pMethods->xRead(f, copy, n, 17) == SQLITE_OK, "read across 9P messages");
	check(memcmp(large, copy, n) == 0, "large I/O contents");
	free(large);
	free(copy);
	check(f->pMethods->xClose(f) == SQLITE_OK, "temporary close");
	free(f);

	name = smprint("%s/plain.db", dir);
	check(name != nil, "plain path");
	fd = create(name, ORDWR|OEXCL, 0600);
	check(fd >= 0, "non-exclusive file");
	close(fd);
	check(sqlite3_open(name, &db) == SQLITE_CANTOPEN, "reject missing DMEXCL");
	sqlite3_close(db);
	check(sqlite3_open(dir, &db) == SQLITE_CANTOPEN, "reject directory");
	sqlite3_close(db);
	uri = smprint("file:%s?immutable=1", name);
	check(uri != nil, "immutable URI");
	check(sqlite3_open_v2(uri, &db, SQLITE_OPEN_READONLY|SQLITE_OPEN_URI, nil) == SQLITE_OK, "immutable read");
	expect(db, "select count(*) from sqlite_schema", "0");
	check(sqlite3_exec(db, "create table forbidden(x)", nil, nil, nil) == SQLITE_READONLY, "immutable write denied");
	sqlite3_close(db);
	remove(name);
	free(name);
	free(uri);
	print("PASS: short and large I/O, offsets, truncate, temporary files and immutable access\n");
}

static void
attached(char *name, char *dir)
{
	sqlite3 *db;
	char *other, *command;

	other = smprint("%s/attached.db", dir);
	check(other != nil, "attached path");
	db = dbopen(name, nil);
	command = sqlite3_mprintf("attach %Q as other; create table other.t(x);", other);
	check(command != nil, "attach SQL");
	sql(db, command);
	sqlite3_free(command);
	sql(db, "begin; delete from main.t; insert into other.t values(7); rollback;");
	expect(db, "select count(*) from main.t", "500");
	expect(db, "select count(*) from other.t", "0");
	sql(db, "begin; update main.t set s='changed' where id=1; insert into other.t values(42); commit;");
	check(sqlite3_close(db) == SQLITE_OK, "close attached databases");
	db = dbopen(other, nil);
	expect(db, "select x from t", "42");
	expect(db, "pragma integrity_check", "ok");
	check(sqlite3_close(db) == SQLITE_OK, "close detached database");
	check(remove(other) == 0, "remove attached database");
	free(other);
	print("PASS: transactions across attached databases\n");
}

static void
childwait(int pid)
{
	Waitmsg *w;

	w = wait();
	check(w != nil && w->pid == pid, "wait child");
	if(w->msg[0] != 0)
		sysfatal("child: %s", w->msg);
	free(w);
}

static void
processes(char *name)
{
	sqlite3 *db, *other;
	int p[2], pid, rc, i;
	char c;

	check(pipe(p) == 0, "pipe");
	pid = fork();
	check(pid >= 0, "fork");
	if(pid == 0){
		close(p[0]);
		db = dbopen(name, nil);
		sql(db, "pragma cache_size=2; begin immediate; update t set s=printf('%0400d', id);");
		check(sqlite3_db_cacheflush(db) == SQLITE_OK, "spill uncommitted pages");
		check(write(p[1], "r", 1) == 1, "child ready");
		check(read(p[1], &c, 1) == 1, "child release");
		exits(nil);
	}
	close(p[1]);
	check(read(p[0], &c, 1) == 1, "wait child open");
	rc = sqlite3_open(name, &other);
	check(rc != SQLITE_OK, "cross-process exclusivity");
	sqlite3_close(other);
	check(write(p[0], "x", 1) == 1, "release child");
	close(p[0]);
	childwait(pid);
	for(i = 0; i < 100; i++){
		rc = sqlite3_open(name, &db);
		if(rc == SQLITE_OK)
			break;
		sqlite3_close(db);
		sleep(10);
	}
	check(rc == SQLITE_OK, "release on process death");
	expect(db, "select s from t where id=1", "changed");
	expect(db, "select count(*) from t", "500");
	expect(db, "pragma integrity_check", "ok");
	check(sqlite3_close(db) == SQLITE_OK, "close recovered database");
	print("PASS: cross-process exclusion, crash release and hot-journal recovery\n");
}

static int
faultwrite(sqlite3_file *f, const void *b, int n, sqlite3_int64 off)
{
	if(fault > 0 && --fault == 0){
		if(cut){
			check(write(faultfd, "I", 1) == 1, "report crash");
			exits(nil);
		}
		werrstr("injected write error");
		return SQLITE_IOERR_WRITE;
	}
	return realmethods->xWrite(f, b, n, off);
}

static int
faultsync(sqlite3_file *f, int flags)
{
	if(fault > 0 && --fault == 0){
		if(cut){
			check(write(faultfd, "I", 1) == 1, "report crash");
			exits(nil);
		}
		werrstr("injected sync error");
		return SQLITE_IOERR_FSYNC;
	}
	return realmethods->xSync(f, flags);
}

static int
faultopen(sqlite3_vfs*, const char *name, sqlite3_file *f, int flags, int *out)
{
	int rc;

	rc = native->xOpen(native, name, f, flags, out);
	if(rc == SQLITE_OK){
		realmethods = f->pMethods;
		faultmethods = *realmethods;
		faultmethods.xWrite = faultwrite;
		faultmethods.xSync = faultsync;
		f->pMethods = &faultmethods;
	}
	return rc;
}

static void
failures(char *dir)
{
	sqlite3 *db;
	sqlite3_vfs vfs;
	char *name, report;
	int point, pid, done, rc, mode, p[2];

	native = sqlite3_vfs_find(nil);
	vfs = *native;
	vfs.zName = "fault";
	vfs.xOpen = faultopen;
	check(sqlite3_vfs_register(&vfs, 0) == SQLITE_OK, "fault VFS");
	for(mode = 0; mode < 2; mode++){
		for(point = 1; point < 100; point++){
			name = smprint("%s/fault-%d-%d.db", dir, mode, point);
			check(name != nil, "fault name");
			db = dbopen(name, nil);
			sql(db, "create table t(n integer primary key, v integer, p blob);"
				"with recursive x(n) as (values(1) union all select n+1 from x where n<8)"
				"insert into t select n, 1, zeroblob(2000) from x;");
			check(sqlite3_close(db) == SQLITE_OK, "close fault fixture");
			check(pipe(p) == 0, "fault pipe");
			pid = fork();
			check(pid >= 0, "fork fault");
			if(pid == 0){
				close(p[0]);
				faultfd = p[1];
				db = dbopen(name, "fault");
				cut = mode;
				fault = point;
				rc = sqlite3_exec(db, "begin; update t set v=2; commit;", nil, nil, nil);
				done = fault != 0;
				fault = 0;
				if(done)
					check(rc == SQLITE_OK, "uninjected transaction");
				else
					check((rc & 255) == SQLITE_IOERR, "reported I/O failure");
				sqlite3_close(db);
				check(write(p[1], done ? "D" : "I", 1) == 1, "report fault");
				exits(nil);
			}
			close(p[1]);
			check(read(p[0], &report, 1) == 1, "read fault result");
			close(p[0]);
			childwait(pid);
			done = report == 'D';
			db = dbopen(name, nil);
			expect(db, "select count(*)=8 and min(v)=max(v) and min(v) in (1,2) and sum(length(p))=16000 from t", "1");
			if(done)
				expect(db, "select min(v) from t", "2");
			expect(db, "pragma integrity_check", "ok");
			sqlite3_close(db);
			remove(name);
			free(name);
			if(done)
				break;
		}
		check(point < 100, "all fault boundaries reached");
		print("PASS: %d write/sync boundaries with %s\n", point-1,
			mode == 0 ? "I/O errors" : "process death");
	}
	sqlite3_vfs_unregister(&vfs);
}

void
main(int argc, char **argv)
{
	char *name;

	if(argc != 2)
		sysfatal("usage: %s directory-on-exclusive-use-filesystem", argv[0]);
	memory();
	io(argv[1]);
	name = smprint("%s/test.db", argv[1]);
	check(name != nil, "database path");
	files(name);
	renamed(argv[1]);
	attached(name, argv[1]);
	processes(name);
	failures(argv[1]);
	free(name);
	exits(nil);
}
