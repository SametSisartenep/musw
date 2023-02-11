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

int
Φfmt(Fmt *f)
{
	int n, i;
	Frame *frame;

	frame = va_arg(f->args, Frame*);

	n = fmtprint(f, "id %x type %ud seq %ud ack %ud len %ud sig ",
		frame->id, frame->type, frame->seq, frame->ack, frame->len);
	for(i = 0; i < MD5dlen; i++)
		n += fmtprint(f, "%2.2x", frame->sig[i]);
	return n;
}
