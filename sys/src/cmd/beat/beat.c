#include <u.h>
#include <libc.h>
#include <bio.h>
#include "beat.h"

char *voice[] = {"kick", "snare", "hat", "clap"};

static void
getpattern(int fd, char *name, Pattern *p)
{
	Biobuf b;
	char *line, *f[3], *s;
	int i, j, n, nr, seen;

	if(Binit(&b, fd, OREAD) < 0)
		sysfatal("Binit: %r");
	Blethal(&b, nil);
	seen = 0;
	for(nr = 1; (line = Brdstr(&b, '\n', 1)) != nil; nr++){
		if(strlen(line) != Blinelen(&b))
			goto bad;
		if((s = strchr(line, '#')) != nil)
			*s = 0;
		n = getfields(line, f, nelem(f), 1, " \t\r");
		if(n == 0){
			free(line);
			continue;
		}
		if(n != 2)
			goto bad;
		for(i = 0; i < Nvoice; i++)
			if(strcmp(f[0], voice[i]) == 0)
				break;
		if(seen & 1<<i)
			goto bad;
		seen |= 1<<i;
		if(i == Nvoice){
			if(strcmp(f[0], "bpm") != 0)
				goto bad;
			n = strtol(f[1], &s, 10);
			if(*s || n < Minbpm || n > Maxbpm)
				goto bad;
			p->bpm = n;
		}else{
			if(strlen(f[1]) != Nstep)
				goto bad;
			p->row[i] = 0;
			for(j = 0; j < Nstep; j++)
				if(f[1][j] == 'x')
					p->row[i] |= 1<<j;
				else if(f[1][j] != '.')
					goto bad;
		}
		free(line);
	}
	Bterm(&b);
	if(seen != (1<<(Nvoice+1))-1)
		sysfatal("%s: expected bpm, kick, snare, hat and clap", name);
	return;
bad:
	sysfatal("%s:%d: invalid pattern line", name, nr);
}

int
putpattern(int fd, Pattern *p)
{
	Dir *d, trunc;
	vlong old;
	char buf[256], row[Nstep+1], *s;
	int i, j, n;

	if((d = dirfstat(fd)) == nil)
		return -1;
	if(d->mode & DMAPPEND){
		free(d);
		werrstr("append-only pattern");
		return -1;
	}
	old = d->length;
	free(d);
	s = seprint(buf, buf+sizeof buf, "bpm %d\n", p->bpm);
	row[Nstep] = 0;
	for(i = 0; i < Nvoice; i++){
		for(j = 0; j < Nstep; j++)
			row[j] = p->row[i] & 1<<j ? 'x' : '.';
		s = seprint(s, buf+sizeof buf, "%s %s\n", voice[i], row);
	}
	n = s-buf;
	if(write(fd, buf, n) != n)
		return -1;
	if(old > n){
		nulldir(&trunc);
		trunc.length = n;
		return dirfwstat(fd, &trunc);
	}
	return 0;
}

static void
usage(void)
{
	fprint(2, "usage: beat [pattern] or beat -e [-p] pattern\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Pattern p = {120, {0x1111, 0x1010, 0x5555, 0x4000}};
	int fd, i, rem, editor, piped;
	char *name;

	editor = piped = 0;
	ARGBEGIN{
	case 'e': editor = 1; break;
	case 'p': piped = 1; break;
	default: usage();
	}ARGEND
	if(argc > 1 || (editor && argc != 1) || (piped && !editor))
		usage();
	name = "stdin";
	fd = 0;
	if(argc){
		name = argv[0];
		fd = open(name, OREAD);
	}
	if(fd < 0 && editor){
		fd = create(name, OWRITE|OEXCL, 0666);
		if(fd < 0 || putpattern(fd, &p) < 0)
			sysfatal("create %s: %r", name);
	}else{
		if(fd < 0)
			sysfatal("open %s: %r", name);
		getpattern(fd, name, &p);
	}
	close(fd);
	if(editor)
		edit(name, &p, piped);
	else{
		if(kit(1) < 0)
			sysfatal("write: %r");
		rem = 0;
		for(i = 0; i < Nstep; i++)
			if(step(1, &p, i, &rem) < 0)
				sysfatal("write: %r");
	}
	exits(nil);
}
