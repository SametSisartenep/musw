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

static void
sprite_step(Sprite *spr, ulong Δt)
{
	if(spr->nframes < 2)
		return;

	spr->elapsed += Δt;

	if(spr->elapsed >= spr->period){
		spr->elapsed -= spr->period;
		spr->curframe = ++spr->curframe % spr->nframes;
	}
}

static void
sprite_draw(Sprite *spr, Image *dst, Point dp)
{
	Point sp = (Point){
		spr->curframe * Dx(spr->r),
		0
	};
	sp = addpt(spr->sp, sp);

	draw(dst, rectaddpt(spr->r, dp), spr->sheet, nil, sp);
}

static Sprite *
sprite_clone(Sprite *spr)
{
	return newsprite(spr->sheet, spr->sp, spr->r, spr->nframes, spr->period);
}

Sprite *
newsprite(Image *sheet, Point sp, Rectangle r, int nframes, ulong period)
{
	Sprite *spr;

	spr = emalloc(sizeof(Sprite));
	spr->sheet = sheet;
	spr->sp = sp;
	spr->r = r;
	spr->nframes = nframes;
	spr->curframe = 0;
	spr->period = period;
	spr->elapsed = 0;
	spr->step = sprite_step;
	spr->draw = sprite_draw;
	spr->clone = sprite_clone;

	return spr;
}

Sprite *
readsprite(char *sheetfile, Point sp, Rectangle r, int nframes, ulong period)
{
	Image *sheet;
	int fd;

	fd = open(sheetfile, OREAD);
	if(fd < 0)
		sysfatal("readsprite: %r");
	sheet = readimage(display, fd, 1);
	close(fd);

	return newsprite(sheet, sp, r, nframes, period);
}

static void
decproc(void *arg)
{
	int fd, *pfd;

	pfd = arg;
	fd = pfd[2];

	close(pfd[0]);
	dup(fd, 0);
	close(fd);
	dup(pfd[1], 1);
	close(pfd[1]);

	execl("/bin/png", "png", "-t9", nil);
	threadexitsall("execl: %r");
}

Sprite *
readpngsprite(char *sheetfile, Point sp, Rectangle r, int nframes, ulong period)
{
	Image *sheet;
	int fd, pfd[3];

	if(pipe(pfd) < 0)
		sysfatal("pipe: %r");
	fd = open(sheetfile, OREAD);
	if(fd < 0)
		sysfatal("readpngsprite: %r");
	pfd[2] = fd;
	procrfork(decproc, pfd, mainstacksize, RFFDG|RFNAMEG|RFNOTEG);
	close(pfd[1]);
	sheet = readimage(display, pfd[0], 1);
	close(pfd[0]);
	close(fd);

	return newsprite(sheet, sp, r, nframes, period);
}

void
delsprite(Sprite *spr)
{
	//freeimage(spr->sheet);
	free(spr);
}
