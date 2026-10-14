#define IsKeyPressed testpressed
#define IsKeyDown testdown
#include "../arkanoid.c"

static int pressed, down;

bool testpressed(int key) { return key == pressed; }
bool testdown(int key) { return key == down; }

void
main(void)
{
	int i, j;
	Vector2 pos;

	InitGame();
	if(ball.position.y != player.position.y-player.size.y/2-ball.radius)
		sysfatal("initial ball position");
	pressed = KEY_SPACE;
	UpdateGame();
	pressed = 0;
	if(!ball.active || ball.speed.y >= 0) sysfatal("launch");
	for(i = 0; i < 40; i++) UpdateGame();
	if(!ball.active || ball.speed.y <= 0) sysfatal("brick bounce");
	j = 0;
	for(i = 0; i < BRICKS_PER_LINE; i++)
		if(!brick[LINES_OF_BRICKS-1][i].active) j++;
	if(j != 1) sysfatal("one brick per hit: %d",j);

	pos = ball.position;
	pressed = KEY_P;
	if(UpdateGame()) sysfatal("paused game requests frames");
	pressed = 0;
	for(i = 0; i < 10; i++) UpdateGame();
	if(!pause || ball.position.x != pos.x || ball.position.y != pos.y)
		sysfatal("pause");
	pressed = KEY_P;
	if(!UpdateGame() || pause) sysfatal("resume requests frames");
	if(UpdateGame()) sysfatal("pause before restart");
	pressed = 0;
	InitGame();
	if(pause || gameOver) sysfatal("restart while paused");
	for(i = 0; i < PLAYER_MAX_LIFE; i++){
		ball.active = true;
		ball.position = (Vector2){10,screenHeight};
		UpdateGame();
	}
	if(!gameOver || player.life != 0) sysfatal("lost lives");
	if(UpdateGame()) sysfatal("game over requests frames");
	pressed = KEY_ENTER;
	if(!UpdateGame()) sysfatal("restart requests frames");
	if(gameOver || player.life != PLAYER_MAX_LIFE) sysfatal("restart after loss");
	print("arkanoid: ok\n");
	exits(nil);
}
