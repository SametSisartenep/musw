#include <u.h>
#include <libc.h>
#include <draw.h>
#include "libgeometry/geometry.h"
#include "dat.h"
#include "fns.h"

static double G = 6.674e-11;

/*
 *	Dynamics stepper
 */
static Point2
accel(GameState *s, double)
{
	static double k = 15, b = 0.1;

	return Vec2(0, -k*s->p.y - b*s->v.y);
}

/*
 * XXX: remember to take thrust into account, based on user input.
 */
static Point2
accelship(Universe *u, Ship *s, double)
{
	double g, d;

	d = vec2len(subpt2(u->star.p, s->p));
	g = G*u->star.mass/(d*d);
	return mulpt2(normvec2(subpt2(u->star.p, s->p)), g);
}

static Point2
accelbullet(Universe *, Bullet *, double)
{
	return Vec2(0,0);
}

static Derivative
eval(GameState *s0, double t, double Δt, Derivative *d)
{
	GameState s;
	Derivative res;

	s.p = addpt2(s0->p, mulpt2(d->dx, Δt));
	s.v = addpt2(s0->v, mulpt2(d->dv, Δt));

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
	static Derivative ZD = {0};
	Derivative d;

	d = eval(s, t, Δt, &ZD);

	s->p = addpt2(s->p, mulpt2(d.dx, Δt));
	s->v = addpt2(s->v, mulpt2(d.dv, Δt));
}

/*
 *	Semi-implicit Euler Integrator
 */
static void
euler1(GameState *s, double t, double Δt)
{
	static Derivative ZD = {0};
	Derivative d;

	d = eval(s, t, Δt, &ZD);

	s->v = addpt2(s->v, mulpt2(d.dv, Δt));
	s->p = addpt2(s->p, mulpt2(s->v, Δt));
}

/*
 *	RK4 Integrator
 */
static void
rk4(GameState *s, double t, double Δt)
{
	static Derivative ZD = {0};
	Derivative a, b, c, d;
	Point2 dxdt, dvdt;

	a = eval(s, t, 0, &ZD);
	b = eval(s, t, Δt/2, &a);
	c = eval(s, t, Δt/2, &b);
	d = eval(s, t, Δt, &c);

	dxdt = divpt2(addpt2(addpt2(a.dx, mulpt2(addpt2(b.dx, c.dx), 2)), d.dx), 6);
	dvdt = divpt2(addpt2(addpt2(a.dv, mulpt2(addpt2(b.dv, c.dv), 2)), d.dv), 6);

	s->p = addpt2(s->p, mulpt2(dxdt, Δt));
	s->v = addpt2(s->v, mulpt2(dvdt, Δt));
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
