#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>
#include <libsec.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

/* Party */

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

/* Player */

Player *
newplayer(char *name, NetConn *nc)
{
	Player *p;

	p = emalloc(sizeof(Player));
	p->name = name? strdup(name): nil;
	p->conn = nc;
	p->oldkdown = p->kdown = 0;
	p->next = nil;

	return p;
}

void
delplayer(Player *p)
{
	free(p->name);
	free(p);
}

/* Player queue */

static void
playerq_put(Playerq *pq, Player *p)
{
	if(pq->tail == nil)
		pq->head = pq->tail = p;
	else{
		pq->tail->next = p;
		pq->tail = p;
	}
	pq->len++;
}

static Player *
playerq_get(Playerq *pq)
{
	Player *p;

	if(pq->head == nil)
		return nil;

	p = pq->head;
	if(pq->head == pq->tail)
		pq->head = pq->tail = nil;
	else{
		pq->head = p->next;
		p->next = nil;
	}
	pq->len--;
	return p;
}

static void
playerq_del(Playerq *pq, Player *p)
{
	Player *np;

	if(pq->head == p){
		pq->get(pq);
		return;
	}

	for(np = pq->head; np->next != nil; np = np->next)
		if(np->next == p){
			np->next = np->next->next;
			p->next = nil;
			pq->len--;
		}
}

void
initplayerq(Playerq *pq)
{
	pq->head = pq->tail = nil;
	pq->len = 0;
	pq->put = playerq_put;
	pq->get = playerq_get;
	pq->del = playerq_del;
}
