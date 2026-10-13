#include "src/defs.h"
#include "src/prototypes.h"
#include <bio.h>

static int failed;

void
emexit(int status)
{
	exits(status || failed ? "eigenmath" : nil);
}

static char*
readinput(int fd)
{
	char *buf, *p;
	long n, len, cap;

	buf = nil;
	len = cap = 0;
	for(;;){
		if(len == cap){
			/* Match the upstream script size limit. */
			if(cap == 1000001){
				werrstr("input exceeds 1000000 bytes");
				break;
			}
			cap += 8192;
			if(cap > 1000001)
				cap = 1000001;
			p = realloc(buf, cap + 1);
			if(p == nil)
				break;
			buf = p;
		}
		n = read(fd, buf + len, cap - len);
		if(n < 0)
			break;
		if(n == 0){
			if(memchr(buf, 0, len) != nil){
				werrstr("NUL in input");
				break;
			}
			buf[len] = 0;
			return buf;
		}
		len += n;
	}
	free(buf);
	return nil;
}

char*
readfile(char *name)
{
	char *buf, err[ERRMAX];
	int fd;

	fd = open(name, OREAD);
	if(fd < 0)
		return nil;
	buf = readinput(fd);
	rerrstr(err, sizeof err);
	close(fd);
	if(buf == nil)
		werrstr("%s", err);
	return buf;
}

static int
onconsole(int fd)
{
	char path[1024];

	return fd2path(fd, path, sizeof path) == 0 && strcmp(path, "/dev/cons") == 0;
}

void
main(int argc, char **argv)
{
	Biobuf in;
	char *buf;
	int i;

	/* Eigenmath's numeric functions use IEEE infinities and NaNs. */
	setfcr(0);
	setfsr(0);
	if(argc == 1 && onconsole(0) && onconsole(1)){
		Binit(&in, 0, OREAD);
		Blethal(&in, nil);
		for(;;){
			fprint(1, "? ");
			buf = Brdstr(&in, '\n', 0);
			if(buf == nil)
				break;
			if(memchr(buf, 0, Blinelen(&in)) != nil)
				sysfatal("NUL in input");
			run(buf);
			free(buf);
		}
		Bterm(&in);
	}else if(argc == 1){
		buf = readinput(0);
		if(buf == nil)
			sysfatal("stdin: %r");
		run(buf);
		free(buf);
	}else{
		for(i = 1; i < argc; i++){
			if(strcmp(argv[i], "-") == 0)
				buf = readinput(0);
			else
				buf = readfile(argv[i]);
			if(buf == nil)
				sysfatal("%s: %r", argv[i]);
			run(buf);
			free(buf);
			if(failed)
				break;
		}
	}
	emexit(0);
}

void
display(void)
{
	fmt();
}

void
printbuf(char *s, int color)
{
	int fd;
	long n, len;

	fd = 1;
	if(color == RED){
		failed = 1;
		fd = 2;
	}
	for(len = strlen(s); len > 0; len -= n, s += n){
		n = write(fd, s, len);
		if(n <= 0)
			sysfatal("write: %r");
	}
}

void
eval_draw(struct atom *p)
{
	USED(p);
	/* Like the upstream terminal front end, this has no drawing surface. */
	push_symbol(NIL);
}

void
eval_exit(struct atom *p)
{
	USED(p);
	emexit(0);
}
