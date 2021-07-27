#define FPS2MS(fps)	(1000/(fps))

/*
 * alloc
 */
void *emalloc(ulong);
void *erealloc(void*, ulong);
//Image *eallocimage(Display*, Rectangle, ulong, int, ulong);

/*
 * physics
 */
void integrate(GameState*, double, double);

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
 * lobby
 */
void inittheparty(void);
Party *newparty(Player[2]);
void delparty(Party*);
void addparty(Party*);

