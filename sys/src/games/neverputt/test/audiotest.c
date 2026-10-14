#include "port.h"
#include "audio.h"
#include "config.h"
#include "fs.h"
#undef strdup

int CONFIG_SOUND_VOLUME, CONFIG_MUSIC_VOLUME;

/* Supply the mixer's configuration and asset interfaces without the game. */
char*
dupe_string(const char *s)
{
	return strdup(s);
}

int
config_get_d(int option)
{
	USED(option);
	return 10;
}

void*
fs_load(const char *name, int *bytes)
{
	USED(name);
	*bytes = 44100*4;
	return mallocz(*bytes,1);
}

char*
fs_realpath(const char *name)
{
	USED(name);
	return strdup("/dev/zero");
}

void
main(void)
{
	Waitmsg *w;
	char proc[64];
	int fd, pid, i, j;

	fd = open("/dev/audio",OWRITE);
	if(fd < 0) sysfatal("audio device unavailable: %r");
	close(fd);
	for(i=0; i<3; i++){
		audio_init();
		audio_play("sound.ogg",1);
		audio_music_fade_to(0,"music.ogg");
		pid = rfork(RFPROC|RFFDG);
		if(pid < 0) sysfatal("test child: %r");
		if(pid == 0) exits("unrelated child");
		snprint(proc,sizeof proc,"/proc/%d/status",pid);
		for(j=0; j<500 && access(proc,AEXIST)==0; j++) sleep(10);
		if(j == 500) sysfatal("test child did not exit");
		audio_check();
		audio_free();
		w = wait();
		if(w == nil) sysfatal("audio cleanup consumed another child's wait message");
		if(w->pid != pid || strstr(w->msg,"unrelated child") == nil)
			sysfatal("audio cleanup: expected child %d, got %d (%s)",pid,w->pid,w->msg);
		free(w);
		audio_free();
	}
	audio_init();
	audio_music_fade_to(0,"music.ogg");
	audio_music_fade_to(0,nil);
	sleep(100);
	audio_check();
	audio_free();
	print("PASS: audio shutdown preserves child status, restarts, and accepts absent music\n");
	exits(nil);
}
