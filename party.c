#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>
#include <libsec.h>
#include <thread.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

Party *
newparty(Party *p, Player *player0, Player *player1)
{
	Party *np;

	np = emalloc(sizeof(Party));
	np->players[0] = player0;
	np->players[1] = player1;

	np->u = newuniverse();
	inituniverse(np->u);

	addparty(p, np);

	return np;
}

void
delparty(Party *p)
{
	p->next->prev = p->prev;
	p->prev->next = p->next;
	deluniverse(p->u);
	free(p);
}

void
addparty(Party *p, Party *np)
{
	np->prev = p->prev;
	np->next = p;
	p->prev->next = np;
	p->prev = np;
}

void
initparty(Party *p)
{
	p->next = p->prev = p;
}
