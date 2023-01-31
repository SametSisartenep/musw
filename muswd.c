#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

int debug;

Lobby *lobby;
Party theparty;


void
threadlisten(void *arg)
{
	uchar buf[MTU], *p, *e;
	int fd, n;
	ushort rport, lport;
	ulong kdown;
	Ioproc *io;
	Frame *frame;

	fd = *(int*)arg;
	io = ioproc();
	frame = emalloc(sizeof(Frame));

	while((n = ioread(io, fd, buf, sizeof buf)) > 0){
		p = buf;
		e = buf+n;

		unpack(p, e-p, "F", frame);

		rport = frame->udp->rport[0]<<8 | frame->udp->rport[1];
		lport = frame->udp->lport[0]<<8 | frame->udp->lport[1];
		
		unpack(frame->data, frame->len, "k", &kdown);

		if(debug)
			fprint(2, "%I!%d → %I!%d | %d (%d) rcvd seq %ud ack %ud id %ud len %ud %.*lub\n",
				frame->udp->raddr, rport, frame->udp->laddr, lport, threadid(), getpid(), frame->seq, frame->ack, frame->id, frame->len, sizeof(kdown)*8, kdown);
	}
	closeioproc(io);
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
			newparty(&theparty, couple);
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
	int acfd, adfd;
	char adir[40], *addr, aux[64];

	GEOMfmtinstall();
	fmtinstall('I', eipfmt);
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

	/* we don't want a line per client. we want it RAW */
	if(fprint(acfd, "headers") < 0)
		sysfatal("couldn't set udp headers option: %r");

	snprint(aux, sizeof aux, "%s/data", adir);
	adfd = open(aux, ORDWR);
	if(adfd < 0)
		sysfatal("open: %r");

	if(debug)
		fprint(2, "listening on %s\n", addr);

	lobby = newlobby();
	initparty(&theparty);

	threadcreate(threadC2, nil, 4096);
	threadcreate(threadlisten, &adfd, 4096);
	threadcreate(threadsim, nil, 4096);
	threadexits(nil);
}
