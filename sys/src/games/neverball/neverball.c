/* Native entry point adapted from ball/main.c; GPL version 2 or later.
 * Copyright (C) 2003 Robert Kooima and the Neverball contributors.
 */
#include "port.h"
#include "audio.h"
#include "config.h"
#include "fs.h"
#include "mtrl.h"
#include "state.h"
#include "st_common.h"
#include "video.h"
#include "ball/demo.h"
#include "ball/game_client.h"
#include "ball/game_server.h"
#include "ball/level.h"
#include "ball/progress.h"
#include "ball/set.h"
#include "ball/st_demo.h"
#include "ball/st_level.h"
#include "ball/st_title.h"

static void
usage(void)
{
	fprint(2,"usage: games/neverball [-d data-directory] [-l level.sol | -r replay.nbr] [-n frames]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	struct level level;
	char *data, *map, *replay;
	const char *path;
	int limit;

	data = map = replay = nil;
	limit = 0;
	ARGBEGIN{
	case 'd': data = EARGF(usage()); break;
	case 'l': map = EARGF(usage()); break;
	case 'r': replay = EARGF(usage()); break;
	case 'n': limit = atoi(EARGF(usage())); if(limit < 1) usage(); break;
	default: usage();
	}ARGEND
	if(argc || (map && replay)) usage();
	fs_init(argv0);
	config_paths(data,"lib/neverball");
	config_init();
	config_load();
	config_set_d(CONFIG_FULLSCREEN,0);
	config_set_d(CONFIG_REFLECTION,0);
	config_set_d(CONFIG_MULTISAMPLE,0);
	config_set_d(CONFIG_HMD,0);
	log_init("Neverball","neverball.log");
	fs_mkdir("Screenshots");
	fs_mkdir("Replays");
	fs_mkdir("Scores");
	lang_init();
	srand(time(nil));
	if(!video_init("Neverball")) sysfatal("video initialization failed");
	audio_init();
	mtrl_init();
	init_state(&st_null);
	if(replay){
		if(!fs_add_path(dir_name(replay)) || !progress_replay(base_name(replay)))
			sysfatal("load replay %s",replay);
		demo_play_goto(1);
		goto_state(&st_demo_play);
	}else if(map){
		path = fs_resolve(map);
		if(path == nil || !level_load(path,&level))
			sysfatal("load level %s",map);
		progress_init(MODE_STANDALONE);
		if(!progress_play(&level)) sysfatal("start level %s",map);
		goto_state(&st_level);
	}else
		goto_state(&st_title);
	game_loop(limit);
	demo_replay_stop(0);
	audio_free();
	game_server_free(nil);
	game_client_free(nil);
	goto_state(&st_null);
	set_quit();
	mtrl_quit();
	config_save();
	log_quit();
	fs_quit();
	video_quit();
	exits(nil);
}
