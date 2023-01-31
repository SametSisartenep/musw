#include <u.h>
#include <libc.h>
#include <ip.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

static double G = 6.674e-11;


/*
 *	Dynamics stepper
 */
static Point2
accelship(Universe *u, Particle *p, double)
{
	double g, d;

	/* TODO: take thrust into account, based on user input. */
	d = vec2len(subpt2(u->star.p, p->p));
	d *= 1e5; /* scale to the 100km/px range */
	g = G*u->star.mass/(d*d);
	return mulpt2(normvec2(subpt2(u->star.p, p->p)), g);
}

static Point2
accelbullet(Universe *, Particle *, double)
{
	return Vec2(0,0);
}

static Derivative
eval(Universe *u, Particle *p0, double t, double Δt, Derivative *d, Point2 (*accel)(Universe*,Particle*,double))
{
	Particle p;
	Derivative res;

	p.p = addpt2(p0->p, mulpt2(d->dx, Δt));
	p.v = addpt2(p0->v, mulpt2(d->dv, Δt));

	res.dx = p.v;
	res.dv = accel(u, &p, t+Δt);
	return res;
}

/*
 *	Semi-implicit Euler Integrator
 */
static void
euler1(Universe *u, Particle *p, double t, double Δt, Point2 (*accel)(Universe*,Particle*,double))
{
	static Derivative ZD = {0};
	Derivative d;

	d = eval(u, p, t, Δt, &ZD, accel);

	p->v = addpt2(p->v, mulpt2(d.dv, Δt));
	p->p = addpt2(p->p, mulpt2(p->v, Δt));
}

/*
 *	RK4 Integrator
 */
static void
rk4(Universe *u, Particle *p, double t, double Δt, Point2 (*accel)(Universe*,Particle*,double))
{
	static Derivative ZD = {0};
	Derivative a, b, c, d;
	Point2 dxdt, dvdt;

	a = eval(u, p, t, 0, &ZD, accel);
	b = eval(u, p, t, Δt/2, &a, accel);
	c = eval(u, p, t, Δt/2, &b, accel);
	d = eval(u, p, t, Δt, &c, accel);

	dxdt = divpt2(addpt2(addpt2(a.dx, mulpt2(addpt2(b.dx, c.dx), 2)), d.dx), 6);
	dvdt = divpt2(addpt2(addpt2(a.dv, mulpt2(addpt2(b.dv, c.dv), 2)), d.dv), 6);

	p->p = addpt2(p->p, mulpt2(dxdt, Δt));
	p->v = addpt2(p->v, mulpt2(dvdt, Δt));
}

/*
 *	The Integrator
 */
void
integrate(Universe *u, double t, double Δt)
{
	int i, j;

	for(i = 0; i < nelem(u->ships); i++){
		rk4(u, &u->ships[i], t, Δt, accelship);
		for(j = 0; j < nelem(u->ships[i].rounds); j++)
			if(u->ships[i].rounds[j].fired)
				euler1(u, &u->ships[i].rounds[j], t, Δt, accelbullet);
	}
}
