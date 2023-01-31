#define HZ2MS(hz)	(1000/(hz))

/*
 * alloc
 */
void *emalloc(ulong);
void *erealloc(void*, ulong);
//Image *eallocimage(Display*, Rectangle, ulong, int, ulong);

/*
 * physics
 */
void integrate(Universe*, double, double);

/*
 * nanosec
 */
uvlong nanosec(void);

/*
 * pack
 */
int pack(uchar*, int, char*, ...);
int unpack(uchar*, int, char*, ...);

/*
 * lobby
 */
Lobby *newlobby(void);
void dellobby(Lobby*);

/*
 * party
 */
Party *newparty(Party*, Player[2]);
void delparty(Party*);
void addparty(Party*, Party*);
void initparty(Party*);

/*
 * universe
 */
Universe *newuniverse(void);
void deluniverse(Universe*);
void inituniverse(Universe*);

/*
 *	sprite
 */
Sprite *newsprite(Image*, Point, Rectangle, int, ulong);
Sprite *readsprite(char*, Point, Rectangle, int, ulong);
void delsprite(Sprite*);
