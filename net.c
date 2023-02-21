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
 * y = g^k mod p
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
newframe(Udphdr *hdr, u8int type, u32int seq, u32int ack, u16int len, uchar *data)
{
	Frame *f;

	f = emalloc(sizeof(Frame)+len);
	memset(f, 0, sizeof(Frame));
	if(hdr != nil)
		memmove(&f->udp, hdr, Udphdrsize);
	f->id = ProtocolID;
	f->type = type;
	f->seq = seq;
	f->ack = ack;
	f->len = len;
	if(data != nil)
		memmove(f->data, data, f->len);

	return f;
}

void
signframe(Frame *f, ulong key)
{
	uchar k[sizeof(ulong)];
	uchar h[MD5dlen];
	uchar msg[MTU];
	int n;

	k[0] = key; k[1] = key>>8; k[2] = key>>16; k[3] = key>>24;

	memset(f->sig, 0, MD5dlen);
	n = pack(msg, sizeof msg, "f", f);
	hmac_md5(msg, n, k, sizeof k, h, nil);
	memmove(f->sig, h, MD5dlen);
}

int
verifyframe(Frame *f, ulong key)
{
	uchar k[sizeof(ulong)];
	uchar h0[MD5dlen], h1[MD5dlen];
	uchar msg[MTU];
	int n;

	k[0] = key; k[1] = key>>8; k[2] = key>>16; k[3] = key>>24;

	memmove(h0, f->sig, MD5dlen);
	memset(f->sig, 0, MD5dlen);
	n = pack(msg, sizeof msg, "f", f);
	hmac_md5(msg, n, k, sizeof k, h1, nil);
	return memcmp(h0, h1, MD5dlen) == 0;
}

void
delframe(Frame *f)
{
	free(f);
}
