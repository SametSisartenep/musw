#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

int debug;

GameState state;
double t, Δt;

Lobby *lobby;


static long
_iolisten(va_list *arg)
{
	char *adir, *ldir;

	adir = va_arg(*arg, char*);
	ldir = va_arg(*arg, char*);

	return listen(adir, ldir);
}

long
iolisten(Ioproc *io, char *adir, char *ldir)
{
	return iocall(io, _iolisten, adir, ldir);
}

void
threadlisten(void *arg)
{
	int lcfd, dfd;
	char *adir, ldir[40];
	Ioproc *io;

	adir = arg;
	io = ioproc();

	for(;;){
		lcfd = iolisten(io, adir, ldir);
		if(lcfd < 0){
			fprint(2, "iolisten: %r\n");
			continue;
		}
		/*
		 * handle connection and allocate user on a seat, ready
		 * to play
		 */
		dfd = accept(lcfd, ldir);
		if(dfd < 0){
			fprint(2, "accept: %r\n");
			continue;
		}

		lobby->takeseat(lobby, ldir, lcfd, dfd);

		if(debug)
			fprint(2, "added conn for %lud conns at %lud max\n",
				lobby->nseats, lobby->cap);
	}
}

void
broadcaststate(void)
{
	int i, n;
	uchar buf[256];
	Party *p;

	if(debug)
		fprint(2, "state: x=%g v=%g\n", state.x, state.v);

	for(p = theparty.next; p != &theparty; p = p->next)
		for(i = 0; i < nelem(p->players); i++){
			n = pack(buf, sizeof buf, "dd", state.x, state.v);
			if(write(p->players[i].conn.data, buf, n) != n){
				lobby->takeseat(lobby, p->players[i^1].conn.dir, p->players[i^1].conn.ctl, p->players[i^1].conn.data);
				delparty(p);
			}
		}

}

void
resetsim(void)
{
	t = 0;
	memset(&state, 0, sizeof state);
	state.x = 100;
}

void
threadsim(void *)
{
	uvlong then, now;
	double frametime, timeacc;
	Ioproc *io;
	Player couple[2];

	Δt = 0.01;
	then = nanosec();
	timeacc = 0;
	io = ioproc();

	resetsim();

	for(;;){
		lobby->healthcheck(lobby);

		if(debug){
			Party *p;
			ulong nparties = 0;

			for(p = theparty.next; p != &theparty; p = p->next)
				nparties++;

			fprint(2, "lobby status: %lud conns at %lud cap\n",
				lobby->nseats, lobby->cap);
			fprint(2, "party status: %lud parties going on\n",
				nparties);
		}

		if(lobby->getcouple(lobby, couple) != -1)
			newparty(couple);

		broadcaststate();

		now = nanosec();
		frametime = now - then;
		then = now;
		timeacc += frametime/1e9;

		while(timeacc >= Δt){
			integrate(&state, t, Δt);
			timeacc -= Δt;
			t += Δt;
		}

		iosleep(io, FPS2MS(1));
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-d] [-a addr]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int acfd;
	char adir[40], *addr;

	addr = "udp!*!112";
	ARGBEGIN{
	case 'a':
		addr = EARGF(usage());
		break;
	case 'd':
		debug++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc != 0)
		usage();

	acfd = announce(addr, adir);
	if(acfd < 0)
		sysfatal("announce: %r");

	if(debug)
		fprint(2, "listening on %s\n", addr);

	lobby = newlobby();
	inittheparty();

	threadcreate(threadlisten, adir, 4096);
	threadcreate(threadsim, nil, 4096);
	threadexits(nil);
}
