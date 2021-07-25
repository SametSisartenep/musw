#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

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
	FPdbleword d;

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
		}
	}
err:
	return -1;
}

static int
vunpack(uchar *p, int n, char *fmt, va_list a)
{
	uchar *p0 = p, *e = p+n;
	FPdbleword d;

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
