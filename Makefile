CC=gcc -g 
CXX=g++ -g
CFLAGS += -Wall -std=c99 -D_POSIX_C_SOURCE=200809L
CXXFLAGS += -Wall -std=c++11 -D_POSIX_C_SOURCE=200809L

cctest : CC=g++ -g -std=gnu++11
cctest : CFLAGS=-O2 -Wall -D_POSIX_C_SOURCE=200809L

SRCFILES=$(shell find . -name '*.c')
SRCPFILES=$(shell find . -name '*.cpp')
INCFILES=$(shell find . -name '*.h')
THEFILES=$(basename $(SRCFILES))
THEPFILES=$(basename $(SRCPFILES))
THEOBJ=$(addsuffix .o,$(THEFILES))
THEPOBJ=$(addsuffix .o,$(THEPFILES))

.PHONY: all clean

all: isicpu
cctest: all

clean:
	@rm -fv isicpu
	@rm -fv depends
	@rm -fv ${THEOBJ}

depends: ${SRCFILES} ${SRCPFILES} ${INCFILES} Makefile
	@rm -fv depends
	@for i in ${THEFILES} ; do echo "depends $$(${CC} ${CFLAGS} -MM $$i.c -MT $$i.o) Makefile" >> depends ; done
	@for i in ${THEPFILES} ; do echo "depends $$(${CXX} ${CXXFLAGS} -MM $$i.cpp -MT $$i.o) Makefile" >> depends ; done

isicpu: ${THEOBJ} ${THEPOBJ} Makefile depends
	@${CXX} ${THEOBJ} ${THEPOBJ} -Wl,-as-needed -lrt -o isicpu
	@if [ -x isicpu ] ; then echo "Build complete"; fi

include depends

%.o: %.c Makefile
	@echo "CC  : $< >> $@ ($*)"
	@${CC} ${CFLAGS} -c $< -o $@
%.o: %.cpp Makefile
	@echo "C++ : $< >> $@ ($*)"
	@${CXX} ${CXXFLAGS} -c $< -o $@

