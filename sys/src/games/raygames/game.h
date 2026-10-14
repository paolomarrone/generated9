#include <u.h>
#include <libc.h>
#include <raylib.h>
#include <raymath9.h>

extern const int screenWidth, screenHeight;
extern char *gameTitle;

void InitGame(void);
bool UpdateGame(void); /* true while the game needs timed frames */
void DrawGame(void);
