</$objtype/mkfile

MAN=/sys/man/1
BIN=/$objtype/bin/games
TARG=\
	musw\
	muswd\

OFILES=\
	alloc.$O\
	physics.$O\
	nanosec.$O\
	pack.$O\
	lobby.$O\
	party.$O\
	sprite.$O\

HFILES=\
	dat.h\
	fns.h\

LIB=\
	libgeometry/libgeometry.a$O\

</sys/src/cmd/mkmany

libgeometry/libgeometry.a$O: pulldeps
	cd libgeometry
	mk install

pulldeps:V:
	! test -d libgeometry && git/clone https://github.com/sametsisartenep/libgeometry || echo >/dev/null

install:V: man

uninstall:V:
	for(i in $TARG){
		rm -f $BIN/$i
		rm -f $MAN/$i
	}
