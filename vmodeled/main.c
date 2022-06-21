#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include "../libgeometry/geometry.h"

/*
 * Vector model - made out of lines and curves
 */
typedef struct VModel VModel;
struct VModel
{
	Point2 *pts;
	ulong npts;
	/* WIP
	 * l(ine) → takes 2 points
	 * c(urve) → takes 3 points
	 */
	char *strokefmt;
};

RFrame worldrf;
VModel *model;
double scale = 1;

void resized(void);

void*
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("malloc: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void*
erealloc(void *p, ulong n)
{
	void *np;

	np = realloc(p, n);
	if(np == nil){
		if(n == 0)
			return nil;
		sysfatal("realloc: %r");
	}
	if(p == nil)
		setmalloctag(np, getcallerpc(&p));
	else
		setrealloctag(np, getcallerpc(&p));
	return np;
}

Point
toscreen(Point2 p)
{
	p = invrframexform(p, worldrf);
	return Pt(p.x,p.y);
}

Point2
fromscreen(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), worldrf);
}

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
drawvmodel(Image *dst, VModel *mdl)
{
	int i;
	char *s;
	Point pts[3];
	Point2 *p;
	Matrix S = {
		scale, 0, 1,
		0, scale, 1,
		0, 0, 1,
	};

	p = mdl->pts;
	for(s = mdl->strokefmt; s != 0 && p-mdl->pts < mdl->npts; s++)
		switch(*s){
		case 'l':
			line(dst, toscreen(xform(p[0], S)), toscreen(xform(p[1], S)), 0, 0, 0, display->white, ZP);
			p += 2;
			break;
		case 'c':
			for(i = 0; i < nelem(pts); i++)
				pts[i] = toscreen(xform(p[i], S));
			bezspline(dst, pts, nelem(pts), 0, 0, 0, display->white, ZP);
			p += 3;
			break;
		}
}

void
drawaxes(void)
{
	line(screen, toscreen(Pt2(0,512,1)), toscreen(Pt2(0,-512,1)), 0, 0, 0, display->white, ZP);
	line(screen, toscreen(Pt2(512,0,1)), toscreen(Pt2(-512,0,1)), 0, 0, 0, display->white, ZP);
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, display->black, nil, ZP);
	drawaxes();
	drawvmodel(screen, model);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
rmb(Mousectl *, Keyboardctl *)
{
}

void
lmb(Mousectl *mc, Keyboardctl *)
{
	Point2 mpos;

	mpos = fromscreen(mc->xy);
	fprint(2, "%v\n", mpos);
}

void
zoom(Mousectl *mc)
{
	double oldscale, z;
	Point oldxy, Δxy;

	oldscale = scale;
	oldxy = mc->xy;

	for(;;){
		readmouse(mc);
		if(mc->buttons != 2)
			break;
		Δxy = subpt(mc->xy, oldxy);
		z = tanh((double)Δxy.y/100) + 1;
		scale = z*oldscale;
		redraw();
	}
}

void
mouse(Mousectl *mc, Keyboardctl *kc)
{
	if((mc->buttons&1) != 0)
		lmb(mc, kc);
	if((mc->buttons&2) != 0)
		zoom(mc);
	if((mc->buttons&4) != 0)
		rmb(mc, kc);
}

void
key(Rune r)
{
	switch(r){
	case Kdel:
	case 'q':
		threadexitsall(nil);
	}
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;

	GEOMfmtinstall();
	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();

	if(newwindow(nil) < 0)
		sysfatal("newwindow: %r");
	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	worldrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	worldrf.bx = Vec2(1, 0);
	worldrf.by = Vec2(0,-1);

	model = readvmodel("../assets/mdl/wedge.vmdl");
	if(model == nil)
		sysfatal("readvmodel: %r");

	display->locking = 1;
	unlockdisplay(display);
	redraw();

	for(;;){
		enum { MOUSE, RESIZE, KEYBOARD };
		Alt a[] = {
			{mc->c, &mc->Mouse, CHANRCV},
			{mc->resizec, nil, CHANRCV},
			{kc->c, &r, CHANRCV},
			{nil, nil, CHANEND}
		};

		switch(alt(a)){
		case MOUSE:
			mouse(mc, kc);
			break;
		case RESIZE:
			resized();
			break;
		case KEYBOARD:
			key(r);
			break;
		}

		redraw();
	}
}

void
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("couldn't resize");
	unlockdisplay(display);
	worldrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	redraw();
}
