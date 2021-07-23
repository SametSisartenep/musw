#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

/*
 * these routines were taken directly from ssh(1).
 * they serve as a reference.
 */

static int
vpack(uchar *p, int n, char *fmt, va_list a)
{
	uchar *p0 = p, *e = p+n;
	u32int u;
//	mpint *m;
	void *s;
	int c;

	for(;;){
		switch(c = *fmt++){
		case '\0':
			return p - p0;
		case '_':
			if(++p > e) goto err;
			break;
		case '.':
			*va_arg(a, void**) = p;
			break;
		case 'b':
			if(p >= e) goto err;
			*p++ = va_arg(a, int);
			break;
		case 'm':
//			m = va_arg(a, mpint*);
//			u = (mpsignif(m)+8)/8;
			if(p+4 > e) goto err;
//			PUT4(p, u), p += 4;
			if(u > e-p) goto err;
//			mptober(m, p, u), p += u;
			break;
		case '[':
		case 's':
			s = va_arg(a, void*);
			u = va_arg(a, int);
			if(c == 's'){
				if(p+4 > e) goto err;
//				PUT4(p, u), p += 4;
			}
			if(u > e-p) goto err;
			memmove(p, s, u);
			p += u;
			break;
		case 'u':
			u = va_arg(a, int);
			if(p+4 > e) goto err;
//			PUT4(p, u), p += 4;
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
	u32int u;
//	mpint *m;
	void *s;

	for(;;){
		switch(*fmt++){
		case '\0':
			return p - p0;
		case '_':
			if(++p > e) goto err;
			break;
		case '.':
			*va_arg(a, void**) = p;
			break;
		case 'b':
			if(p >= e) goto err;
			*va_arg(a, int*) = *p++;
			break;
		case 'm':
			if(p+4 > e) goto err;
//			u = GET4(p), p += 4;
			if(u > e-p) goto err;
//			m = va_arg(a, mpint*);
//			betomp(p, u, m), p += u;
			break;
		case 's':
			if(p+4 > e) goto err;
//			u = GET4(p), p += 4;
			if(u > e-p) goto err;
			*va_arg(a, void**) = p;
			*va_arg(a, int*) = u;
			p += u;
			break;
		case '[':
			s = va_arg(a, void*);
			u = va_arg(a, int);
			if(u > e-p) goto err;
			memmove(s, p, u);
			p += u;
			break;
		case 'u':
			if(p+4 > e) goto err;
//			u = GET4(p);
			*va_arg(a, int*) = u;
			p += 4;
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
