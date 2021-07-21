#define FPS2MS(fps)	(1000/(fps))

typedef struct GameState GameState;
typedef struct Derivative Derivative;
typedef struct Conn Conn;

struct GameState
{
	double x, v;
};

struct Derivative
{
	double dx, dv;
};

struct Conn
{
	int *fds;
	ulong off;
	ulong cap;
};
