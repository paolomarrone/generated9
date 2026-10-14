#ifndef NEVERBALL_PORT_H
#define NEVERBALL_PORT_H
#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <ctype.h>
#include <keyboard.h>

#define M_PI PI
#define RAND_MAX 32767
#define INT_MAX 2147483647
#define INT_MIN (-2147483647-1)
#define FLT_MAX 3.402823466e38F
#define ENABLE_NLS 0
#define ENABLE_OPENGLES 0
#define CONFIG_DATA "/sys/games/lib/neverball"
#define VERSION "1.6.0"

typedef uintptr size_t;
typedef long time_t;

/* Used only for the saved configuration and input dispatch. */
#define MOUSE_LEFT 1
#define MOUSE_MIDDLE 2
#define MOUSE_RIGHT 3

void game_loop(int);
void game_quit(void);

int key_code(const char*);
const char *key_name(int);
int key_shift(void);
int dir_make(const char*);
const char *display_name(int);
int display_count(void);
#endif
