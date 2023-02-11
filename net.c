#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

/* DHX */

void
dhgenpg(ulong *p, ulong *g)
{
	static ulong P = 97;
	static ulong G = 71;

	*p = P;
	*g = G;
}

/*
 * x = g^k mod p
 */
ulong
dhgenkey(ulong g, ulong k, ulong p)
{
	ulong r, y;

	y = 1;

	while(k > 0){
		r = k % 2;
		if(r == 1)
			y = y*g % p;
		g = g*g % p;
		k /= 2;
	}
	return y;
}

/* NetConn */

NetConn *
newnetconn(NCState s, Udphdr *u)
{
	NetConn *nc;

	nc = emalloc(sizeof(NetConn));
	memset(nc, 0, sizeof(NetConn));
	if(u != nil)
		memmove(&nc->udp, u, Udphdrsize);
	nc->state = s;

	return nc;
}

void
delnetconn(NetConn *nc)
{
	free(nc);
}

/* Frame */

Frame *
newframe(Frame *pf, u8int type, u32int seq, u32int ack, u16int len, uchar *data)
{
	Frame *f;

	f = emalloc(sizeof(Frame)+len);
	f->id = ProtocolID;
	f->type = type;
	if(pf != nil){
		memmove(&f->udp, &pf->udp, Udphdrsize);
		f->seq = pf->seq+1;
		f->ack = pf->seq;
	}else{
		memset(&f->udp, 0, Udphdrsize);
		f->seq = seq;
		f->ack = ack;
	}
	f->len = len;
	if(data != nil)
		memmove(f->data, data, f->len);

	return f;
}

void
delframe(Frame *f)
{
	free(f);
}
