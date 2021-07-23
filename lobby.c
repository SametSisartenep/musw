#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

static int
lobby_takeseat(Lobby *l, int fd)
{
	if(l->seats.len >= l->seats.cap){
		l->seats.cap += 8;
		l->seats.fds = erealloc(l->seats.fds, l->seats.cap*sizeof(*l->seats.fds));
	}

	l->seats.fds[l->seats.len] = fd;
	return l->seats.len++;
}

static int
lobby_getcouple(Lobby *l, int *fds)
{
	if(l->seats.len >= 2){
		fds[0] = l->seats.fds[l->seats.len-2];
		fds[1] = l->seats.fds[l->seats.len-1];

		if(l->seats.len < l->seats.cap-2)
			memmove(&l->seats.fds[l->seats.len], &l->seats.fds[l->seats.len+2], l->seats.cap*sizeof(int) - (l->seats.len + 2)*sizeof(int));

		l->seats.len -= 2;
		return 0;
	}
	return -1;
}

static int
lobby_leaveseat(Lobby *l, ulong idx)
{
	if(idx < l->seats.cap-1)
		memmove(&l->seats.fds[idx], &l->seats.fds[idx+1], l->seats.cap*sizeof(int) - (l->seats.len + 1)*sizeof(int));
	l->seats.len--;
	return 0;
}

Lobby *
newlobby(void)
{
	Lobby *l;

	l = emalloc(sizeof(Lobby));
	memset(l, 0, sizeof(*l));
	l->takeseat = lobby_takeseat;
	l->getcouple = lobby_getcouple;
	l->leaveseat = lobby_leaveseat;

	return l;
}

void
dellobby(Lobby *l)
{
	free(l->seats.fds);
	free(l);
}
