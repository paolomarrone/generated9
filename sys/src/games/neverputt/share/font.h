#ifndef FONT_H
#define FONT_H
#include "port.h"
#include <bio.h>
#include <ttf.h>
#include "base_config.h"
struct font {
    char path[PATHMAX];
    TTFont *ttf[3];
};
int font_load(struct font *, const char *, int[3]);
void font_free(struct font *);
int font_init(void);
void font_quit(void);
void font_measure(TTFont *, const char *, int *, int *);
#endif
