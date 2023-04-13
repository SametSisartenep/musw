#define HZ2MS(hz)	(1000/(hz))

/*
 * alloc
 */
void *emalloc(ulong);
void *erealloc(void*, ulong);
Image *eallocimage(Display*, Rectangle, ulong, int, ulong);

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
 * party
 */
Party *newparty(Party*, Player*, Player*);
void delparty(Party*);
void addparty(Party*, Party*);
void initparty(Party*);
Player *newplayer(char*, NetConn*);
void delplayer(Player*);
void initplayerq(Playerq*);

/*
 * universe
 */
Universe *newuniverse(void);
void deluniverse(Universe*);
void inituniverse(Universe*);

/*
 * sprite
 */
Sprite *newsprite(Image*, Point, Rectangle, int, ulong);
Sprite *readsprite(char*, Point, Rectangle, int, ulong);
Sprite *readpngsprite(char*, Point, Rectangle, int, ulong);
void delsprite(Sprite*);

/*
 * net
 */
void dhgenpg(ulong*, ulong*);
ulong dhgenkey(ulong, ulong, ulong);
NetConn *newnetconn(NCState, Udphdr*);
void delnetconn(NetConn*);
Frame *newframe(Udphdr*, u8int, u32int, u32int, u16int, uchar*);
void signframe(Frame*, ulong);
int verifyframe(Frame*, ulong);
void delframe(Frame*);

/*
 * fmt
 */
int Φfmt(Fmt*);
