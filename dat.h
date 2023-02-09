typedef enum {
	K↑,
	K↺,
	K↻,
	Kfire,
	Khyper,
	Ksay,
	Kquit,
	NKEYOPS
} KeyOp;

typedef enum {
	NEEDLE,
	WEDGE
} Kind;

enum {
	SCRW	= 640,
	SCRH	= 480,
	SCRWB	= SCRW+2*Borderwidth,
	SCRHB	= SCRH+2*Borderwidth
};

enum {
	NChi	= 10,	/* C wants to connect */
	NShi,		/* S accepts */
	NCdhx0	= 12,	/* C asks for p and g */
	NSdhx0,		/* S sends them. it's not a negotiation */
	NCdhx1	= 14,	/* C shares pubkey */
	NSdhx1,		/* S shares pubkey */
	NCnudge	= 16,
	NSnudge,	/* check the pulse of the line */

	NCinput	= 20,	/* C sends player input state */
	NSsimstate,	/* S sends current simulation state */

	NCbuhbye	= 30,
	NSbuhbye
};

enum {
	Framehdrsize	= 1+4+4+2,
	MTU		= 1024
};

typedef struct VModel VModel;
typedef struct Sprite Sprite;
typedef struct Particle Particle;
typedef struct Bullet Bullet;
typedef struct Ship Ship;
typedef struct Star Star;
typedef struct Universe Universe;
typedef struct Derivative Derivative;

typedef struct Frame Frame;
typedef struct NetConn NetConn;
typedef struct Player Player;
typedef struct Party Party;

/*
 * Vector model - made out of lines and curves
 */
struct VModel
{
	Point2 *pts;
	ulong npts;
	/* WIP
	 * l(ine) → takes 2 points
	 * c(urve) → takes 3 points
	 */
	char *strokefmt;
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
	Point2 p, v;
	double θ, ω;
	double mass; /* kg */
};

struct Bullet
{
	Particle;
	ulong ttl; /* in s */
	int fired; /* XXX: |v| != 0 */
};

struct Ship
{
	Particle;
	Kind kind;
	int fuel;
	Bullet rounds[10];
	VModel *mdl;
	Matrix mdlxform;
};

struct Star
{
	Particle;
	Sprite *spr;
};

struct Universe
{
	Ship ships[2];
	Star star;
	double t, timeacc;

	void (*step)(Universe*, double);
	void (*reset)(Universe*);
};

struct Derivative
{
	Point2 dx; /* v */
	Point2 dv; /* a */
};

struct Frame
{
	Udphdr udp;
	u8int type;
	u32int seq;
	u32int ack;
	u16int len;
	uchar data[];
};

struct NetConn
{
	Udphdr udp;
	int isconnected;
};

struct Player
{
	char *name;
	NetConn conn;
	ulong okdown, kdown;
};

struct Party
{
	Player players[2];	/* the needle and the wedge */
	Universe *u;
	Party *prev, *next;
};
