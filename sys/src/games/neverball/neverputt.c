/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERPUTT is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

/* Native event loop adapted from putt/main.c. */
#include "port.h"
#include "audio.h"
#include "config.h"
#include "fs.h"
#include "putt/game.h"
#include "putt/hole.h"
#include "mtrl.h"
#include "state.h"
#include "putt/st_all.h"
#include "putt/st_conf.h"
#include "video.h"

static void
usage(void)
{
	fprint(2,"usage: games/neverputt [-d data-directory] [-h hole.sol] [-n frames]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *data, *hole;
	const char *path;
	int limit;

	data = hole = nil;
	limit = 0;
	ARGBEGIN{
	case 'd': data = EARGF(usage()); break;
	case 'h': hole = EARGF(usage()); break;
	case 'n': limit = atoi(EARGF(usage())); if(limit < 1) usage(); break;
	default: usage();
	}ARGEND
	if(argc) usage();
	fs_init(argv0);
	config_paths(data,"lib/neverputt");
	config_init();
	config_load();
	/* rio owns the window; this backend provides a single classic GL target. */
	config_set_d(CONFIG_FULLSCREEN,0);
	config_set_d(CONFIG_REFLECTION,0);
	config_set_d(CONFIG_MULTISAMPLE,0);
	config_set_d(CONFIG_HMD,0);
	log_init("Neverputt","neverputt.log");
	fs_mkdir("Screenshots");
	lang_init();
	srand(time(nil));
	if(!video_init("Neverputt")) sysfatal("video initialization failed");
	audio_init();
	mtrl_init();
	init_state(&st_null);
	path = hole == nil ? nil : fs_resolve(hole);
	if(path != nil){
		hole_init(nil);
		if(!hole_load(0,path) || !hole_load(1,path) || !hole_goto(1,1))
			sysfatal("load hole %s",path);
		goto_state(&st_next);
	}else if(hole != nil)
		sysfatal("hole not found: %s",hole);
	else
		goto_state(&st_title);
	game_loop(limit);
	audio_free();
	goto_state(&st_null);
	mtrl_quit();
	config_save();
	log_quit();
	fs_quit();
	video_quit();
	exits(nil);
}
