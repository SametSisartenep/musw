#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include "libgeometry/geometry.h"
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

typedef struct Ball Ball;
struct Ball
{
	Point2 p, v;
};

RFrame screenrf;
Ball bouncer;
Universe *universe;
VModel *needlemdl;
char winspec[32];
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

	mdl = emalloc(sizeof *mdl);
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
				werrstr("syntax error: %s:%lud 'v' expects %d args",
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
		0, 0, 1
	}, R = {
		cos(ship->θ), -sin(ship->θ), 0,
		sin(ship->θ), cos(ship->θ), 0,
		0, 0, 1
	};

	mulm(T, R);
	mdl = ship->mdl;
	p = mdl->pts;
	for(s = mdl->strokefmt; s != 0 && p-mdl->pts < mdl->npts; s++)
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
			fprint(2, "kdown %.*lub\n",
				sizeof(kdown)*8, kdown);
	}
}

void
threadnetrecv(void *arg)
{
	uchar buf[1024];
	int fd, n;
	Ioproc *io;

	fd = *((int*)arg);
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof buf)) > 0){
		unpack(buf, n, "PPdPdP", &bouncer.p,
			&universe->ships[0].p, &universe->ships[0].θ,
			&universe->ships[1].p, &universe->ships[1].θ,
			&universe->star.p);
	}
	closeioproc(io);
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
redraw(void)
{
	lockdisplay(display);

	draw(screen, screen->r, display->black, nil, ZP);
	fillellipse(screen, toscreen(bouncer.p), 2, 2, display->white, ZP);

	drawship(&universe->ships[0], screen);
	drawship(&universe->ships[1], screen);
	fillellipse(screen, toscreen(universe->star.p), 4, 4, display->white, ZP);

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

	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);

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
	fprint(2, "usage: %s [-d] server\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *server;
	int fd;
	Mousectl *mc;
	Ioproc *io;

	GEOMfmtinstall();
	ARGBEGIN{
	case 'd':
		debug++;
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

	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	screenrf.bx = Vec2(1, 0);
	screenrf.by = Vec2(0,-1);

	proccreate(kbdproc, nil, 4096);

	fd = dial(server, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");

	universe = newuniverse();
	needlemdl = readvmodel("assets/mdl/needle.vmdl");
	if(needlemdl == nil)
		sysfatal("readvmodel: %r");
	universe->ships[0].mdl = needlemdl;
	universe->ships[1].mdl = needlemdl;

	threadcreate(threadnetrecv, &fd, 4096);
	threadcreate(threadresize, mc, 4096);

	io = ioproc();
	for(;;){
		redraw();
		iosleep(io, HZ2MS(30));
	}
}
