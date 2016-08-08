CC=gcc -g 
CFLAGS += -Wall -std=c99 -D_POSIX_C_SOURCE=200809L

cctest : CC=g++ -g
cctest : CFLAGS=-O2 -Wall -D_POSIX_C_SOURCE=200809L

SRCFILES=$(shell find . -name '*.c')
INCFILES=$(shell find . -name '*.h')
THEFILES=$(basename $(SRCFILES))
THEOBJ=$(addsuffix .o,$(THEFILES))

.PHONY: all clean

all: isicpu
cctest: all

clean:
	@rm -fv isicpu
	@rm -fv ${THEOBJ}

depends: ${SRCFILES} ${INCFILES} Makefile
	@rm -fv depends
	@for i in ${SRCFILES} ; do echo "depends $$(${CC} ${CFLAGS} -MM $$i)" >> depends ; done

isicpu: ${THEOBJ} Makefile depends
	@${CC} ${THEOBJ} -Wl,-as-needed -lrt -o isicpu
	@if [ -x isicpu ] ; then echo "Build complete"; fi

include depends

%.o: %.c Makefile
	@echo "$< > CC > $@ ($*)"
	@${CC} ${CFLAGS} -c $< -o $@
