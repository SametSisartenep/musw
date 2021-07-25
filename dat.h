#define FPS2MS(fps)	(1000/(fps))

typedef struct GameState GameState;
typedef struct Derivative Derivative;
typedef struct Conn Conn;
typedef struct Player Player;
typedef struct Lobby Lobby;
typedef struct Party Party;

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
	char dir[40];
	int ctl;
	int data;
};

struct Player
{
	char *name;
	Conn conn;
};

struct Lobby
{
	Player *seats;
	ulong nseats;
	ulong cap;

	int (*takeseat)(Lobby*, char*, int, int);
	int (*leaveseat)(Lobby*, ulong);
	int (*getcouple)(Lobby*, Player*);
	void (*healthcheck)(Lobby*);
};

struct Party
{
	Player players[2];	/* the needle and the wedge */
	Party *prev, *next;
};

extern Party theparty;
