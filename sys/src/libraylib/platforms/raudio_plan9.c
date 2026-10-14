/* The upstream mixer and resampler feed Plan 9's 44.1 kHz stereo PCM file. */
static void
mixaudio(void)
{
	float mix[512*2], volume, value;
	uchar pcm[512*4];
	int i, sample;

	for(;;){
		qlock(&AUDIO.System.lock);
		if(AUDIO.System.quit){
			qunlock(&AUDIO.System.lock);
			break;
		}
		volume = AUDIO.System.volume;
		qunlock(&AUDIO.System.lock);
		OnSendAudioDataToDevice(mix, 512);
		for(i=0; i<nelem(mix); i++){
			value = mix[i]*volume;
			sample = isNaN(value) ? 0 : value >= 1 ? 32767 : value <= -1 ? -32768 : value*32768;
			pcm[i*2] = sample;
			pcm[i*2+1] = sample>>8;
		}
		if(write(AUDIO.System.fd, pcm, sizeof pcm) != sizeof pcm){
			fprint(2, "raylib: write audio: %r\n");
			qlock(&AUDIO.System.lock);
			AUDIO.System.isReady = false;
			qunlock(&AUDIO.System.lock);
			break;
		}
	}
	/* Do not consume the application's wait messages. */
	while(rendezvous(&AUDIO.System, nil) == (void*)~(uintptr)0) {}
	_exits(nil);
}

void
InitAudioDevice(void)
{
	int pid;
	char err[ERRMAX];

	if(AUDIO.System.pid != 0) return;
	AUDIO.System.fd = open("/dev/audio", OWRITE);
	if(AUDIO.System.fd < 0){
		rerrstr(err, sizeof err);
		TraceLog(LOG_WARNING, "AUDIO: Open /dev/audio: %s", err);
		return;
	}
	AUDIO.System.owner = getpid();
	AUDIO.System.quit = 0;
	AUDIO.System.volume = 1;
	AUDIO.System.isReady = true;
	pid = rfork(RFPROC|RFMEM|RFNOWAIT);
	if(pid < 0){
		rerrstr(err, sizeof err);
		close(AUDIO.System.fd);
		AUDIO.System.isReady = false;
		TraceLog(LOG_WARNING, "AUDIO: Start mixer: %s", err);
		return;
	}
	if(pid == 0) mixaudio();
	AUDIO.System.pid = pid;
	atexit(CloseAudioDevice);
	TraceLog(LOG_INFO, "AUDIO: /dev/audio, 44100 Hz stereo");
}

void
CloseAudioDevice(void)
{
	if(AUDIO.System.pid == 0 || getpid() != AUDIO.System.owner) return;
	qlock(&AUDIO.System.lock);
	AUDIO.System.quit = 1;
	qunlock(&AUDIO.System.lock);
	while(rendezvous(&AUDIO.System, nil) == (void*)~(uintptr)0) {}
	close(AUDIO.System.fd);
	AUDIO.System.pid = 0;
	AUDIO.System.isReady = false;
	free(AUDIO.System.pcmBuffer);
	AUDIO.System.pcmBuffer = nil;
	AUDIO.System.pcmBufferSize = 0;
	atexitdont(CloseAudioDevice);
}

bool
IsAudioDeviceReady(void)
{
	int ready;

	qlock(&AUDIO.System.lock);
	ready = AUDIO.System.isReady;
	qunlock(&AUDIO.System.lock);
	return ready;
}

void
SetMasterVolume(float volume)
{
	qlock(&AUDIO.System.lock);
	AUDIO.System.volume = !(volume > 0) ? 0 : volume > 1 ? 1 : volume;
	qunlock(&AUDIO.System.lock);
}

float
GetMasterVolume(void)
{
	float volume;

	qlock(&AUDIO.System.lock);
	volume = AUDIO.System.volume;
	qunlock(&AUDIO.System.lock);
	return volume;
}
