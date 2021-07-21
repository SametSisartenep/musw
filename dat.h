#define FPS2MS(fps)	(1000/(fps))

typedef struct GameState GameState;
typedef struct Derivative Derivative;

struct GameState
{
	double x, v;
};

struct Derivative
{
	double dx, dv;
};
