#define IsKeyPressed testpressed
#define IsKeyDown testdown
#include "../asteroids.c"

static int pressed, down;

bool testpressed(int key) { return key == pressed; }
bool testdown(int key) { return key == down; }

void
main(void)
{
	int i;

	SetRandomSeed(1);
	InitGame();
	for(i = 0; i < MAX_BIG_METEORS; i++) bigMeteor[i].active = false;
	bigMeteor[0].active = true;
	bigMeteor[0].position = (Vector2){100,100};
	bigMeteor[0].speed = (Vector2){0,0};
	shoot[0].active = true;
	shoot[0].position = bigMeteor[0].position;
	shoot[0].speed = (Vector2){0,0};
	UpdateGame();
	if(shoot[0].active || bigMeteor[0].active || destroyedMeteorsCount != 1
	|| midMeteorsCount != 2 || smallMeteorsCount != 0
	|| !mediumMeteor[0].active || !mediumMeteor[1].active)
		sysfatal("one shot must split only one meteor");

	shoot[0].active = true;
	shoot[0].position = mediumMeteor[0].position;
	UpdateGame();
	if(shoot[0].active || mediumMeteor[0].active || !mediumMeteor[1].active
	|| destroyedMeteorsCount != 2 || smallMeteorsCount != 2
	|| !smallMeteor[0].active || !smallMeteor[1].active)
		sysfatal("one shot must preserve the new fragments");

	pressed = KEY_P;
	UpdateGame();
	pressed = 0;
	if(!pause) sysfatal("pause");
	gameOver = victory = true;
	InitGame();
	if(pause || gameOver || victory || midMeteorsCount || smallMeteorsCount
	|| destroyedMeteorsCount) sysfatal("restart");
	print("asteroids: ok\n");
	exits(nil);
}
