#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

int debug;
int mainstacksize = 24*1024;

Party theparty;
Channel *ingress;
Channel *egress;


void
threadnetrecv(void *arg)
{
	uchar buf[MTU];
	int fd, n;
	Ioproc *io;
	Frame *frame;

	threadsetname("threadnetrecv");

	fd = *(int*)arg;
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof buf)) > 0){
		frame = emalloc(sizeof(Frame)+(n-Udphdrsize-Framehdrsize));
		unpack(buf, n, "F", frame);
		sendp(ingress, frame);
	}
	closeioproc(io);
}

void
threadnetppu(void *)
{
	ushort rport, lport;
	ulong kdown;
	Frame *frame;

	threadsetname("threadnetppu");

	while((frame = recvp(ingress)) != nil){
		rport = frame->udp.rport[0]<<8 | frame->udp.rport[1];
		lport = frame->udp.lport[0]<<8 | frame->udp.lport[1];

		switch(frame->type){
		case NCinput:
			unpack(frame->data, frame->len, "k", &kdown);

			if(debug){
				fprint(2, "%I!%d ← %I!%d | rcvd type %ud seq %ud ack %ud len %ud %.*lub\n",
					frame->udp.laddr, lport, frame->udp.raddr, rport,
					frame->type, frame->seq, frame->ack, frame->len,
					sizeof(kdown)*8, kdown);
			}
			break;
		}

		free(frame);
	}
}

void
threadnetsend(void *arg)
{
	uchar buf[MTU];
	int fd, n;
	Frame *frame;

	threadsetname("threadnetsend");

	fd = *(int*)arg;

	while((frame = recvp(egress)) != nil){
		n = pack(buf, sizeof buf, "F", frame);
		free(frame);
		if(write(fd, buf, n) != n)
			sysfatal("write: %r");
	}
}

void
broadcaststate(void)
{
	int i;
	Frame *frame;
//	Player *player;
	Party *p;

	for(p = theparty.next; p != &theparty; p = p->next){
		frame = emalloc(sizeof(Frame)+2*(3*8+8)+3*8);
		pack(frame->data, frame->len, "PdPdP",
			p->u->ships[0].p, p->u->ships[0].θ,
			p->u->ships[1].p, p->u->ships[1].θ,
			p->u->star.p);

		for(i = 0; i < nelem(p->players); i++){
		}
	}

}

void
threadsim(void *)
{
	uvlong then, now;
	double frametime, Δt;
	Ioproc *io;
//	Player couple[2];
	Party *p;

	Δt = 0.01;
	then = nanosec();
	io = ioproc();

	for(;;){
//		if(lobby->getcouple(lobby, couple) != -1){
//			newparty(&theparty, couple);
//			theparty.prev->u->reset(theparty.prev->u);
//		}

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

//	fprint(fd, "curplayers	%lud\n"
//		   "totplayers	%lud\n"
//		   "maxplayers	%lud\n"
//		   "curparties	%lud\n"
//		   "totparties	%lud\n",
//		lobby->nseats, 0UL, lobby->cap,
//		nparties, 0UL);
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

	initparty(&theparty);

	ingress = chancreate(sizeof(Frame*), 32);
	egress = chancreate(sizeof(Frame*), 32);
	threadcreate(threadC2, nil, mainstacksize);
	threadcreate(threadnetrecv, &adfd, mainstacksize);
	threadcreate(threadnetppu, nil, mainstacksize);
	threadcreate(threadnetsend, &adfd, mainstacksize);
	threadcreate(threadsim, nil, mainstacksize);
	threadexits(nil);
}
