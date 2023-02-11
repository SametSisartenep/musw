#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>
#include <libsec.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

static ushort
get2(uchar *p)
{
	return p[0]<<8 | p[1];
}

static void
put2(uchar *p, ushort u)
{
	p[0] = u>>8, p[1] = u;
}

static ulong
get4(uchar *p)
{
	return p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3];
}

static void
put4(uchar *p, ulong u)
{
	p[0] = u>>24, p[1] = u>>16, p[2] = u>>8, p[3] = u;
}

static int
vpack(uchar *p, int n, char *fmt, va_list a)
{
	uchar *p0 = p, *e = p+n;
	ulong k;
	FPdbleword d;
	Point2 P;
	Frame *F = nil;

	for(;;){
		switch(*fmt++){
		case '\0':
			return p - p0;
		case 'd':
			d.x = va_arg(a, double);

			if(p+8 > e)
				goto err;

			put4(p, d.hi), p += 4;
			put4(p, d.lo), p += 4;

			break;
		case 'P':
			P = va_arg(a, Point2);

			if(p+3*8 > e)
				goto err;

			pack(p, n, "ddd", P.x, P.y, P.w), p += 3*8;

			break;
		case 'k':
			k = va_arg(a, ulong);

			if(p+4 > e)
				goto err;

			put4(p, k), p += 4;

			break;
		case 'F':
			F = va_arg(a, Frame*);

			if(p+Udphdrsize > e)
				goto err;

			memmove(p, &F->udp, Udphdrsize), p += Udphdrsize;
			/* fallthrough */
		case 'f':
			if(F == nil)
				F = va_arg(a, Frame*);

			if(p+Framehdrsize+F->len > e)
				goto err;

			put4(p, F->id), p += 4;
			*p++ = F->type;
			put4(p, F->seq), p += 4;
			put4(p, F->ack), p += 4;
			put2(p, F->len), p += 2;
			memmove(p, F->sig, MD5dlen), p += MD5dlen;

			if(p+F->len > e)
				goto err;
			memmove(p, F->data, F->len), p += F->len;

			break;
		}
	}
err:
	return -1;
}

static int
vunpack(uchar *p, int n, char *fmt, va_list a)
{
	uchar *p0 = p, *e = p+n;
	ulong k;
	FPdbleword d;
	Point2 P;
	Frame *F = nil;

	for(;;){
		switch(*fmt++){
		case '\0':
			return p - p0;
		case 'd':
			if(p+8 > e)
				goto err;

			d.hi = get4(p), p += 4;
			d.lo = get4(p), p += 4;
			*va_arg(a, double*) = d.x;

			break;
		case 'P':
			if(p+3*8 > e)
				goto err;

			unpack(p, n, "ddd", &P.x, &P.y, &P.w), p += 3*8;
			*va_arg(a, Point2*) = P;

			break;
		case 'k':
			if(p+4 > e)
				goto err;

			k = get4(p), p += 4;
			*va_arg(a, ulong*) = k;

			break;
		case 'F':
			if(p+Udphdrsize > e)
				goto err;

			F = va_arg(a, Frame*);

			memmove(&F->udp, p, Udphdrsize), p += Udphdrsize;
			/* fallthrough */
		case 'f':
			if(p+Framehdrsize > e)
				goto err;

			if(F == nil)
				F = va_arg(a, Frame*);

			F->id = get4(p), p += 4;
			F->type = *p++;
			F->seq = get4(p), p += 4;
			F->ack = get4(p), p += 4;
			F->len = get2(p), p += 2;
			memmove(F->sig, p, MD5dlen), p += MD5dlen;

			if(p+F->len > e)
				goto err;

			memmove(F->data, p, F->len), p += F->len;

			break;
		}
	}
err:
	return -1;
}

int
pack(uchar *p, int n, char *fmt, ...)
{
	va_list a;

	va_start(a, fmt);
	n = vpack(p, n, fmt, a);
	va_end(a);

	return n;
}

int
unpack(uchar *p, int n, char *fmt, ...)
{
	va_list a;

	va_start(a, fmt);
	n = vunpack(p, n, fmt, a);
	va_end(a);

	return n;
}
