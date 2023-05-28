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
#include "cmixer.h"

enum {
	GSIntro,
	GSConnecting,
	GSMatching,
	GSPlaying,
	NGAMESTATES
};

enum {
	VFX_BULLET_EXPLOSION,
	NVFX
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
Universe *universe, *olduniverse;
VModel *needlemdl, *wedgemdl;
Image *screenb;
Image *skymap;
Sprite *intro;
Sprite *vfxtab[NVFX];
Vfx vfxqueue;
State gamestates[NGAMESTATES];
State *gamestate;
Channel *ingress;
Channel *egress;
NetConn netconn;
char deffont[] = "/lib/font/bit/pelm/unicode.9.font";
char winspec[32];
int weplaying;
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

void
swapuniverses(void)
{
	Universe *u;

	u = universe;
	universe = olduniverse;
	olduniverse = u;
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

		if(!b->fired)
			continue;

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
buhbye(void)
{
	Frame *frame;
	int i, naptime;

	if(netconn.state != NCSConnected)
		return;

	i = 10;
	naptime = 2000/i;
	while(i--){
		frame = newframe(nil, NCbuhbye, netconn.lastseq+1, 0, 0, nil);
		signframe(frame, netconn.dh.priv);
		sendp(egress, frame);

		sleep(naptime);
	}
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
defenestrate:
				close(fd);
				threadexitsall(nil);
			}else if(utfrune(buf, 'q')){
				buhbye();
				goto defenestrate;
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
			fprint(2, "%I!%ud ← %I!%ud | rcvd %Φ\n",
				frame->udp.laddr, lport, frame->udp.raddr, rport, frame);
		}
	}
	closeioproc(io);
}

void
threadnetppu(void *)
{
	int i, j;
	int nfired[2], bi;
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
				weplaying = 1;

				swapuniverses();

				bufp = frame->data;
				bufp += unpack(bufp, frame->len, "PdPdP",
					&universe->ships[0].p, &universe->ships[0].θ,
					&universe->ships[1].p, &universe->ships[1].θ,
					&universe->star.p);

				bufp += unpack(bufp, frame->len - (bufp-frame->data), "bb", &nfired[0], &nfired[1]);

				if(debug)
					fprint(2, "nfired0 %d nfired1 %d\n", nfired[0], nfired[1]);

				for(i = 0; i < nelem(universe->ships); i++)
					for(j = 0; j < nelem(universe->ships[i].rounds); j++)
						universe->ships[i].rounds[j].fired = 0;

				for(i = 0; i < nelem(universe->ships); i++)
					for(j = 0; j < nfired[i]; j++){
						bufp += unpack(bufp, frame->len - (bufp-frame->data), "b",
							&bi);
						if(debug)
							fprint(2, "si %d bi %d\n", i, bi);
						bufp += unpack(bufp, frame->len - (bufp-frame->data), "Pd",
							&universe->ships[i].rounds[bi].p, &universe->ships[i].rounds[bi].θ);
						universe->ships[i].rounds[bi].fired++;
					}

				for(i = 0; i < nelem(universe->ships); i++)
					for(j = 0; j < nelem(universe->ships[i].rounds); j++)
						if(!universe->ships[i].rounds[j].fired && olduniverse->ships[i].rounds[j].fired)
							addvfx(&vfxqueue, newvfx(vfxtab[VFX_BULLET_EXPLOSION]->clone(vfxtab[VFX_BULLET_EXPLOSION]), toscreen(universe->ships[i].rounds[j].p), 1));
				break;
			case NSnudge:
				newf = newframe(nil, NCnudge, frame->seq+1, frame->seq, 0, nil);
				signframe(newf, netconn.dh.priv);

				sendp(egress, newf);

				break;
			case NSawol:
				weplaying = 0;

				newf = newframe(nil, NCawol, frame->seq+1, frame->seq, 0, nil);
				signframe(newf, netconn.dh.priv);

				sendp(egress, newf);

				break;
			case NSbuhbye:
				weplaying = 0;
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
drawprogressing(char *s)
{
	static double t0;
	static Point p = {100,300};
	Point np;
	int i;

	if(t0 == 0)
		t0 = nanosec();

	if(nanosec()-t0 >= 5e9){ /* every five seconds */
		p = Pt(ntruerand(SCRW-stringwidth(font, s)-3*font->width),ntruerand(SCRH-font->height));
		t0 = nanosec();
	}

	np = string(screenb, addpt(screenb->r.min, p), display->white, ZP, font, s);

	for(i = 1; i < 3+1; i++){
		if(nanosec()-t0 > i*1e9)
			np = string(screenb, np, display->white, ZP, font, ".");
	}
}

void
redraw(void)
{
	Vfx *vfx;

	lockdisplay(display);

	if(doghosting)
		blendimages(screenb, skymap, 0.05);
	else
		draw(screenb, screenb->r, skymap, nil, ZP);

	switch(gamestate-gamestates){
	case GSIntro:
		intro->draw(intro, screenb, subpt(divpt(screenb->r.max, 2), divpt(intro->r.max, 2)));
		break;
	case GSConnecting:
		drawprogressing("connecting");
		break;
	case GSMatching:
		drawprogressing("waiting for players");
		break;
	case GSPlaying:
		drawship(&universe->ships[0], screenb);
		drawship(&universe->ships[1], screenb);
		universe->star.spr->draw(universe->star.spr, screenb, subpt(toscreen(universe->star.p), divpt(universe->star.spr->r.max, 2)));
		break;
	}

	for(vfx = vfxqueue.next; vfx != &vfxqueue; vfx = vfx->next)
		vfx->draw(vfx, screenb);

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

State *intro_δ(State *s, void *arg)
{
	static ulong elapsed;
	uvlong ∆t;

	∆t = *(uvlong*)arg;
	elapsed += ∆t;
	if(elapsed > 5000)
		return &gamestates[GSConnecting];
	return s;
}

State *connecting_δ(State *s, void*)
{
	if(netconn.state != NCSConnecting)
		return &gamestates[GSMatching];
	return s;
}

State *matching_δ(State *s, void*)
{
	if(netconn.state == NCSConnected && weplaying)
		return &gamestates[GSPlaying];
	return s;
}

State *playing_δ(State *s, void*)
{
	if(!weplaying)
		return &gamestates[GSMatching];
	return s;
}

void
soundproc(void *)
{
	Biobuf *aout;
	uchar adata[512];

	threadsetname("soundproc");

	aout = Bopen("/dev/audio", OWRITE);
	if(aout == nil)
		sysfatal("Bopen: %r");

	for(;;){
		cm_process((void *)adata, sizeof(adata)/2);
		Bwrite(aout, adata, sizeof adata);
	}
}

void
threadshow(void *)
{
	uvlong then, now, frametime, lastpktsent;
	Vfx *vfx;
	Ioproc *io;

	then = nanosec();
	lastpktsent = 0;
	io = ioproc();
	for(;;){
		now = nanosec();
		frametime = (now - then)/1000000ULL;
		then = now;

		switch(gamestate-gamestates){
		case GSPlaying:
			universe->star.spr->step(universe->star.spr, frametime);
			for(vfx = vfxqueue.next; vfx != &vfxqueue; vfx = vfx->next)
				vfx->step(vfx, frametime);
			/* fallthrough */
		default:
			if(netconn.state == NCSConnecting)
				lastpktsent += frametime;

			if(netconn.state == NCSDisconnected ||
			  (netconn.state == NCSConnecting && lastpktsent >= 1000)){
				initconn();
				lastpktsent = 0;
			}
			break;
		case GSIntro:
			intro->step(intro, frametime);
			break;
		}
		gamestate = gamestate->δ(gamestate, &frametime);

		redraw();

		iosleep(io, HZ2MS(30));
	}
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
	char *server;
	int fd;
	cm_Source *bgsound;
	Mousectl *mc;

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
	if(initdraw(nil, deffont, "musw") < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	cm_init(44100);
	cm_set_master_gain(0.5);

	display->locking = 1;
	unlockdisplay(display);

	initskymap();
	screenb = eallocimage(display, rectsubpt(screen->r, screen->r.min), RGBA32, 0, DNofill);
	draw(screenb, screenb->r, skymap, nil, ZP);

	screenrf.p = Pt2(Dx(screenb->r)/2,Dy(screenb->r)/2,1);
	screenrf.bx = Vec2(1, 0);
	screenrf.by = Vec2(0,-1);

	proccreate(kbdproc, nil, mainstacksize);

	fd = dial(server, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");

	universe = newuniverse();
	olduniverse = newuniverse();
	needlemdl = readvmodel("assets/mdl/needle.vmdl");
	if(needlemdl == nil)
		sysfatal("readvmodel: %r");
	wedgemdl = readvmodel("assets/mdl/wedge.vmdl");
	if(wedgemdl == nil)
		sysfatal("readvmodel: %r");
	olduniverse->ships[0].mdl = universe->ships[0].mdl = needlemdl;
	olduniverse->ships[1].mdl = universe->ships[1].mdl = wedgemdl;
	olduniverse->star.spr = universe->star.spr = readpngsprite("assets/spr/pulsar.png", ZP, Rect(0,0,64,64), 9, 50);

	intro = readpngsprite("assets/spr/intro.png", ZP, Rect(0,0,640,480), 28, 100);

	vfxtab[VFX_BULLET_EXPLOSION] = readpngsprite("assets/vfx/bullet.explosion.png", ZP, Rect(0, 0, 32, 32), 12, 100);
	initvfx(&vfxqueue);

	bgsound = cm_new_source_from_file("assets/sfx/intro.wav");
	if(bgsound == nil)
		sysfatal("cm_new_source_from_file: %s", cm_get_error());
	cm_play(bgsound);

	proccreate(soundproc, nil, mainstacksize);

	gamestates[GSIntro].δ = intro_δ;
	gamestates[GSConnecting].δ = connecting_δ;
	gamestates[GSMatching].δ = matching_δ;
	gamestates[GSPlaying].δ = playing_δ;
	gamestate = &gamestates[GSIntro];

	ingress = chancreate(sizeof(Frame*), 8);
	egress = chancreate(sizeof(Frame*), 8);
	threadcreate(threadnetrecv, &fd, mainstacksize);
	threadcreate(threadnetppu, nil, mainstacksize);
	threadcreate(threadnetsend, &fd, mainstacksize);
	threadcreate(threadresize, mc, mainstacksize);
	threadcreate(threadshow, nil, mainstacksize);
	yield();
}
