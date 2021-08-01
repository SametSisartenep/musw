#include <u.h>
#include <libc.h>
#include <draw.h>
#include "libgeometry/geometry.h"
#include "dat.h"
#include "fns.h"

static void
universe_step(Universe *u, double Δt)
{
	//integrate(u, u->t, Δt);
}

static void
universe_reset(Universe *u)
{
	int i, j;

	for(i = 0; i < nelem(u->ships); i++){
		for(j = 0; j < nelem(u->ships[i].rounds); j++)
			memset(&u->ships[i].rounds[j], 0, sizeof(Bullet));
		memset(&u->ships[i].Particle, 0, sizeof(Particle));
	}
	memset(&u->star.Particle, 0, sizeof(Particle));
	inituniverse(u);
}

void
inituniverse(Universe *u)
{
	int i;
	double θ;
	Point2 aimstar;

	u->star.p = Pt2(0,0,1);
	u->star.mass = 5.97e24; /* earth's mass */

	θ = ntruerand(360)*DEG;
	for(i = 0; i < nelem(u->ships); i++){
		θ += i*180*DEG;
		Matrix R = {
			cos(θ), -sin(θ), 0,
			sin(θ),  cos(θ), 0,
			0, 0, 1
		};

		u->ships[i].p = addpt2(Pt2(0,0,1), mulpt2(xform(Vec2(1,0), R), 200));
		aimstar = subpt2(u->star.p, u->ships[i].p);
		u->ships[i].θ = atan2(aimstar.y, aimstar.x);
	}

	u->ships[0].mass = 10e3; /* 10 tons */
	u->ships[0].kind = NEEDLE;
	u->ships[0].fuel = 100;

	u->ships[1].mass = 40e3; /* 40 tons */
	u->ships[1].kind = WEDGE;
	u->ships[1].fuel = 200;
}

Universe *
newuniverse(void)
{
	Universe *u;

	u = emalloc(sizeof(Universe));
	memset(u, 0, sizeof *u);
	u->step = universe_step;
	u->reset = universe_reset;
	return u;
}

void
deluniverse(Universe *u)
{
	free(u);
}
