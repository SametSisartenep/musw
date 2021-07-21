#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

void
threadnetin(void *arg)
{
	char buf[256];
	int fd, n;
	Ioproc *io;

	fd = *((int*)arg);
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof buf)) > 0)
		if(iowrite(io, 1, buf, n) != n)
			fprint(2, "iowrite: %r\n");
	closeioproc(io);
}

void
threadnetout(void *arg)
{
	char buf[256];
	int fd, n;
	Ioproc *io;

	fd = *((int*)arg);
	io = ioproc();

	while((n = ioread(io, 0, buf, sizeof buf)) > 0)
		if(iowrite(io, fd, buf, n) != n)
			fprint(2, "iowrite: %r\n");
	closeioproc(io);
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

	ARGBEGIN{
	default:
		usage();
	}ARGEND;
	if(argc != 1)
		usage();
	server = argv[0];

	fd = dial(server, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");

	threadcreate(threadnetin, &fd, 4096);
	threadcreate(threadnetout, &fd, 4096);
	threadexits(nil);
}