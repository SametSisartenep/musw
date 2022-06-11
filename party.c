#include <u.h>
#include <libc.h>
#include <ip.h>
#include <draw.h>
#include "libgeometry/geometry.h"
#include "dat.h"
#include "fns.h"

Party theparty;


void
inittheparty(void)
{
	theparty.next = theparty.prev = &theparty;
}

Party *
newparty(Player *players)
{
	Party *p;

	p = emalloc(sizeof(Party));
	p->players[0] = players[0];
	p->players[1] = players[1];

	p->u = newuniverse();

	addparty(p);

	return p;
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
addparty(Party *p)
{
	p->prev = theparty.prev;
	p->next = &theparty;
	theparty.prev->next = p;
	theparty.prev = p;
}
