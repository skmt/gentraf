#
# Makefile
#

#--------------------------------------------------------------------------
# compile option
#--------------------------------------------------------------------------
CC	= gcc
TAGS	= ctags	# for vi
# in case the both options, optimizer and debugger are set it must fail to debug on gdb
OPTIM	= -pipe # -O
CFLAGS	= -g -Wall ${OPTIM}
LDFLAGS	= # -static

#--------------------------------------------------------------------------
# target and dependency
#--------------------------------------------------------------------------
SRC	= gentraf.c
TARGET	= gentraf


all:${TARGET} ${TAGS}

clean:
	rm -f ${TARGET} *.exe *.o core tags 
	rm -rf *.dSYM

ctags:
	ctags *.c

etags:
	etags *.c

${TARGET}: ${SRC}
	${CC} -o $@ ${SRC} ${CFLAGS} ${LDFLAGS}


# end of makefile
