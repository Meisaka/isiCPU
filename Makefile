CC=gcc -g 
CFLAGS += -O2 

SRCFILES=$(shell find . -name '*.c')
THEFILES=$(basename $(SRCFILES))
THEOBJ=$(addsuffix .o,$(THEFILES))

.PHONY: all clean

all: isicpu

clean:
	@rm -v isicpu
	@rm -v ${THEOBJ}

isicpu: ${THEOBJ} Makefile
	@${CC} ${THEOBJ} -Wl,-as-needed -lrt -o isicpu
	@if [ -x isicpu ] ; then echo "Build complete"; fi

%.o: %.c Makefile
	@echo "$< > CC > $@ ($*)"
	@${CC} ${CFLAGS} -c $< -o $@

