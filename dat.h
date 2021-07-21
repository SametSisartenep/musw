typedef struct GameState GameState;
typedef struct Derivative Derivative;
typedef struct Stats Stats;
typedef struct Sprite Sprite;

struct Stats
{
	double cur;
	double total;
	double min, avg, max;
	uvlong nupdates;

	void (*update)(Stats*, double);
};

struct GameState
{
	double x, v;
	Stats stats;
};

struct Derivative
{
	double dx, dv;
};
