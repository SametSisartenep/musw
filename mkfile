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
	party.$O\
	player.$O\
	universe.$O\
	sprite.$O\
	vfx.$O\
	cmixer.$O\
	net.$O\
	fmt.$O\

HFILES=\
	dat.h\
	fns.h\
	cmixer.h\

</sys/src/cmd/mkmany

install:V: man

uninstall:V:
	for(i in $TARG){
		rm -f $BIN/$i
		rm -f $MAN/$i
	}
