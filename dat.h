typedef enum
{
	K↑,
	K↺,
	K↻,
	Kfire,
	Khyper,
	Ksay,
	Kquit,
	NKEYOPS
} KeyOp;

typedef enum
{
	NEEDLE,
	WEDGE
} Kind;

enum {
	SCRW	= 640,
	SCRH	= 480,
	SCRWB	= SCRW+2*Borderwidth,
	SCRHB	= SCRH+2*Borderwidth
};

typedef struct Vector Vector;
typedef struct VModel VModel;
typedef struct Sprite Sprite;
typedef struct Particle Particle;
typedef struct Ship Ship;
typedef struct Star Star;
typedef struct Universe Universe;
typedef struct GameState GameState;
typedef struct Derivative Derivative;
typedef struct Conn Conn;
typedef struct Player Player;
typedef struct Lobby Lobby;
typedef struct Party Party;

struct Vector
{
	double x, y;
};

/*
 * Vector model - made out of lines and curves
 */
struct VModel
{
	Vector *pts;
	ulong npts;
	/* WIP
	 * l(ine) → takes 2 points
	 * c(urve) → takes 3 points
	 */
//	char *strokefmt;
};

struct Sprite
{
	Image *sheet;
	Point sp;
	Rectangle r;
	int nframes;
	int curframe;
	ulong period;
	ulong elapsed;

	void (*step)(Sprite*, ulong);
	void (*draw)(Sprite*, Image*, Point);
};

struct Particle
{
	Vector p, v;
	double yaw;
	double mass;
};

struct Ship
{
	Particle;
	Kind kind;
	uint ammo;
	VModel *mdl;
//	Matrix mdlxform;
};

struct Star
{
	Particle;
	Sprite spr;
};

struct Universe
{
	Ship ships[2];
	Star star;

	int (*step)(Universe*);
};

struct GameState
{
	double t, timeacc;
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
	void (*purge)(Lobby*);
};

struct Party
{
	Player players[2];	/* the needle and the wedge */
	Universe *u;
	Party *prev, *next;

	/* testing */
	GameState state;
};


extern Party theparty;
