#define IsKeyPressed testpressed
#define IsMouseButtonPressed testbutton
#define GetMousePosition testmouse
#include "../missile.c"

static int pressed, button = -1;
static Vector2 mouse;

bool testpressed(int key) { return key == pressed; }
bool testbutton(int b) { return b == button; }
Vector2 testmouse(void) { return mouse; }

void
main(void)
{
	int i;
	Interceptor saved;

	InitGame();
	button = MOUSE_BUTTON_LEFT;
	mouse = (Vector2){-1,100};
	UpdateOutgoingFire();
	if(interceptorNumber != 0) sysfatal("shot outside arena");
	/* Rejected aim must not change a live slot when the ring wraps. */
	interceptor[0].active = true;
	interceptor[0].position = (Vector2){123,234};
	saved = interceptor[0];
	mouse = launcher[0].position;
	UpdateOutgoingFire();
	if(interceptorNumber != 0 || memcmp(&saved,&interceptor[0],sizeof saved))
		sysfatal("zero-distance shot");

	InitGame();
	mouse = (Vector2){80,200};
	UpdateOutgoingFire();
	button = -1;
	for(i = 0; i < 25; i++) UpdateGame();
	if(interceptor[0].active || !explosion[0].active)
		sysfatal("interceptor detonation");

	/* Reusing an explosion slot must not inherit its previous hit radius. */
	InitGame();
	explosion[0].radiusMultiplier = 1;
	interceptor[0].active = true;
	interceptor[0].position = interceptor[0].objective = (Vector2){300,200};
	missile[0].active = true;
	missile[0].position = (Vector2){330,200};
	UpdateGame();
	if(!missile[0].active || score != 0)
		sysfatal("new explosion inherited an old hit radius");

	InitGame();
	for(i = 0; i < 2; i++){
		interceptor[i].active = true;
		interceptor[i].position = interceptor[i].objective = (Vector2){300+100*i,200};
	}
	UpdateGame();
	if(interceptor[0].active || interceptor[1].active || explosionIndex != 2)
		sysfatal("simultaneous interceptors must both detonate");
	explosion[0].frame = EXPLOSION_TOTAL_TIME-1;
	UpdateGame();
	if(fabs(explosion[0].radiusMultiplier) > .001)
		sysfatal("explosion must shrink to zero before expiring");
	UpdateGame();
	if(explosion[0].active) sysfatal("explosion lifetime");

	InitGame();
	missile[0].active = true;
	missile[0].position = launcher[0].position;
	UpdateGame();
	if(launcher[0].active || missile[0].active || score != 0 || explosionIndex != 1)
		sysfatal("impact must not also score a kill");
	InitGame();
	explosion[0].active = true;
	explosion[0].position = (Vector2){300,200};
	explosion[0].radiusMultiplier = 1;
	explosion[0].frame = EXPLOSION_INCREASE_TIME;
	explosionIndex = 1;
	missile[0].active = true;
	missile[0].position = explosion[0].position;
	UpdateGame();
	UpdateGame();
	if(missile[0].active || score != 100 || explosionIndex != 2)
		sysfatal("one score per intercepted missile");
	pressed = KEY_P;
	UpdateGame();
	if(!pause) sysfatal("pause");
	InitGame();
	if(pause || gameOver || score || framesCounter || interceptorNumber
	|| missileIndex || explosionIndex) sysfatal("restart");
	print("missile: ok\n");
	exits(nil);
}
