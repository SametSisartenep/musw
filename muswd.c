#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>
#include <libsec.h>
#include <thread.h>
#include <draw.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

int debug;
int mainstacksize = 24*1024;

Party theparty;
Playerq players;
NetConn **conns;
usize nconns;
usize maxconns;
Channel *ingress;
Channel *egress;


void
putconn(NetConn *nc)
{
	if(++nconns > maxconns){
		conns = erealloc(conns, sizeof(NetConn*)*nconns);
		maxconns = nconns;
	}
	conns[nconns-1] = nc;
}

NetConn *
getconn(Frame *f)
{
	NetConn **nc;

	for(nc = conns; nc < conns+nconns; nc++)
		if(memcmp(&(*nc)->udp, &f->udp, Udphdrsize) == 0)
			return *nc;
	return nil;
}

/* TODO: this is an ugly hack. nc should probably reference the player back */
void
dissolveparty(NetConn *nc)
{
	int i;
	Party *p;
	Player *player;

	for(p = theparty.next; p != &theparty; p = p->next)
		for(i = 0; i < nelem(p->players); i++)
			if(p->players[i]->conn == nc){
				delplayer(p->players[i]);
				players.put(&players, p->players[i^1]);
				delparty(p);
			}

	/* also clean the player queue */
	for(player = players.head; player != nil; player = player->next)
		if(player->conn == nc)
			players.del(&players, player);
}

int
popconn(NetConn *nc)
{
	NetConn **ncp, **ncpe;

	ncpe = conns+nconns;

	for(ncp = conns; ncp < ncpe; ncp++)
		if(*ncp == nc){
			memmove(ncp, ncp+1, sizeof(NetConn*)*(ncpe-ncp-1));
			nconns--;
			dissolveparty(nc); /* TODO: ugly hack. */
			delnetconn(nc);
			return 0;
		}
	return -1;
}

void
nudgeconns(ulong curts)
{
	NetConn **ncp, **ncpe;
	Frame *f;
	ulong elapsed, elapsednudge;

	ncpe = conns+nconns;

	for(ncp = conns; ncp < ncpe; ncp++){
		elapsed = curts - (*ncp)->lastrecvts;
		elapsednudge = curts - (*ncp)->lastnudgets;

		if((*ncp)->state == NCSConnected && elapsed > ConnTimeout)
			popconn(*ncp);
		else if((*ncp)->state == NCSConnected && elapsednudge > 1000){ /* every second */
			f = newframe(&(*ncp)->udp, NSnudge, (*ncp)->lastseq+1, 0, 0, nil);
			signframe(f, (*ncp)->dh.priv);
			sendp(egress, f);

			(*ncp)->lastnudgets = curts;
		}
	}
}

void
threadnetrecv(void *arg)
{
	uchar buf[MTU];
	int fd, n;
	ushort rport, lport;
	Ioproc *io;
	Frame *frame;

	threadsetname("threadnetrecv");

	fd = *(int*)arg;
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof buf)) > 0){
		frame = newframe(nil, 0, 0, 0, n-Udphdrsize-Framehdrsize, nil);
		unpack(buf, n, "F", frame);
		sendp(ingress, frame);

		if(debug){
			rport = frame->udp.rport[0]<<8 | frame->udp.rport[1];
			lport = frame->udp.lport[0]<<8 | frame->udp.lport[1];
			fprint(2, "%I!%ud → %I!%ud | rcvd %Φ\n",
				frame->udp.laddr, lport, frame->udp.raddr, rport, frame);
		}
	}
	closeioproc(io);
}

void
threadnetppu(void *)
{
	ulong kdown;
	Frame *frame, *newf;
	NetConn *nc;

	threadsetname("threadnetppu");

	while((frame = recvp(ingress)) != nil){
		if(frame->id != ProtocolID)
			goto discard;

		nc = getconn(frame);
		if(nc == nil){
			if(frame->type == NChi){
				nc = newnetconn(NCSConnecting, &frame->udp);
				putconn(nc);

				newf = newframe(&frame->udp, NShi, frame->seq+1, frame->seq, 2*sizeof(ulong), nil);

				dhgenpg(&nc->dh.p, &nc->dh.g);
				pack(newf->data, newf->len, "kk", nc->dh.p, nc->dh.g);
				sendp(egress, newf);

				if(debug)
					fprint(2, "\tsent p %ld g %ld\n", nc->dh.p, nc->dh.g);
			}else
				goto discard;
		}

		switch(nc->state){
		case NCSConnecting:
			if(frame->seq != nc->lastseq + 1 &&
			   frame->ack != nc->lastseq)
				goto discard;

			switch(frame->type){
			case NCdhx:
				unpack(frame->data, frame->len, "k", &nc->dh.pub);
				nc->state = NCSConnected;

				players.put(&players, newplayer(nil, nc));

				if(debug)
					fprint(2, "\trcvd pubkey %ld\n", nc->dh.pub);

				newf = newframe(&frame->udp, NSdhx, frame->seq+1, frame->seq, sizeof(ulong), nil);

				nc->dh.sec = truerand();
				nc->dh.priv = dhgenkey(nc->dh.pub, nc->dh.sec, nc->dh.p);
				pack(newf->data, newf->len, "k", dhgenkey(nc->dh.g, nc->dh.sec, nc->dh.p));
				sendp(egress, newf);

				if(debug)
					fprint(2, "\tsent pubkey %ld\n", dhgenkey(nc->dh.g, nc->dh.sec, nc->dh.p));

				break;
			}
			break;
		case NCSConnected:
			if(!verifyframe(frame, nc->dh.priv)){
				if(debug)
					fprint(2, "\tbad signature\n");
				goto discard;
			}

			switch(frame->type){
			case NCinput:
				unpack(frame->data, frame->len, "k", &kdown);

				if(debug)
					fprint(2, "\t%.*lub\n", sizeof(kdown)*8, kdown);

				break;
			case NCbuhbye:
				popconn(nc);
				break;
			}
			break;
		}

		nc->lastrecvts = nanosec()/1e6;
		nc->lastseq = frame->seq;
		nc->lastack = frame->ack;
discard:
		delframe(frame);
	}
}

void
threadnetsend(void *arg)
{
	uchar buf[MTU];
	int fd, n;
	ushort rport, lport;
	Frame *frame;
	NetConn *nc;

	threadsetname("threadnetsend");

	fd = *(int*)arg;

	while((frame = recvp(egress)) != nil){
		n = pack(buf, sizeof buf, "F", frame);
		if(write(fd, buf, n) != n)
			sysfatal("write: %r");

		if(debug){
			rport = frame->udp.rport[0]<<8 | frame->udp.rport[1];
			lport = frame->udp.lport[0]<<8 | frame->udp.lport[1];
			fprint(2, "%I!%ud → %I!%ud | sent %Φ\n",
				frame->udp.laddr, lport, frame->udp.raddr, rport, frame);
		}

		nc = getconn(frame);
		if(nc != nil){
			nc->lastseq = frame->seq;
			nc->lastack = frame->ack;
		}

		delframe(frame);
	}
}

void
broadcaststate(void)
{
	int i;
	Frame *frame;
	NetConn *pnc;
	Party *p;

	for(p = theparty.next; p != &theparty; p = p->next)
		for(i = 0; i < nelem(p->players); i++){
			pnc = p->players[i]->conn;

			frame = newframe(&pnc->udp, NSsimstate, pnc->lastseq+1, 0, 2*(3*8+8)+3*8, nil);
			pack(frame->data, frame->len, "PdPdP",
				p->u->ships[0].p, p->u->ships[0].θ,
				p->u->ships[1].p, p->u->ships[1].θ,
				p->u->star.p);
			signframe(frame, pnc->dh.priv);

			sendp(egress, frame);
		}

}

void
threadsim(void *)
{
	uvlong then, now;
	double frametime, Δt;
	Ioproc *io;
	Party *p;

	Δt = 0.01;
	then = nanosec();
	io = ioproc();

	for(;;){
		if(players.len >= 2)
			newparty(&theparty, players.get(&players), players.get(&players));

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
		nudgeconns(now/1e6);

		iosleep(io, HZ2MS(70));
	}
}

void
fprintstats(int fd)
{
	usize nparties = 0;
	Party *p;

	for(p = theparty.next; p != &theparty; p = p->next)
		nparties++;

	fprint(fd, "curconns	%lld\n"
		   "maxconns	%lld\n"
		   "nplayers	%lld\n"
		   "nparties	%lld\n",
		nconns, maxconns, players.len, nparties);
}

void
fprintstates(int fd)
{
	ulong i = 0;
	Party *p;
	Ship *s;

	for(p = theparty.next; p != &theparty; p = p->next, i++){
		for(s = &p->u->ships[0]; s-p->u->ships < nelem(p->u->ships); s++){
			fprint(fd, "%ld s%lld k%d p %v v %v θ %g ω %g m %g f %d\n",
				i, s-p->u->ships, s->kind, s->p, s->v, s->θ, s->ω, s->mass, s->fuel);
		}
		fprint(fd, "%ld S p %v m %g\n", i, p->u->star.p, p->u->star.mass);
	}
}


/*
 * Command & Control
 *
 *	- show stats: prints some server stats
 *	- show states: prints the state of running simulations
 *	- debug [on|off]: toggles debug mode
 */
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
			}else if(strcmp(cmdargs[0], "debug") == 0){
				if(strcmp(cmdargs[1], "on") == 0)
					debug = 1;
				else if(strcmp(cmdargs[1], "off") == 0)
					debug = 0;
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
	fmtinstall(L'Φ', Φfmt);
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
	initplayerq(&players);

	ingress = chancreate(sizeof(Frame*), 32);
	egress = chancreate(sizeof(Frame*), 32);
	threadcreate(threadC2, nil, mainstacksize);
	threadcreate(threadnetrecv, &adfd, mainstacksize);
	threadcreate(threadnetppu, nil, mainstacksize);
	threadcreate(threadnetsend, &adfd, mainstacksize);
	threadcreate(threadsim, nil, mainstacksize);
	threadexits(nil);
}
