#include <u.h>
#include <libc.h>
#include <ip.h>
#include <draw.h>
#include "libgeometry/geometry.h"
#include "dat.h"
#include "fns.h"

static int
lobby_takeseat(Lobby *l, char *dir, int cfd, int dfd)
{
	if(l->nseats >= l->cap){
		l->cap += 8;
		l->seats = erealloc(l->seats, l->cap*sizeof(*l->seats));
	}

	l->seats[l->nseats].name = nil;
	memmove(l->seats[l->nseats].conn.dir, dir, sizeof l->seats[l->nseats].conn.dir);
	l->seats[l->nseats].conn.ctl = cfd;
	l->seats[l->nseats].conn.data = dfd;

	return l->nseats++;
}

static int
lobby_leaveseat(Lobby *l, ulong idx)
{
	if(idx >= l->cap)
		return -1;

	if(idx < l->cap - 1)
		memmove(&l->seats[idx], &l->seats[idx+1], l->cap*sizeof(*l->seats) - (idx + 1)*sizeof(*l->seats));

	return --l->nseats;
}

static int
lobby_getcouple(Lobby *l, Player *couple)
{
	if(l->nseats >= 2){
		couple[0] = l->seats[l->nseats-2];
		couple[1] = l->seats[l->nseats-1];

		if(l->nseats < l->cap - 2)
			memmove(&l->seats[l->nseats], &l->seats[l->nseats+2], l->cap*sizeof(*l->seats) - (l->nseats + 2)*sizeof(*l->seats));

		l->nseats -= 2;

		return 0;
	}

	return -1;
}

static void
lobby_purge(Lobby *l)
{
	char status[48], buf[16];
	int i, fd;

	for(i = 0; i < l->nseats; i++){
		snprint(status, sizeof status, "%s/status", l->seats[i].conn.dir);

		fd = open(status, OREAD);
		if(fd < 0)
			goto cleanup;

		if(read(fd, buf, sizeof buf) > 0)
			if(strncmp(buf, "Close", 5) == 0)
				goto cleanup;
			else{
				close(fd);
				continue;
			}
cleanup:
		close(fd);
		l->leaveseat(l, i);
	}
}

Lobby *
newlobby(void)
{
	Lobby *l;

	l = emalloc(sizeof(Lobby));
	memset(l, 0, sizeof(Lobby));
	l->takeseat = lobby_takeseat;
	l->getcouple = lobby_getcouple;
	l->leaveseat = lobby_leaveseat;
	l->purge = lobby_purge;

	return l;
}

void
dellobby(Lobby *l)
{
	free(l->seats);
	free(l);
}
