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

Player *
newplayer(char *name, NetConn *nc)
{
	Player *p;

	p = emalloc(sizeof(Player));
	p->name = name? strdup(name): nil;
	p->conn = nc;
	p->inputq = chancreate(sizeof(ulong), 20);
	p->oldkdown = p->kdown = 0;
	p->next = nil;

	return p;
}

void
delplayer(Player *p)
{
	free(p->name);
	chanclose(p->inputq);
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

	for(np = pq->head; np != nil && np->next != nil; np = np->next)
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