#include <u.h>
#include <libc.h>
#include <bio.h>
#include <sqlite3.h>

static Biobuf output;
static int headers;
static char *separator = "\t";

static void
usage(void)
{
	fprint(2, "usage: sqlite3 [-hr] [-s separator] database [sql]\n");
	exits("usage");
}

static void
sqlerror(sqlite3 *db)
{
	int rc;

	rc = sqlite3_errcode(db);
	if(rc == SQLITE_IOERR || rc == SQLITE_CANTOPEN)
		sysfatal("%s: %r", sqlite3_errmsg(db));
	sysfatal("%s", sqlite3_errmsg(db));
}

static void
run(sqlite3 *db, char *sql)
{
	sqlite3_stmt *stmt;
	const char *tail, *text;
	int rc, i, n, first;

	while(*sql != 0){
		rc = sqlite3_prepare_v2(db, sql, -1, &stmt, &tail);
		if(rc != SQLITE_OK)
			sqlerror(db);
		sql = (char*)tail;
		if(stmt == nil)
			continue;
		first = 1;
		n = sqlite3_column_count(stmt);
		while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
			if(first && headers){
				for(i = 0; i < n; i++){
					text = sqlite3_column_name(stmt, i);
					if(text == nil)
						sqlerror(db);
					Bprint(&output, "%s%s", i == 0 ? "" : separator, text);
				}
				Bputc(&output, '\n');
			}
			first = 0;
			for(i = 0; i < n; i++){
				if(i != 0)
					Bprint(&output, "%s", separator);
				text = (char*)sqlite3_column_text(stmt, i);
				if(text == nil && sqlite3_column_type(stmt, i) != SQLITE_NULL)
					sqlerror(db);
				if(text != nil)
					Bwrite(&output, text, sqlite3_column_bytes(stmt, i));
			}
			Bputc(&output, '\n');
		}
		if(rc != SQLITE_DONE)
			sqlerror(db);
		if(sqlite3_finalize(stmt) != SQLITE_OK)
			sqlerror(db);
	}
	if(Bflush(&output) < 0)
		sysfatal("write: %r");
}

void
main(int argc, char **argv)
{
	sqlite3 *db;
	Biobuf input;
	char *sql;
	int flags, c, cap, len, limit, rc;

	flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_URI;
	ARGBEGIN{
	case 'h': headers = 1; break;
	case 'r': flags = SQLITE_OPEN_READONLY|SQLITE_OPEN_URI; break;
	case 's': separator = EARGF(usage()); break;
	default: usage();
	}ARGEND
	if(argc < 1 || argc > 2)
		usage();
	rc = sqlite3_open_v2(argv[0], &db, flags, nil);
	if(rc != SQLITE_OK)
		sqlerror(db);
	Binit(&output, 1, OWRITE);
	Blethal(&output, nil);
	if(argc == 2)
		run(db, argv[1]);
	else{
		Binit(&input, 0, OREAD);
		Blethal(&input, nil);
		cap = 4096;
		sql = malloc(cap);
		if(sql == nil)
			sysfatal("malloc: %r");
		limit = sqlite3_limit(db, SQLITE_LIMIT_SQL_LENGTH, -1);
		len = 0;
		while((c = Bgetc(&input)) >= 0){
			if(c == 0)
				sysfatal("NUL in SQL input");
			if(len == limit)
				sysfatal("SQL input too long");
			if(len+1 == cap){
				cap = cap > limit/2 ? limit+1 : cap*2;
				sql = realloc(sql, cap);
				if(sql == nil)
					sysfatal("malloc: %r");
			}
			sql[len++] = c;
			sql[len] = 0;
			if(c == '\n' && sqlite3_complete(sql)){
				run(db, sql);
				len = 0;
				sql[0] = 0;
			}
		}
		if(len > 0)
			run(db, sql);
		free(sql);
		Bterm(&input);
	}
	if(sqlite3_close(db) != SQLITE_OK)
		sqlerror(db);
	if(Bterm(&output) < 0)
		sysfatal("write: %r");
	exits(nil);
}
