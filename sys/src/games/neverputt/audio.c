/* PCM mixing over /dev/audio; audio/oggdec prepares the original assets. */
#include "port.h"
#include "audio.h"
#include "config.h"
#include "fs.h"

enum { Nframe=512, Nvoice=16, Rate=44100 };
typedef struct Sound Sound;
typedef struct Voice Voice;
typedef struct Music Music;
struct Sound {
	Sound *next;
	char *name;
	uchar *data;
	int bytes;
};
struct Voice {
	Sound *sound;
	int offset;
	float gain;
};
struct Music {
	char *name;
	int fd;
	float gain, step;
};
static QLock audiolock;
static Sound *sounds;
static Voice voices[Nvoice];
static Music *music, *queued;
static int audiofd=-1, audioowner, audioquit;
static char audioerror[ERRMAX];
static float effectvol=1, musicvol=1;

static char*
pcmname(const char *name)
{
	char *s, *ext;

	s = strdup(name);
	if(s == nil) sysfatal("audio path: %r");
	ext = strrchr(s,'.');
	if(ext == nil || strcmp(ext,".ogg") != 0)
		sysfatal("unsupported audio asset %s",name);
	strcpy(ext,".pcm");
	return s;
}

static void
freemusic(Music *m)
{
	if(m == nil) return;
	close(m->fd);
	free(m->name);
	free(m);
}

static Music*
newmusic(const char *name, float seconds)
{
	Music *m;
	char *pcm, *path;
	int fd;

	/* Standalone holes may have no background music. */
	if(name == nil || name[0] == 0)
		return nil;
	pcm = pcmname(name);
	path = fs_realpath(pcm);
	free(pcm);
	if(path == nil)
		sysfatal("missing audio %s; run mk data",name);
	fd = open(path,OREAD);
	free(path);
	if(fd < 0) sysfatal("open audio: %r");
	m = mallocz(sizeof(*m),1);
	if(m == nil) sysfatal("audio allocation: %r");
	m->name = strdup(name);
	m->fd = fd;
	m->gain = seconds > 0 ? 0 : 1;
	m->step = seconds > 0 ? 1/(Rate*seconds) : 0;
	return m;
}

static int
sample(uchar *p)
{
	return (short)(p[0]|p[1]<<8);
}

static void
mixaudio(void)
{
	uchar bgm[Nframe*4], out[Nframe*4];
	float mix[Nframe*2], gain;
	Voice *v;
	int n, k, i, j, bytes, value;

	for(;;){
		memset(mix,0,sizeof(mix));
		qlock(&audiolock);
		if(audioquit){
			qunlock(&audiolock);
			return;
		}
		if(music == nil && queued != nil){
			music = queued;
			queued = nil;
		}
		if(music != nil){
			bytes = readn(music->fd,bgm,sizeof(bgm));
			if(bytes < 0) goto readerror;
			if(bytes < sizeof(bgm)){
				seek(music->fd,0,0);
				n = readn(music->fd,bgm+bytes,sizeof(bgm)-bytes);
				if(n < 0) goto readerror;
				bytes += n;
			}
			for(i=0; i<bytes/4; i++){
				music->gain += music->step;
				if(music->gain > 1) music->gain = 1;
				if(music->gain < 0) music->gain = 0;
				gain = music->gain*musicvol;
				for(j=0; j<2; j++) mix[2*i+j] = sample(bgm+4*i+2*j)*gain;
			}
			if(music->gain == 0 && music->step < 0){
				freemusic(music);
				music = nil;
			}
		}
		for(k=0; k<Nvoice; k++){
			v = &voices[k];
			if(v->sound == nil) continue;
			n = (v->sound->bytes-v->offset)/4;
			if(n > Nframe) n = Nframe;
			for(i=0; i<n*2; i++)
				mix[i] += sample(v->sound->data+v->offset+2*i)*v->gain*effectvol;
			v->offset += n*4;
			if(v->offset == v->sound->bytes) v->sound = nil;
		}
		qunlock(&audiolock);
		for(i=0; i<Nframe*2; i++){
			value = mix[i];
			if(value > 32767) value = 32767;
			if(value < -32768) value = -32768;
			out[2*i] = value;
			out[2*i+1] = value>>8;
		}
		if(write(audiofd,out,sizeof(out)) != sizeof(out)){
			qlock(&audiolock);
			snprint(audioerror,sizeof(audioerror),"write audio: %r");
			qunlock(&audiolock);
			return;
		}
	}
readerror:
	snprint(audioerror,sizeof(audioerror),"read music: %r");
	qunlock(&audiolock);
}

void
audio_check(void)
{
	char err[ERRMAX];

	qlock(&audiolock);
	memmove(err,audioerror,sizeof(err));
	qunlock(&audiolock);
	if(err[0]) sysfatal("%s",err);
}

void
audio_init(void)
{
	int pid;

	if(audioowner != 0) return;
	audiofd = open("/dev/audio",OWRITE);
	if(audiofd < 0){
		fprint(2,"Neverputt: audio unavailable: %r\n");
		return;
	}
	audioquit = 0;
	audioerror[0] = 0;
	audio_volume(config_get_d(CONFIG_SOUND_VOLUME),config_get_d(CONFIG_MUSIC_VOLUME));
	pid = rfork(RFPROC|RFMEM|RFNOWAIT);
	if(pid < 0) sysfatal("audio process: %r");
	if(pid == 0){
		mixaudio();
		while(rendezvous(&audiolock,nil) == (void*)~(uintptr)0) {}
		_exits(nil);
	}
	audioowner = getpid();
	atexit(audio_free);
}

void
audio_free(void)
{
	Sound *s;

	if(audioowner == 0 || audioowner != getpid()) return;
	qlock(&audiolock);
	audioquit = 1;
	qunlock(&audiolock);
	/* Join only the mixer; wait messages belong to the caller. */
	while(rendezvous(&audiolock,nil) == (void*)~(uintptr)0) {}
	audioowner = 0;
	close(audiofd);
	audiofd = -1;
	memset(voices,0,sizeof voices);
	freemusic(music);
	freemusic(queued);
	music = queued = nil;
	while((s=sounds) != nil){
		sounds = s->next;
		free(s->data);
		free(s->name);
		free(s);
	}
	atexitdont(audio_free);
}

void
audio_play(const char *name, float gain)
{
	Sound *s;
	char *pcm;
	int i, slot;

	if(audiofd < 0) return;
	for(s=sounds; s!=nil; s=s->next)
		if(strcmp(s->name,name) == 0) break;
	if(s == nil){
		s = mallocz(sizeof(*s),1);
		if(s == nil) sysfatal("sound allocation: %r");
		pcm = pcmname(name);
		s->data = fs_load(pcm,&s->bytes);
		free(pcm);
		if(s->data == nil || (s->bytes&3)) sysfatal("invalid or missing sound %s; run mk data",name);
		s->name = strdup(name);
		s->next = sounds;
		sounds = s;
	}
	if(gain < 0) gain=0;
	if(gain > 1) gain=1;
	qlock(&audiolock);
	slot = -1;
	for(i=0; i<Nvoice; i++){
		if(voices[i].sound == s){ slot=i; break; }
		if(voices[i].sound == nil) slot=i;
	}
	if(slot >= 0) voices[slot] = (Voice){s,0,gain};
	qunlock(&audiolock);
}

void
audio_volume(int effects, int background)
{
	qlock(&audiolock);
	effectvol = effects/10.f;
	musicvol = background/10.f;
	qunlock(&audiolock);
}

void
audio_music_stop(void)
{
	qlock(&audiolock);
	freemusic(music);
	music = nil;
	qunlock(&audiolock);
}

void
audio_music_play(const char *name)
{
	Music *m;

	if(audiofd < 0) return;
	m = newmusic(name,0);
	if(m != nil) m->gain = 0;
	qlock(&audiolock);
	freemusic(music);
	music = m;
	qunlock(&audiolock);
}

void
audio_music_queue(const char *name, float seconds)
{
	Music *m;

	if(audiofd < 0) return;
	m = newmusic(name,seconds);
	qlock(&audiolock);
	freemusic(queued);
	queued = m;
	qunlock(&audiolock);
}

void
audio_music_fade_out(float seconds)
{
	qlock(&audiolock);
	if(music != nil){
		if(seconds <= 0) music->gain=0;
		music->step = seconds>0 ? -1/(Rate*seconds) : -1;
	}
	qunlock(&audiolock);
}

void
audio_music_fade_in(float seconds)
{
	qlock(&audiolock);
	if(music != nil){
		if(seconds <= 0) music->gain=1;
		music->step = seconds>0 ? 1/(Rate*seconds) : 0;
	}
	qunlock(&audiolock);
}

void
audio_music_fade_to(float seconds, const char *name)
{
	int same, playing;

	if(audiofd < 0) return;
	qlock(&audiolock);
	playing = music != nil;
	same = playing && name != nil && strcmp(music->name,name) == 0;
	if(same){
		freemusic(queued);
		queued = nil;
	}
	qunlock(&audiolock);
	if(same)
		audio_music_fade_in(seconds);
	else if(playing){
		audio_music_fade_out(seconds);
		audio_music_queue(name,seconds);
	}else{
		audio_music_play(name);
		audio_music_fade_in(seconds);
	}
}
