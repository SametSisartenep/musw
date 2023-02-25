#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>
#include <libsec.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

typedef struct Keymap Keymap;
struct Keymap
{
	Rune key;
	KeyOp op;
};

Keymap kmap[] = {
	{.key = Kup,	.op = K↑},
	{.key = Kleft,	.op = K↺},
	{.key = Kright,	.op = K↻},
	{.key = 'w',	.op = K↑},
	{.key = 'a',	.op = K↺},
	{.key = 'd',	.op = K↻},
	{.key = ' ',	.op = Kfire},
	{.key = 'h',	.op = Khyper},
	{.key = 'y',	.op = Ksay},
	{.key = 'q',	.op = Kquit}
};
ulong kdown;

RFrame screenrf;
Universe *universe;
VModel *needlemdl, *wedgemdl;
Image *screenb;
Image *skymap;
Channel *ingress;
Channel *egress;
NetConn netconn;
char winspec[32];
int doghosting;
int debug;


Point
toscreen(Point2 p)
{
	p = invrframexform(p, screenrf);
	return Pt(p.x,p.y);
}

Point2
fromscreen(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), screenrf);
}

/*
 * readvmodel and drawship are testing routines
 * that will later be implemented as VModel methods.
 */
VModel *
readvmodel(char *file)
{
	ulong lineno;
	char *s, *args[2];
	Biobuf *bin;
	VModel *mdl;

	bin = Bopen(file, OREAD);
	if(bin == nil)
		sysfatal("Bopen: %r");

	mdl = emalloc(sizeof(VModel));
	mdl->pts = nil;
	mdl->npts = 0;
	mdl->strokefmt = nil;

	lineno = 0;
	while(s = Brdline(bin, '\n')){
		s[Blinelen(bin)-1] = 0;
		lineno++;

		switch(*s++){
		case '#':
			continue;
		case 'v':
			if(tokenize(s, args, nelem(args)) != nelem(args)){
				werrstr("syntax error: %s:%ld 'v' expects %d args",
					file, lineno, nelem(args));
				free(mdl);
				Bterm(bin);
				return nil;
			}
			mdl->pts = erealloc(mdl->pts, ++mdl->npts*sizeof(Point2));
			mdl->pts[mdl->npts-1].x = strtod(args[0], nil);
			mdl->pts[mdl->npts-1].y = strtod(args[1], nil);
			mdl->pts[mdl->npts-1].w = 1;
			break;
		case 'l':
		case 'c':
			mdl->strokefmt = strdup(s-1);
			break;
		}
	}
	Bterm(bin);

	return mdl;
}

int
blendimages(Image *i0, Image *i1, double f)
{
	static uchar *buf0, *buf1;
	static char c0[10], c1[10];
	static ulong n;
	uchar *s0, *s1;

	assert(i0->depth == i1->depth);
	assert(Dx(i0->r) == Dx(i1->r) && Dy(i0->r) == Dy(i1->r));

	f = fclamp(f, 0, 1);
	if(buf0 == nil && buf1 == nil){
		n = bytesperline(i0->r, i0->depth)*Dy(i0->r);
		buf0 = emalloc(n);
		buf1 = emalloc(n);

		/* i1 won't ever change */
		if(unloadimage(i1, i1->r, buf1, n) != n)
			sysfatal("unloadimage: %r");

		if(debug){
			chantostr(c0, i0->chan);
			chantostr(c1, i1->chan);
			fprint(2, "i0 %s\ti1 %s\n", c0, c1);
		}
	}

	if(unloadimage(i0, i0->r, buf0, n) != n)
		sysfatal("unloadimage: %r");

	for(s0 = buf0, s1 = buf1; s0 < buf0+n; s0++, s1++)
		*s0 = flerp(*s0, *s1, f);

	if(loadimage(i0, i0->r, buf0, n) != n)
		sysfatal("loadimage: %r");

	return 0;
}

void
drawbullets(Ship *ship, Image *dst)
{
	int i;
	Bullet *b;
	Point2 v;

	for(i = 0; i < nelem(ship->rounds); i++){
		b = &ship->rounds[i];
		v = Vec2(-1,0); /* it's pointing backwards to paint the tail */
		Matrix R = {
			cos(b->θ), -sin(b->θ), 0,
			sin(b->θ),  cos(b->θ), 0,
			0, 0, 1,
		};

		v = xform(v, R);
		line(dst, toscreen(b->p), toscreen(addpt2(b->p, mulpt2(v, 10))), 0, 0, 0, display->white, ZP);
		fillellipse(dst, toscreen(b->p), 1, 1, display->white, ZP);
	}
}

void
drawship(Ship *ship, Image *dst)
{
	int i;
	char *s;
	Point pts[3];
	VModel *mdl;
	Point2 *p;
	Matrix T = {
		1, 0, ship->p.x,
		0, 1, ship->p.y,
		0, 0, 1,
	}, R = {
		cos(ship->θ), -sin(ship->θ), 0,
		sin(ship->θ),  cos(ship->θ), 0,
		0, 0, 1,
	};

	mulm(T, R);
	mdl = ship->mdl;
	p = mdl->pts;
	for(s = mdl->strokefmt; s != nil && p-mdl->pts < mdl->npts; s++)
		switch(*s){
		case 'l':
			line(dst, toscreen(xform(p[0], T)), toscreen(xform(p[1], T)), 0, 0, 0, display->white, ZP);
			p += 2;
			break;
		case 'c':
			for(i = 0; i < nelem(pts); i++)
				pts[i] = toscreen(xform(p[i], T));
			bezspline(dst, pts, nelem(pts), 0, 0, 0, display->white, ZP);
			p += 3;
			break;
		}

	drawbullets(ship, dst);
}

void
initconn(void)
{
	Frame *frame;

	frame = newframe(nil, NChi, ntruerand(1000), 0, 0, nil);
	sendp(egress, frame);
	netconn.state = NCSConnecting;
}

void
sendkeys(ulong kdown)
{
	Frame *frame;

	if(netconn.state != NCSConnected)
		return;

	frame = newframe(nil, NCinput, netconn.lastseq+1, 0, sizeof(kdown), nil);
	pack(frame->data, frame->len, "k", kdown);
	signframe(frame, netconn.dh.priv);
	sendp(egress, frame);
}

void
kbdproc(void *)
{
	Rune r;
	Keymap *k;
	char buf[128], *s;
	int fd, n;

	threadsetname("kbdproc");

	if((fd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("kbdproc: %r");

	memset(buf, 0, sizeof buf);

	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			if((n = read(fd, buf, sizeof(buf)-1)) <= 0)
				break;
			buf[n-1] = 0;
			buf[n] = 0;
		}
		if(buf[0] == 'c'){
			if(utfrune(buf, Kdel)){
				close(fd);
				threadexitsall(nil);
			}
		}
		if(buf[0] != 'k' && buf[0] != 'K')
			continue;
		s = buf+1;
		kdown = 0;
		while(*s){
			s += chartorune(&r, s);
			for(k = kmap; k < kmap+nelem(kmap); k++)
				if(r == k->key){
					kdown |= 1 << k->op;
					break;
				}
		}

		if(debug)
			fprint(2, "kdown %.*lub\n", sizeof(kdown)*8, kdown);

		sendkeys(kdown);
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
		frame = newframe(nil, 0, 0, 0, n-Framehdrsize, nil);
		unpack(buf, n, "f", frame);
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
	int i, j;
	uchar *bufp;
	Frame *frame, *newf;

	threadsetname("threadnetppu");

	while((frame = recvp(ingress)) != nil){
		if(frame->id != ProtocolID)
			goto discard;

		switch(netconn.state){
		case NCSConnecting:
			if(frame->seq != netconn.lastseq + 1 &&
			   frame->ack != netconn.lastseq)
				goto discard;

			switch(frame->type){
			case NShi:
				unpack(frame->data, frame->len, "kk", &netconn.dh.p, &netconn.dh.g);

				newf = newframe(nil, NCdhx, frame->seq+1, frame->seq, sizeof(ulong), nil);

				netconn.dh.sec = truerand();
				pack(newf->data, newf->len, "k", dhgenkey(netconn.dh.g, netconn.dh.sec, netconn.dh.p));
				sendp(egress, newf);

				if(debug)
					fprint(2, "\tsent pubkey %ld\n", dhgenkey(netconn.dh.g, netconn.dh.sec, netconn.dh.p));

				break;
			case NSdhx:
				unpack(frame->data, frame->len, "k", &netconn.dh.pub);
				netconn.state = NCSConnected;

				if(debug)
					fprint(2, "\trcvd pubkey %ld\n", netconn.dh.pub);

				netconn.dh.priv = dhgenkey(netconn.dh.pub, netconn.dh.sec, netconn.dh.p);
				break;
			}
			break;
		case NCSConnected:
			if(!verifyframe(frame, netconn.dh.priv)){
				if(debug)
					fprint(2, "\tbad signature\n");
				goto discard;
			}

			switch(frame->type){
			case NSsimstate:
				bufp = frame->data;
				bufp += unpack(bufp, frame->len, "PdPdP",
					&universe->ships[0].p, &universe->ships[0].θ,
					&universe->ships[1].p, &universe->ships[1].θ,
					&universe->star.p);

					/* TODO: only recv the fired ones */
					for(i = 0; i < nelem(universe->ships); i++)
						for(j = 0; j < nelem(universe->ships[i].rounds); j++)
							bufp += unpack(bufp, frame->len - (bufp-frame->data), "Pd",
								&universe->ships[i].rounds[j].p, &universe->ships[i].rounds[j].θ);
				break;
			case NSnudge:
				newf = newframe(nil, NCnudge, frame->seq+1, frame->seq, 0, nil);
				signframe(newf, netconn.dh.priv);

				sendp(egress, newf);

				break;
			case NSbuhbye:
				netconn.state = NCSDisconnected;
				break;
			}
			break;
		}

		netconn.lastseq = frame->seq;
		netconn.lastack = frame->ack;
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

	threadsetname("threadnetsend");

	fd = *(int*)arg;

	while((frame = recvp(egress)) != nil){
		n = pack(buf, sizeof buf, "f", frame);
		if(write(fd, buf, n) != n)
			sysfatal("write: %r");

		if(debug){
			rport = frame->udp.rport[0]<<8 | frame->udp.rport[1];
			lport = frame->udp.lport[0]<<8 | frame->udp.lport[1];
			fprint(2, "%I!%ud → %I!%ud | sent %Φ\n",
				frame->udp.laddr, lport, frame->udp.raddr, rport, frame);
		}

		netconn.lastseq = frame->seq;
		netconn.lastack = frame->ack;

		delframe(frame);
	}
}

void resize(void);

void
threadresize(void *arg)
{
	Mousectl *mc;
	Alt a[3];

	mc = arg;
	a[0].op = CHANRCV; a[0].c = mc->c; a[0].v = &mc->Mouse;
	a[1].op = CHANRCV; a[1].c = mc->resizec; a[1].v = nil;
	a[2].op = CHANEND;

	for(;;)
		if(alt(a) == 1)
			resize();
}

void
initskymap(void)
{
	int fd;

	fd = open("assets/bg/defskymap.pic", OREAD);
	if(fd < 0)
		goto darkness;

	skymap = readimage(display, fd, 1);
	if(skymap == nil){
darkness:
		fprint(2, "couldn't read the sky map. falling back to darkness...\n");
		skymap = display->black;
	}
	close(fd);
}

void
drawconnecting(void)
{
	static double t0;
	static Point p = {100,300};
	Point np;
	int i;

	if(t0 == 0)
		t0 = nanosec();

	if(nanosec()-t0 >= 5e9){ /* every five seconds */
		p = Pt(ntruerand(SCRW-2*100)+100,ntruerand(SCRH-100)+100);
		t0 = nanosec();
	}

	np = string(screenb, addpt(screenb->r.min, p), display->white, ZP, font, "connecting");

	for(i = 1; i < 3+1; i++){
		if(nanosec()-t0 > i*1e9)
			np = string(screenb, np, display->white, ZP, font, ".");
	}
}

void
redraw(void)
{
	lockdisplay(display);

	if(doghosting)
		blendimages(screenb, skymap, 0.05);
	else
		draw(screenb, screenb->r, skymap, nil, ZP);

	if(netconn.state == NCSConnecting)
		drawconnecting();

	if(netconn.state == NCSConnected){
		drawship(&universe->ships[0], screenb);
		drawship(&universe->ships[1], screenb);
		universe->star.spr->draw(universe->star.spr, screenb, subpt(toscreen(universe->star.p), Pt(16,16)));
	}

	draw(screen, screen->r, screenb, nil, ZP);

	flushimage(display, 1);
	unlockdisplay(display);
}

void
resize(void)
{
	int fd;

	if(debug)
		fprint(2, "resizing\n");

	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);

//	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);

	/* ignore move events */
	if(Dx(screen->r) != SCRW || Dy(screen->r) != SCRH){
		fd = open("/dev/wctl", OWRITE);
		if(fd >= 0){
			fprint(fd, "resize %s", winspec);
			close(fd);
		}
	}

	redraw();
}

void
usage(void)
{
	fprint(2, "usage: %s [-dg] server\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	uvlong then, now;
	double frametime;
	char *server;
	int fd;
	Mousectl *mc;
	Ioproc *io;

	GEOMfmtinstall();
	fmtinstall('I', eipfmt);
	fmtinstall(L'Φ', Φfmt);
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'g':
		doghosting++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc != 1)
		usage();
	server = argv[0];

	snprint(winspec, sizeof winspec, "-dx %d -dy %d", SCRWB, SCRHB);
	if(newwindow(winspec) < 0)
		sysfatal("newwindow: %r");
	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	display->locking = 1;
	unlockdisplay(display);

	initskymap();
	screenb = eallocimage(display, rectsubpt(screen->r, screen->r.min), RGBA32, 0, DNofill);
	draw(screenb, screenb->r, skymap, nil, ZP);

	screenrf.p = Pt2(Dx(screenb->r)/2,Dy(screenb->r)/2,1);
	screenrf.bx = Vec2(1, 0);
	screenrf.by = Vec2(0,-1);

	proccreate(kbdproc, nil, mainstacksize);

	/* TODO: implement this properly with screens and iodial(2) */
	fd = dial(server, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");

	universe = newuniverse();
	needlemdl = readvmodel("assets/mdl/needle.vmdl");
	if(needlemdl == nil)
		sysfatal("readvmodel: %r");
	wedgemdl = readvmodel("assets/mdl/wedge.vmdl");
	if(wedgemdl == nil)
		sysfatal("readvmodel: %r");
	universe->ships[0].mdl = needlemdl;
	universe->ships[1].mdl = wedgemdl;
	universe->star.spr = readsprite("assets/spr/earth.pic", ZP, Rect(0,0,32,32), 5, 20e3);

	ingress = chancreate(sizeof(Frame*), 8);
	egress = chancreate(sizeof(Frame*), 8);
	threadcreate(threadnetrecv, &fd, mainstacksize);
	threadcreate(threadnetppu, nil, mainstacksize);
	threadcreate(threadnetsend, &fd, mainstacksize);
	threadcreate(threadresize, mc, mainstacksize);

	then = nanosec();
	io = ioproc();
	for(;;){
		now = nanosec();
		frametime = now - then;
		then = now;

		universe->star.spr->step(universe->star.spr, frametime/1e6);

		redraw();

		if(netconn.state == NCSDisconnected)
			initconn();

		iosleep(io, HZ2MS(30));
	}
}
