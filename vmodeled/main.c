#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

typedef enum {
	SLine,
	SCurve,
	NStrokes
} Stroke;

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

typedef struct Object Object;
struct Object
{
	RFrame;
	VModel *mdl;

	void (*scale)(Object*, double);
	void (*rotate)(Object*, double);
};

char *strokename[NStrokes] = {
 [SLine]	"line",
 [SCurve]	"curve",
};

RFrame worldrf;
Object mainobj;
Stroke stroke;
Point ptstk[3];
Point *ptstkp;

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

void
obj_rotate(Object *o, double θ)
{
	Matrix R = {
		cos(θ), -sin(θ), 0,
		sin(θ),  cos(θ), 0,
		0, 0, 1,
	};
	o->bx = xform(o->bx, R);
	o->by = xform(o->by, R);
}

void
obj_scale(Object *o, double s)
{
	o->bx = mulpt2(o->bx, s);
	o->by = mulpt2(o->by, s);
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

int
writevmodel(VModel *mdl)
{
	Point2 *p;

	for(p = mdl->pts; p < mdl->pts+mdl->npts; p++)
		print("v %g %g\n", p->x, p->y);
	print("%s\n", mdl->strokefmt);
	return 0;
}

void
drawvmodel(Image *dst, VModel *mdl)
{
	int i;
	char *s;
	Point pts[3];
	Point2 *p;

	p = mdl->pts;
	for(s = mdl->strokefmt; s != 0 && p-mdl->pts < mdl->npts; s++)
		switch(*s){
		case 'l':
			line(dst, toscreen(invrframexform(p[0], mainobj)), toscreen(invrframexform(p[1], mainobj)), 0, 0, 0, display->white, ZP);
			p += 2;
			break;
		case 'c':
			for(i = 0; i < nelem(pts); i++)
				pts[i] = toscreen(invrframexform(p[i], mainobj));
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
drawinfo(void)
{
	Point p;
	char buf[128];

	p = Pt(10,3);

	snprint(buf, sizeof buf, "fmt %s", mainobj.mdl->strokefmt);
	string(screen, addpt(screen->r.min, p), display->white, ZP, font, buf);
	p.y += font->height;
	snprint(buf, sizeof buf, "op %s", strokename[stroke]);
	string(screen, addpt(screen->r.min, p), display->white, ZP, font, buf);
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, display->black, nil, ZP);
	drawaxes();
	drawvmodel(screen, mainobj.mdl);
	drawinfo();
	flushimage(display, 1);
	unlockdisplay(display);
}

//void
//plotaline(Mousectl *mc, Keyboardctl *)
//{
//
//}

//void
//plotacurve(Mousectl *mc, Keyboardctl *)
//{
//
//}

void
plot(Mousectl *mc, Keyboardctl *)
{
	Point2 mpos;

	mpos = fromscreen(mc->xy);
}

void
zoom(Mousectl *mc, Keyboardctl *)
{
	double z; /* zooming factor */
	Point oldxy, Δxy;

	oldxy = mc->xy;

	for(;;){
		readmouse(mc);
		if(mc->buttons != 2)
			break;
		Δxy = subpt(mc->xy, oldxy);
		z = tanh((double)Δxy.y/100) + 1;
		mainobj.scale(&mainobj, z);
		oldxy = mc->xy;
		redraw();
	}
}

void
rota(Mousectl *mc, Keyboardctl *)
{
	Point2 p;
	double oldθ, θ;

	p = rframexform(fromscreen(mc->xy), mainobj);
	oldθ = atan2(p.y, p.x);

	for(;;){
		readmouse(mc);
		if(mc->buttons != 4)
			break;
		p = rframexform(fromscreen(mc->xy), mainobj);
		θ = atan2(p.y, p.x) - oldθ;
		mainobj.rotate(&mainobj, θ);
		redraw();
	}
}

void
mouse(Mousectl *mc, Keyboardctl *kc)
{
	if((mc->buttons&1) != 0)
		plot(mc, kc);
	if((mc->buttons&2) != 0)
		zoom(mc, kc);
	if((mc->buttons&4) != 0)
		rota(mc, kc);
}

void
key(Rune r)
{
	switch(r){
	case Kdel:
	case 'q':
		writevmodel(mainobj.mdl);
		threadexitsall(nil);
	case 'l':
		stroke = SLine;
		break;
	case 'c':
		stroke = SCurve;
		break;
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
	mainobj.bx = Vec2(1, 0);
	mainobj.by = Vec2(0, 1);
	mainobj.scale = obj_scale;
	mainobj.rotate = obj_rotate;

	mainobj.mdl = readvmodel("../assets/mdl/wedge.vmdl");
	if(mainobj.mdl == nil)
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
