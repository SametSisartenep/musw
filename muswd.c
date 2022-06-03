#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include "libgeometry/geometry.h"
#include "dat.h"
#include "fns.h"

int debug;

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
procnetrecv(void *arg)
{
	uchar buf[1024];
	int fd, n;
	ulong kdown;
	Ioproc *io;

	fd = *(int*)arg;
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof buf)) > 0){
		unpack(buf, n, "k", &kdown);
		fprint(2, "%d (%d) [%d] rcvd %.*lub\n", threadid(), threadpid(threadid()), getpid(), sizeof(kdown)*8, kdown);
	}
	closeioproc(io);
	threadexits(nil);
}

void
threadlisten(void *arg)
{
	int lcfd, dfd, pid;
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

		pid = proccreate(procnetrecv, &dfd, 4096);

		if(debug)
			fprint(2, "forked %d for new conn\n", pid);
	}
}

void
broadcaststate(void)
{
	int i, n;
	uchar buf[1024];
	Player *player;
	Party *p;

	for(p = theparty.next; p != &theparty; p = p->next){
		n = pack(buf, sizeof buf, "PdPdP",
			p->u->ships[0].p, p->u->ships[0].θ,
			p->u->ships[1].p, p->u->ships[1].θ,
			p->u->star.p);

		for(i = 0; i < nelem(p->players); i++){
			if(write(p->players[i].conn.data, buf, n) != n){
				player = &p->players[i^1];
				lobby->takeseat(lobby, player->conn.dir, player->conn.ctl, player->conn.data);
				/* step back and delete the spoiled party */
				p = p->prev;
				delparty(p->next);
				break;
			}
		}
	}

}

void
threadsim(void *)
{
	uvlong then, now;
	double frametime, Δt;
	Ioproc *io;
	Player couple[2];
	Party *p;

	Δt = 0.01;
	then = nanosec();
	io = ioproc();

	for(;;){
		lobby->purge(lobby);

		if(lobby->getcouple(lobby, couple) != -1){
			newparty(couple);
			theparty.prev->u->reset(theparty.prev->u);
		}

		now = nanosec();
		frametime = now - then;
		then = now;

		for(p = theparty.next; p != &theparty; p = p->next){
			p->u->timeacc += frametime/1e9;

			while(p->u->timeacc >= Δt){
				p->u->step(p->u, Δt);
				p->u->timeacc -= Δt;
				p->u->t += Δt;
			}
		}

		broadcaststate();

		iosleep(io, HZ2MS(70));
	}
}

void
fprintstats(int fd)
{
	ulong nparties = 0;
	Party *p;

	for(p = theparty.next; p != &theparty; p = p->next)
		nparties++;

	fprint(fd, "curplayers	%lud\n"
		   "totplayers	%lud\n"
		   "maxplayers	%lud\n"
		   "curparties	%lud\n"
		   "totparties	%lud\n",
		lobby->nseats, (ulong)0, lobby->cap,
		nparties, (ulong)0);
}

void
fprintstates(int fd)
{
	ulong i = 0;
	Party *p;
	Ship *s;

	for(p = theparty.next; p != &theparty; p = p->next, i++){
		for(s = &p->u->ships[0]; s-p->u->ships < nelem(p->u->ships); s++){
			fprint(fd, "%lud s%lld k%d p %v v %v θ %g ω %g m %g f %d\n",
				i, s-p->u->ships, s->kind, s->p, s->v, s->θ, s->ω, s->mass, s->fuel);
		}
		fprint(fd, "%lud S p %v m %g\n", i, p->u->star.p, p->u->star.mass);
	}
}


/* Command & Control */
void
threadC2(void *)
{
	int fd, pfd[2], n, ncmdargs;
	char buf[256], *usr, *cmdargs[2];
	Ioproc *io;

	if(pipe(pfd) < 0)
		sysfatal("pipe: %r");

	usr = getenv("user");
	snprint(buf, sizeof buf, "/srv/muswctl.%s.%d", usr, getpid());
	free(usr);

	fd = create(buf, OWRITE|ORCLOSE|OCEXEC, 0600);
	if(fd < 0)
		sysfatal("open: %r");
	fprint(fd, "%d", pfd[0]);
	close(pfd[0]);

	io = ioproc();
	while((n = ioread(io, pfd[1], buf, sizeof(buf)-1)) > 0){
		buf[n] = 0;

		ncmdargs = tokenize(buf, cmdargs, 2);
		if(ncmdargs == 2){
			if(strcmp(cmdargs[0], "show") == 0){
				if(strcmp(cmdargs[1], "stats") == 0)
					fprintstats(pfd[1]);
				else if(strcmp(cmdargs[1], "states") == 0)
					fprintstates(pfd[1]);
			}
		}
	}
	closeioproc(io);
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

	GEOMfmtinstall();
	addr = "tcp!*!112"; /* for testing. will work out udp soon */
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

	threadcreate(threadC2, nil, 4096);
	threadcreate(threadlisten, adir, 4096);
	threadcreate(threadsim, nil, 4096);
	threadexits(nil);
}
