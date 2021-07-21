#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

int debug;

GameState state;
double t, Δt;

Conn conns;

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

		if(conns.off >= conns.cap){
			conns.cap += 8;
			conns.fds = erealloc(conns.fds, conns.cap*sizeof(*conns.fds));
		}
		conns.fds[conns.off++] = dfd;

		if(debug)
			fprint(2, "added conn %d for %lud conns at %lud max\n",
				dfd, conns.off, conns.cap);
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
	int i;
	uvlong then, now;
	double frametime, timeacc;
	Ioproc *io;

	Δt = 0.01;
	then = nanosec();
	timeacc = 0;
	io = ioproc();

	resetsim();

	for(;;){
		now = nanosec();
		frametime = now - then;
		then = now;
		timeacc += frametime/1e9;

		for(i = 0; i < conns.off; i++)
			if(fprint(conns.fds[i], "state: x=%g v=%g\n", state.x, state.v) < 0){
				fprint(2, "client #%d hanged up\n", i+1);
				if(conns.off < conns.cap-1)
					memmove(&conns.fds[conns.off], &conns.fds[conns.off+1], conns.cap-conns.off - 1);
				conns.off--;

				if(debug)
					fprint(2, "removed conn %d for %lud conns at %lud max\n",
						i, conns.off, conns.cap);

				i--;
			}

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

	conns.fds = emalloc(2*sizeof(*conns.fds));
	conns.cap = 2;

	threadcreate(threadlisten, adir, 4096);
	threadcreate(threadsim, nil, 4096);
	threadexits(nil);
}
