#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>
#include <libsec.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

/* Ship */

static void
ship_forward(Ship *s, double Δt)
{
	Point2 v;
	Matrix R = {
		cos(s->θ), -sin(s->θ), 0,
		sin(s->θ),  cos(s->θ), 0,
		0, 0, 1,
	};

	v = mulpt2(xform(Vec2(1,0), R), THRUST*Δt);
	s->v = addpt2(s->v, v);
}

static void
ship_rotate(Ship *s, int dir, double Δt)
{
	s->θ += PI*dir*Δt;
}

static void
ship_hyperjump(Ship *s)
{
	USED(s);
	return;
}

static void
ship_fire(Ship *s)
{
	int i;
	Point2 bv;
	Matrix R = {
		cos(s->θ), -sin(s->θ), 0,
		sin(s->θ),  cos(s->θ), 0,
		0, 0, 1,
	};

	bv = mulpt2(xform(Vec2(1,0), R), 10*THRUST);

	for(i = 0; i < nelem(s->rounds); i++)
		if(!s->rounds[i].fired){
			s->rounds[i].p = s->p;
			s->rounds[i].v = addpt2(s->v, bv);
			s->rounds[i].θ = s->θ;
			s->rounds[i].fired++;
			break;
		}
}

/* Universe */

static void
universe_step(Universe *u, double Δt)
{
	integrate(u, u->t, Δt);
	u->timeacc -= Δt;
	u->t += Δt;
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

	u->ships[0].forward = u->ships[1].forward = ship_forward;
	u->ships[0].rotate = u->ships[1].rotate = ship_rotate;
	u->ships[0].hyperjump = u->ships[1].hyperjump = ship_hyperjump;
	u->ships[0].fire = u->ships[1].fire = ship_fire;
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
