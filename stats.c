#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

static double min(double a, double b) { return a < b? a: b; }
static double max(double a, double b) { return a > b? a: b; }

void
statsupdate(Stats *s, double n)
{
	s->cur = n;
	s->total += s->cur;
	s->avg = s->total/++s->nupdates;
	s->min = min(s->cur, s->min);
	s->max = max(s->cur, s->max);
}
