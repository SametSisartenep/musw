#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

int debug;

double t, Δt;

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
	int lcfd;
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
	}
}

void
resetsim(void)
{
	memset(&state, 0, sizeof(GameState));
	state.x = 100;
	state.stats.update = statsupdate;
	t = 0;
}

void
threadsim(void *)
{
	uvlong then, now;
	double frametime, timeacc;

	Δt = 0.01;
	then = nanosec();
	timeacc = 0;

	resetsim();

	for(;;){
		now = nanosec();
		frametime = now - then;
		then = now;
		timeacc += frametime/1e9;

		while(timeacc >= Δt){
			integrate(&state, t, Δt);
			timeacc -= Δt;
			t += Δt;
		}

		sleep(66);
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-d]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int acfd;
	char adir[40];

	ARGBEGIN{
	case 'd':
		debug++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc != 0)
		usage();

	acfd = announce("tcp!*!112", adir);
	if(acfd < 0)
		sysfatal("announce: %r");

	threadcreate(threadlisten, adir, 1024);
	threadcreate(threadsim, nil, 8192);
	threadexits(nil);
}
