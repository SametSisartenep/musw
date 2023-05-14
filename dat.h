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

typedef enum {
	NCSDisconnected,
	NCSConnecting,
	NCSConnected
} NCState;

enum {
	SCRW	= 640,
	SCRH	= 480,
	SCRWB	= SCRW+2*Borderwidth,
	SCRHB	= SCRH+2*Borderwidth
};

enum {
	NChi		= 10,	/* C wants to connect */
	NShi,			/* S accepts. sends P and G for DHX */
	NCdhx		= 12,	/* C shares pubkey */
	NSdhx,			/* S shares pubkey */
	NCnudge		= 16,	/* C ACKs nudge */
	NSnudge,		/* S checks the pulse of the line */

	NCinput		= 20,	/* C sends player input state */
	NSsimstate,		/* S sends current simulation state */
	NCawol		= 22,	/* C ACKs AWOL */
	NSawol,			/* S notifies the adversary flew away */

	NCbuhbye	= 30,	/* C quits gracefully */
	NSbuhbye,		/* S kicks the player out */

	NSerror 	= 66	/* S reports an error */
};

enum {
	ProtocolID	= 0x5753554d,	/* MUSW */
	Framehdrsize	= 4+1+4+4+2+MD5dlen,
	MTU		= 1024,
	ConnTimeout	= 10000		/* in ms */
};

enum {
	THRUST = 10
};

typedef struct VModel VModel;
typedef struct Sprite Sprite;
typedef struct Keymap Keymap;
typedef struct State State;
typedef struct Particle Particle;
typedef struct Bullet Bullet;
typedef struct Ship Ship;
typedef struct Star Star;
typedef struct Universe Universe;
typedef struct Derivative Derivative;

typedef struct Frame Frame;
typedef struct DHparams DHparams;
typedef struct NetConn NetConn;
typedef struct Player Player;
typedef struct Playerq Playerq;
typedef struct Party Party;

/*
 * Vector model - made out of lines and curves
 */
struct VModel
{
	Point2 *pts;
	usize npts;
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

struct Keymap
{
	Rune key;
	KeyOp op;
};

struct State
{
	char *name;

	State *(*δ)(State*, void*); /* transition fn */
};

struct Particle
{
	Point2 p, v;
	double θ, ω;
	double mass; /* in kg */
};

struct Bullet
{
	Particle;
	double ttl; /* in s */
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

	void (*forward)(Ship*, double);
	void (*rotate)(Ship*, int, double);
	void (*hyperjump)(Ship*);
	void (*fire)(Ship*);
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
	void (*collide)(Universe*);
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
	u32int id;	/* ProtocolID */
	u8int type;
	u32int seq;
	u32int ack;
	u16int len;
	uchar sig[MD5dlen];
	uchar data[];
};

struct DHparams
{
	ulong p, g, pub, sec, priv;
};

struct NetConn
{
	Udphdr udp;
	DHparams dh;
	NCState state;
	u32int lastseq;
	u32int lastack;
	ulong lastrecvts;	/* last time a packet was received (in ms) */
	ulong lastnudgets;	/* last time a nudge was sent (in ms) */
	Player *player;
};

struct Player
{
	char *name;
	NetConn *conn;
	ulong oldkdown, kdown;
	Player *next;
};

struct Playerq
{
	Player *head, *tail;
	usize len;

	void (*put)(Playerq*, Player*);
	Player *(*get)(Playerq*);
	void (*del)(Playerq*, Player*);
};

struct Party
{
	Player *players[2];	/* the needle and the wedge */
	Universe *u;
	Party *prev, *next;
};

#pragma varargck type "Φ" Frame*
