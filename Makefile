CC=gcc -g 
CFF=-O2 

THEOBJ=main.o dcpu.o asm.o nya.o keyb.o dcpuhw.o dcpuhwtbl.o

All: isicpu

Clean:
	@rm -v ecisim
	@rm -v ${THEOBJ}

isicpu: ${THEOBJ}
	@${CC} ${THEOBJ} -Wl,-as-needed -lrt -o isicpu
	@if [ -x isicpu ] ; then echo "Build complete"; fi

main.o: main.c stdsys.h cputypes.h dcpu.h dcpuhw.h asm.h Makefile
	@${CC} -S main.c -o main.s
	@${CC} -c main.s -o main.o

asm.o: asm.c asm.h opcode.h
	@${CC} -c asm.c -o asm.o

dcpu.o: dcpu.s
	@${CC} -c dcpu.s -o dcpu.o

dcpu.s: dcpu.c dcpu.h dcpuhw.h opcode.h
	@${CC} -O2 -S dcpu.c -o dcpu.s

dcpuhw.o: dcpuhw.h dcpuhw.c dcpu.h
	@${CC} ${CFF} -c dcpuhw.c

dcpuhwtbl.o: dcpuhw.h dcpuhwtbl.c
	@${CC} ${CFF} -c dcpuhwtbl.c

keyb.o: keyb.c dcpuhw.h
	@${CC} ${CFF} -c keyb.c

nya.o: nya.c dcpuhw.h
	@${CC} ${CFF} -c nya.c

