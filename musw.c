#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include "dat.h"
#include "fns.h"

enum {
	K↑,
	K←,
	K→,
	Kfire,
	Khyper,
	Kquit,
	NKEYS
};

Rune keys[NKEYS] = {
 [K↑]		Kup,
 [K←]		Kleft,
 [K→]		Kright,
 [Kfire]	' ',
 [Khyper]	'h',
 [Kquit]	'q'
};
ulong kup, kdown;

typedef struct Ball Ball;
struct Ball
{
	double x, v;
};

Ball bouncer;
int debug;


void
kbdproc(void *)
{
	Rune r, *k;
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
			for(k = keys; k < keys+NKEYS; k++)
				if(r == *k){
					kdown |= 1 << k-keys;
					break;
				}
		}
		kup = ~kdown;

		if(debug)
			fprint(2, "kup\t%lub\nkdown\t%lub\n", kup, kdown);
	}
}

void
threadnetrecv(void *arg)
{
	uchar buf[256];
	int fd, n;
	Ioproc *io;

	fd = *((int*)arg);
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof buf)) > 0){
		unpack(buf, n, "dd", &bouncer.x, &bouncer.v);

		if(debug)
			fprint(2, "bouncer [%g %g]\n", bouncer.x, bouncer.v);
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
	fillellipse(screen, addpt(screen->r.min,Pt(Dx(screen->r)/2,Dy(screen->r)/2+bouncer.x)), 2, 2, display->white, ZP);

	flushimage(display, 1);
	unlockdisplay(display);
}

void
resize(void)
{
	if(debug)
		fprint(2, "resizing\n");

	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);
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

	if(newwindow("-dx 640 -dy 480") < 0)
		sysfatal("newwindow: %r");
	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	display->locking = 1;
	unlockdisplay(display);

	proccreate(kbdproc, nil, 4096);

	fd = dial(server, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");

	threadcreate(threadnetrecv, &fd, 4096);
	threadcreate(threadresize, mc, 4096);

	io = ioproc();
	for(;;){
		redraw();
		iosleep(io, FPS2MS(30));
	}
}
