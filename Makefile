CC=gcc -g 
CFF=-O2 

SRCFILES=$(shell find . -name '*.c')
THEFILES=$(basename $(SRCFILES))
THEOBJ=$(addsuffix .o,$(THEFILES))

.PHONY: All Clean

All: isicpu

Clean:
	@rm -v isicpu
	@rm -v ${THEOBJ}

isicpu: ${THEOBJ}
	@${CC} ${THEOBJ} -Wl,-as-needed -lrt -o isicpu
	@if [ -x isicpu ] ; then echo "Build complete"; fi

%.o: %.c
	@echo "$< > CC > $@ ($*)"
	@${CC} -c $< -o $@

