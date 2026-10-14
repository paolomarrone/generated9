/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */


#include "port.h"
#include "base_config.h"
#include "common.h"
#include "fs.h"

void config_paths(const char *arg_data_path, const char *userdir)
{
    char *env, *home, *user;
    const char *data, *base;

    env = getenv("NEVERBALL_DATA");
    data = arg_data_path ? arg_data_path : env ? env : CONFIG_DATA;
    if (!fs_add_path_with_archives(data))
        sysfatal("game data directory %s not found", data);
    free(env);

    home = getenv("home");
    base = home ? home : fs_base_dir();
    user = concat_string(base, "/", userdir, NULL);
    if (!fs_set_write_dir(user))
    {
        if (!fs_set_write_dir(base) || !fs_mkdir(userdir) ||
            !fs_set_write_dir(user))
            sysfatal("create settings directory %s: %r", user);
    }
    fs_add_path_with_archives(user);
    free(user);
    free(home);
}
