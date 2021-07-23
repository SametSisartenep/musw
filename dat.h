#define FPS2MS(fps)	(1000/(fps))

typedef struct GameState GameState;
typedef struct Derivative Derivative;
typedef struct Seats Seats;
typedef struct Lobby Lobby;

struct GameState
{
	double x, v;
};

struct Derivative
{
	double dx, dv;
};

struct Seats
{
	int *fds;
	ulong len;
	ulong cap;
};

struct Lobby
{
	Seats seats;

	int (*takeseat)(Lobby*, int);
	int (*getcouple)(Lobby*, int*);
	int (*leaveseat)(Lobby*, ulong);
};
