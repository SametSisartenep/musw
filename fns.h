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
