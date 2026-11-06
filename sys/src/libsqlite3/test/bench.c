#ifdef PLAN9
#include <u.h>
#include <libc.h>
#define fatal(s) sysfatal("%s", s)
#else
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define nil NULL
#define AEXIST F_OK
#define print printf

static void
fatal(const char *s)
{
	fprintf(stderr, "bench: %s\n", s);
	exit(1);
}
#endif
#include <sqlite3.h>

static sqlite3 *db;
static sqlite3_int64 startwall;
static double startcpu;

static sqlite3_int64
wall(void)
{
#ifdef PLAN9
	return nsec();
#else
	struct timespec t;

	if(clock_gettime(CLOCK_MONOTONIC, &t) < 0)
		fatal("monotonic clock");
	return (sqlite3_int64)t.tv_sec*1000000000+t.tv_nsec;
#endif
}

static double
cpu(void)
{
#ifdef PLAN9
	return cputime();
#else
	struct timespec t;

	if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t) < 0)
		fatal("CPU clock");
	return t.tv_sec+t.tv_nsec/1e9;
#endif
}

static void
check(int ok)
{
	if(!ok)
		fatal(sqlite3_errmsg(db));
}

static void
sql(const char *s)
{
	check(sqlite3_exec(db, s, nil, nil, nil) == SQLITE_OK);
}

static sqlite3_int64
scalar(const char *q)
{
	sqlite3_stmt *s;
	sqlite3_int64 n;

	check(sqlite3_prepare_v2(db, q, -1, &s, nil) == SQLITE_OK);
	check(sqlite3_step(s) == SQLITE_ROW);
	n = sqlite3_column_int64(s, 0);
	check(sqlite3_step(s) == SQLITE_DONE);
	check(sqlite3_finalize(s) == SQLITE_OK);
	return n;
}

static void
begin(void)
{
	startcpu = cpu();
	startwall = wall();
}

static void
report(const char *name, int n, sqlite3_int64 sum)
{
	double elapsed, used;

	elapsed = (wall()-startwall)/1e6;
	used = cpu()-startcpu;
	if(print("%s\t%d\t%.3f\t%.3f\t%lld\n", name, n, elapsed, used*1000, sum) < 0)
		fatal("write output");
}

static void
lookups(int n, int transaction)
{
	sqlite3_stmt *s;
	sqlite3_int64 sum, expected;
	int i, k, m;

	m = 20000;
	check(sqlite3_prepare_v2(db, "select n from t where k=?1", -1, &s, nil) == SQLITE_OK);
	expected = 0;
	for(i = 0; i < m; i++){
		k = (i*7919)%n+1;
		expected += (k*37)%10000;
	}
	begin();
	if(transaction)
		sql("begin");
	sum = 0;
	for(i = 0; i < m; i++){
		k = (i*7919)%n+1;
		check(sqlite3_bind_int(s, 1, k) == SQLITE_OK);
		check(sqlite3_step(s) == SQLITE_ROW);
		sum += sqlite3_column_int64(s, 0);
		check(sqlite3_step(s) == SQLITE_DONE);
		check(sqlite3_reset(s) == SQLITE_OK);
	}
	if(transaction)
		sql("commit");
	report(transaction ? "lookup_transaction" : "lookup_autocommit", m, sum);
	if(sum != expected)
		fatal("lookup checksum");
	check(sqlite3_finalize(s) == SQLITE_OK);
}

static void
run(char *name, int exclusive)
{
	sqlite3_stmt *s;
	sqlite3_int64 sum;
	const unsigned char *text;
	int i, n, commits;

	if(strcmp(name, ":memory:") != 0 && access(name, AEXIST) == 0)
		fatal("database already exists");
	n = 100000;
	commits = 5;
	check(sqlite3_open(name, &db) == SQLITE_OK);
	if(print("version\t%s\t%s\t%s\n", sqlite3_libversion(), name,
	   exclusive ? "exclusive" : "normal") < 0)
		fatal("write output");
	if(print("test\tcount\twall_ms\tcpu_ms\tchecksum\n") < 0)
		fatal("write output");
	sql("pragma page_size=4096; pragma cache_size=-16384; pragma temp_store=memory;"
		"pragma journal_mode=delete; pragma synchronous=full;");
	sql(exclusive ? "pragma locking_mode=exclusive" : "pragma locking_mode=normal");
	begin();
	sum = scalar("with recursive t(x) as (values(1) union all select x+1 from t where x<1000000) select sum(x) from t");
	report("recursive_sum", 1000000, sum);
	if(sum != 500000500000LL)
		fatal("recursive checksum");
	sql("create table t(k integer primary key, v text, n integer);");
	check(sqlite3_prepare_v2(db, "insert into t values(?1,?2,?3)", -1, &s, nil) == SQLITE_OK);
	begin();
	sql("begin");
	for(i = 1; i <= n; i++){
		check(sqlite3_bind_int(s, 1, i) == SQLITE_OK);
		check(sqlite3_bind_text(s, 2, "abcdefghijklmnop", -1, SQLITE_STATIC) == SQLITE_OK);
		check(sqlite3_bind_int(s, 3, (i*37)%10000) == SQLITE_OK);
		check(sqlite3_step(s) == SQLITE_DONE);
		check(sqlite3_reset(s) == SQLITE_OK);
	}
	sql("commit");
	report("insert_batch", n, sqlite3_total_changes(db));
	check(sqlite3_finalize(s) == SQLITE_OK);
	if(scalar("select count(*) from t") != n)
		fatal("row count");
	begin();
	sql("create index tn on t(n)");
	report("create_index", n, n);
	lookups(n, 1);
	lookups(n, 0);
	begin();
	sum = scalar("select sum(n) from (select n from t order by v desc, n)");
	report("sort_scan", n, sum);
	if(sum != 499950000LL)
		fatal("sort checksum");
	begin();
	sql("begin; update t set n=n+1; commit");
	report("update_batch", n, sqlite3_changes(db));
	if(scalar("select sum(n) from t") != 500050000LL)
		fatal("update checksum");
	sql("create table c(k integer primary key, v integer); insert into c values(1,0)");
	begin();
	for(i = 0; i < commits; i++)
		sql("begin; update c set v=v+1 where k=1; commit");
	report("commit_full", commits, commits);
	sql("pragma synchronous=off");
	begin();
	for(i = 0; i < commits; i++)
		sql("begin; update c set v=v+1 where k=1; commit");
	report("commit_off", commits, commits);
	if(scalar("select v from c") != 2*commits)
		fatal("commit count");
	check(sqlite3_prepare_v2(db, "pragma integrity_check", -1, &s, nil) == SQLITE_OK);
	check(sqlite3_step(s) == SQLITE_ROW);
	text = sqlite3_column_text(s, 0);
	if(text == nil || strcmp((const char*)text, "ok") != 0)
		fatal("integrity check");
	check(sqlite3_step(s) == SQLITE_DONE);
	check(sqlite3_finalize(s) == SQLITE_OK);
	check(sqlite3_close(db) == SQLITE_OK);
	if(print("ok\n") < 0)
		fatal("write output");
}

static void
usage(void)
{
	fatal("usage: bench [-x] database");
}

#ifdef PLAN9
void
main(int argc, char **argv)
{
	int exclusive;

	exclusive = 0;
	ARGBEGIN{
	case 'x': exclusive = 1; break;
	default: usage();
	}ARGEND
	if(argc != 1)
		usage();
	run(argv[0], exclusive);
	exits(nil);
}
#else
int
main(int argc, char **argv)
{
	int c, exclusive;

	exclusive = 0;
	while((c = getopt(argc, argv, "x")) != -1){
		switch(c){
		case 'x': exclusive = 1; break;
		default: usage();
		}
	}
	if(argc-optind != 1)
		usage();
	run(argv[optind], exclusive);
	if(fflush(stdout) == EOF)
		fatal("write output");
	return 0;
}
#endif
