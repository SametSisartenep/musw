#include <u.h>
#include <libc.h>
#include <ip.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

Party *
newparty(Party *p, Player *players)
{
	Party *np;

	np = emalloc(sizeof(Party));
	np->players[0] = players[0];
	np->players[1] = players[1];

	np->u = newuniverse();

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
addparty(Party *theparty, Party *p)
{
	p->prev = theparty->prev;
	p->next = theparty;
	theparty->prev->next = p;
	theparty->prev = p;
}

void
initparty(Party *p)
{
	p->next = p->prev = p;
}
