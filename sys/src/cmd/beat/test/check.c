#include <u.h>
#include <libc.h>

/* Check the public OPL stream and the output of games/opl3. */
void
main(int argc, char **argv)
{
	uchar cmd[5], buf[4096];
	int fd, bpm, ticks, n, i, loud;
	long bytes;

	if(argc != 4 && (argc != 5 || strcmp(argv[4], "silent") != 0))
		sysfatal("usage: check bpm opl pcm [silent]");
	bpm = atoi(argv[1]);
	if((fd = open(argv[2], OREAD)) < 0)
		sysfatal("open %s: %r", argv[2]);
	ticks = 0;
	while((n = readn(fd, cmd, sizeof cmd)) == sizeof cmd)
		ticks += cmd[3] | cmd[4]<<8;
	close(fd);
	if(n != 0 || ticks != 49716*240/bpm)
		sysfatal("bad OPL duration at %d bpm: %d samples", bpm, ticks);
	if((fd = open(argv[3], OREAD)) < 0)
		sysfatal("open %s: %r", argv[3]);
	bytes = loud = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		bytes += n;
		for(i = 0; i < n; i++)
			loud |= buf[i];
	}
	if(n < 0 || bytes%4 || labs(bytes/4 - 44100*240/bpm) > 16)
		sysfatal("bad PCM at %d bpm: %ld bytes", bpm, bytes);
	if((loud == 0) != (argc == 5))
		sysfatal("unexpected PCM level in %s", argv[3]);
	exits(nil);
}
