#include <u.h>
#include <libc.h>
#include <draw.h> /* because of dat.h */
#include "dat.h"
#include "fns.h"

/*
 *	Dynamics stepper
 */
static double
accel(GameState *s, double)
{
	static double k = 15, b = 0.1;

	return -k*s->x - b*s->v;
}

static Derivative
eval(GameState *s0, double t, double Δt, Derivative *d)
{
	GameState s;
	Derivative res;

	s.x = s0->x + d->dx*Δt;
	s.v = s0->v + d->dv*Δt;

	res.dx = s.v;
	res.dv = accel(&s, t+Δt);
	return res;
}

/*
 *	Explicit Euler Integrator
 */
static void
euler0(GameState *s, double t, double Δt)
{
	static Derivative ZD = {0,0};
	Derivative d;

	d = eval(s, t, Δt, &ZD);

	s->x += d.dx*Δt;
	s->v += d.dv*Δt;
}

/*
 *	Semi-implicit Euler Integrator
 */
static void
euler1(GameState *s, double t, double Δt)
{
	static Derivative ZD = {0,0};
	Derivative d;

	d = eval(s, t, Δt, &ZD);

	s->v += d.dv*Δt;
	s->x += s->v*Δt;
}

/*
 *	RK4 Integrator
 */
static void
rk4(GameState *s, double t, double Δt)
{
	static Derivative ZD = {0,0};
	Derivative a, b, c, d;
	double dxdt, dvdt;

	a = eval(s, t, 0, &ZD);
	b = eval(s, t, Δt/2, &a);
	c = eval(s, t, Δt/2, &b);
	d = eval(s, t, Δt, &c);

	dxdt = 1.0/6 * (a.dx + 2*(b.dx + c.dx) + d.dx);
	dvdt = 1.0/6 * (a.dv + 2*(b.dv + c.dv) + d.dv);

	s->x += dxdt*Δt;
	s->v += dvdt*Δt;
}

/*
 *	The Integrator
 */
void
integrate(GameState *s, double t, double Δt)
{
	//euler0(s, t, Δt);
	//euler1(s, t, Δt);
	rk4(s, t, Δt);
}
