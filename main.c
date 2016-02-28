
#include "asm.h"
#include "dcpu.h"
#include "dcpuhw.h"
#include "cputypes.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#define CLOCK_MONOTONIC_RAW             4   /* since 2.6.28 */
#define CLOCK_REALTIME_COARSE           5   /* since 2.6.32 */
#define CLOCK_MONOTONIC_COARSE          6   /* since 2.6.32 */

static struct timespec LUTime;
static struct timespec CUTime;
static struct timespec DeltaTime;
static struct timespec TUTime;
static struct timespec LTUTime;
static int fdserver;
static int haltnow;

static int cpucount;

static const int listenportnumber = 58704;

enum {
	CPUSMAX = 800,
	CPUSMIN = 20
};
static int numberofcpus = 1;
static int softcpumax = 1;
static int softcpumin = 1;

static CPUSlot allcpus[CPUSMAX];
DCPU cpl[CPUSMAX];
short mem[CPUSMAX][0x10000];

static const char * const gsc_usage = 
"Usage:\n%s [-Desr] [-c <asmfile>] [-B <binfile>]\n\n"
"Options:\n -D  Enable debug\n -e  Assume <binfile> is little-endian\n"
" -s  Enable server and wait for connection before\n"
"     starting emulation. (Valid with -r)\n"
" -r  Run a DCPU emulation.\n"
" -m  Emulate multiple DCPUs\n"
" -B <binfile>  Load <binfile> into DCPU memory starting at 0x0000.\n"
"      File is assmued to contain 16 bit words, 2 octets each in big-endian\n"
"      Use the -e option to load little-endian files.\n"
" -c <asmfile>  Load <asmfile> and asmemble it. (in-dev feature)\n";

int loadtxtfile(FILE* infl, char** txtptr, int * newsize)
{
	char * txt;
	void * tmpp;
	int msz;
	int tsz;
	int i;
	tsz = 0;
	msz = 1000;
	txt = (char*)malloc(msz);

	while(!feof(infl)) {
		i = getc(infl);
		if(i != -1) {
			if(tsz >= msz) {
				msz += 1000;
				tmpp = realloc(txt, msz);
				if(!tmpp) {
					fprintf(stderr, "!!! Out of memory !!!\n");
				       	return -5;
				}
				txt = (char*)tmpp;
			}
			txt[tsz++] = (char)i;
		}
	}
	*txtptr = txt;
	*newsize = tsz;
	return msz;
}

int makewaitserver()
{
	int fdsvr, i;
	struct sockaddr_in lipn;
	struct sockaddr_in ripn;
	socklen_t rin;

	memset(&lipn, 0, sizeof(lipn));
	memset(&ripn, 0, sizeof(ripn));
	lipn.sin_family = AF_INET;
	lipn.sin_port = htons(listenportnumber);
	fdsvr = socket(AF_INET, SOCK_STREAM, 0);
	if(fdsvr < 0) { perror("socket"); return -1; }
	i = 1;
	setsockopt(fdsvr, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int));
	i = 1;
	if( setsockopt(fdsvr, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		perror("set'opt");
		close(fdsvr);
		return -1;
	}
	if( bind(fdsvr, (struct sockaddr*)&lipn, sizeof(struct sockaddr_in)) < 0 ) {
		perror("bind");
		return -1;
	}
	if( listen(fdsvr, 1) < 0 ) {
		perror("listen");
		return -1;
	}
	fprintf(stderr, "Listening on port %d ...\n", listenportnumber);
	rin = sizeof(ripn);
	fdserver = accept(fdsvr, (struct sockaddr*)&ripn, &rin);
	if(fdserver < 0) {
		perror("accept");
		close(fdsvr);
		fdserver = 0;
		return -1;
	}
	i = 1;
	if( setsockopt(fdserver, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		perror("set'opt");
		close(fdsvr);
		close(fdserver);
		return -1;
	}
	if( fcntl(fdserver, F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl");
		close(fdsvr);
		close(fdserver);
		return -1;
	}
	close(fdsvr);
	return 0;
}

int loadbinfile(const char* path, int endian, unsigned char * mem)
{
	int fd, i, f;
	char bf[4];
	fd = open(path, O_RDONLY);
	if(fd < 0) { perror("open"); return -5; }

	f = 0;
	while(i = read(fd, bf, 2) > 0) {
		if(f < 0x10000) {
			if(endian) {
			mem[f++] = bf[1];
			mem[f++] = bf[0];
			} else {
			mem[f++] = bf[0];
			mem[f++] = bf[1];
			}
		}
	}
	close(fd);
	return 0;
}

void sysfaulthdl(int ssn) {
	if(ssn == SIGINT) {
		haltnow = 1;
		fprintf(stderr, "SIGNAL CAUGHT!\n");
	}
}

static const char * DIAG_L4 = 
	" A    B    C    X    Y    Z   \n"
	"%04x %04x %04x %04x %04x %04x \n"
	" I    J    PC   SP   EX   IA  \n"
	"%04x %04x %04x %04x %04x %04x \n";
static const char * DIAG_L2 = 
	" A    B    C    X    Y    Z    I    J    PC   SP   EX   IA  \n"
	"%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x \n";

void showdiag_dcpu(const DCPU* cpu, int fmt)
{
	const char *diagt;
	diagt = fmt ? DIAG_L2 : DIAG_L4;
	fprintf(stderr, diagt,
	cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3],cpu->R[4],cpu->R[5],
	cpu->R[6],cpu->R[7],cpu->PC,cpu->SP,cpu->EX,cpu->IA);
}
void showdiag_up(int l)
{
	fprintf(stderr, "\e[%dA", l);
}

void fetchtime(struct timespec * t)
{
	clock_gettime(CLOCK_MONOTONIC_RAW, t);
}

int main(int argc, char**argv, char**envp)
{

	FILE* opfile;
	char * asmtxt;
	char * binf;
	int asmlen;
	int rqrun;
	int endian;
	int avcycl;
	int ssvr;
	int cux;
	long nsec_err;
	long long tcc;
	uintptr_t gccq = 0;
	uintptr_t glccq = 0;
	uintptr_t lucycles = 0;
	uintptr_t lucpus = 0;
	int paddlimit = 0;
	int dbg;

	struct timeval ltv;
	fd_set fds;
	char cc;

	dbg = 0;
	rqrun = 0;
	avcycl = 0;
	endian = 0;
	ssvr = 0;
	haltnow= 0;
	binf = NULL;
	memset(mem, 0, sizeof(short)*0x10000);
	memset(allcpus, 0, sizeof(allcpus));
	int i,k, j , rr;

	if( argc > 1 ) {
		for(i = 1; i < argc; i++) {
			switch(argv[i][0]) {
			case '-':
				for(k = 1; k > 0 && argv[i][k] > 31; k++) {
					switch(argv[i][k]) {
					case 'T':
						rqrun = 2;
						break;
					case 'r':
						rqrun = 1;
						break;
					case 'c':
						k = -1; // end search
						if(i+1 < argc) {
							opfile = fopen(argv[i+1], "r");
							if(!opfile) {
								perror("Open File");
							} else {
								fprintf(stderr, "Loading file\n");
							loadtxtfile(opfile, &asmtxt, &asmlen);
							fclose(opfile);
							DCPUASM_asm(asmtxt, asmlen, mem[0]);
							i++;
							}
						}
						break;
					case 'm':
						softcpumax = CPUSMAX;
						softcpumin = CPUSMIN;
						numberofcpus = CPUSMIN;
						break;
					case 'e':
						endian = 2;
						break;
					case 's':
						ssvr = 1;
						break;
					case 'D':
						dbg = 1;
						break;
					case 'B':
						k = -1;
						if(i+1<argc) {
							binf = argv[++i];
						}
						break;
					}
				}
				break;
			default:
				fprintf(stderr, "\"%s\" Ignored\n", argv[i]);
				break;
			}
		}
	} else {
		fprintf(stderr, gsc_usage, argv[0]);
		return 0;
	}

	struct sigaction hnler;
	hnler.sa_handler = sysfaulthdl;
	hnler.sa_flags = 0;


	if(rqrun == 1) { // normal operation /////////////////////////
	
		for(cux = 0; cux < softcpumax; cux++) {
			allcpus[cux].archtype = ARCH_DCPU;
			allcpus[cux].memsize = 0x10000;
			allcpus[cux].cpustate = &cpl[cux];
			allcpus[cux].memptr = mem[cux];
			allcpus[cux].runrate = 10000; // Nano seconds per cycle (100kHz)
			allcpus[cux].RunCycles = DCPU_run;
			allcpus[cux].nse = 0;
			allcpus[cux].cyclequeue = 0; // Should always be reset
			DCPU_init(allcpus[cux].cpustate, allcpus[cux].memptr);
			HWM_InitLoadout(allcpus[cux].cpustate, 3);
			HWM_DeviceAdd(allcpus[cux].cpustate, 2);
			HWM_DeviceAdd(allcpus[cux].cpustate, 0);
			HWM_DeviceAdd(allcpus[cux].cpustate, 1);
			HWM_InitAll(allcpus[cux].cpustate);
			DCPU_sethwqcallback(allcpus[cux].cpustate, HWM_Query);
			DCPU_sethwicallback(allcpus[cux].cpustate, HWM_HWI);
			if(binf) {
				loadbinfile(binf, endian, (char*)allcpus[cux].memptr);
			}
		}
		if(ssvr) {
			makewaitserver();
			sleep(3);
		}
		
		sigaction(SIGINT, &hnler, NULL);

		for(cux = 0; cux < numberofcpus; cux++) {
			fetchtime(&allcpus[cux].lrun);
		}
		fetchtime(&LTUTime);
		lucycles = 0; // how many cycles ran (debugging)
		cux = 0; // CPU index - currently never changes
		while(!haltnow) {
			CPUSlot * ccpu;
			ccpu = allcpus + cux;

			if(((DCPU*)ccpu->cpustate)->MODE != BURNING) {
				struct timespec *LRun = &ccpu->lrun;
				LUTime.tv_sec = LRun->tv_sec;
				LUTime.tv_nsec = LRun->tv_nsec;
				fetchtime(LRun);
				DeltaTime.tv_sec = LRun->tv_sec - LUTime.tv_sec;
				DeltaTime.tv_nsec = LRun->tv_nsec - LUTime.tv_nsec;
				DeltaTime.tv_nsec += ccpu->nse;
				if(DeltaTime.tv_sec) {
					DeltaTime.tv_nsec += 1000000000;
				}
				nsec_err = 0;
				uintptr_t ccq = 0;
				if(ccpu->runrate > 0) {
					ccq = (DeltaTime.tv_nsec / ccpu->runrate);
					ccpu->nse = (DeltaTime.tv_nsec % ccpu->runrate);
					ccq += ccpu->cyclequeue;
					if(ccpu->cyclewait) {
						if(ccpu->cyclewait < ccq) {
							ccq -= ccpu->cyclewait;
							ccpu->cyclewait = 0;
						} else {
							ccpu->cyclewait -= ccq;
							ccq = 0;
						}
					}
				} else {
					ccq = 0;
				}

				if(dbg) {
					if(lucycles) {
						ccpu->RunCycles(ccpu, ccpu->cpustate, ccpu->memptr);
						ccpu->cycl = 0;
						lucycles = 0;
						showdiag_dcpu((DCPU*)ccpu->cpustate, 1);
					}
				} else {
					if(ccq) {
						for(j = 10000000 / ccpu->runrate; ccq && j; j--) { // limit calls
							ccpu->RunCycles(ccpu, ccpu->cpustate, ccpu->memptr);
							//TODO some hardware may need to work at the same time
							lucycles += ccpu->cycl;
							if(ccq < ccpu->cycl) { // used too many
								ccpu->cyclewait += (ccpu->cycl - ccq);
								ccq = 0;
							} else {
								ccq -= ccpu->cycl; // each op uses cycles
							}
							ccpu->cycl = 0;
						}
					}
					ccpu->cyclequeue = ccq; // save when done
					gccq += ccq;
				}

				// If the queue has cycles left over then the server may be overloaded
				// some CPUs should be slowed or moved to a different server/thread

				HWM_TickAll(ccpu->cpustate, cux == 0 ? fdserver : 0, 0);
			}

			double clkdelta;
			float clkrate;
			fetchtime(&TUTime);
			if(!dbg && TUTime.tv_sec > LTUTime.tv_sec) { // roughly one second between status output
				showdiag_dcpu((DCPU*)allcpus[0].cpustate, 0);
				clkdelta = ((double)(TUTime.tv_sec - LTUTime.tv_sec)) + (((double)TUTime.tv_nsec) * 0.000000001);
				clkdelta-=(((double)LTUTime.tv_nsec) * 0.000000001);
				if(!lucpus) lucpus = 1;
				clkrate = ((((double)lucycles) / clkdelta) * 0.001) / numberofcpus;
				fprintf(stderr, "DC: %.4f sec, %d at %.3f kHz  (%d)\r", clkdelta, numberofcpus, clkrate, gccq);
				showdiag_up(4);
				fetchtime(&LTUTime);
				lucycles = 0;
				lucpus = 0;
			}

			ltv.tv_sec = 0;
			ltv.tv_usec = 0;
			FD_ZERO(&fds);
			FD_SET(0, &fds); // stdin
			i = select(1, &fds, NULL, NULL, &ltv);
			if(i > 0) {
				i = read(0, &cc, 1);
				if(i > 0) {
					switch(cc) {
					case 10:
						lucycles = 1;
						break;
					case 'l':
						HWM_TickAll(&cpl[0], fdserver,0x3400);
						break;
					case 'x':
						haltnow = 1;
						break;
					default:
						break;
					}
				}
			}
			if(!dbg) {
				cux++;
				if(cux > numberofcpus - 1) {
					if(gccq) {
						if(numberofcpus > softcpumin) {
							if(gccq > glccq) numberofcpus--;
						}
						paddlimit = 0;
					} else {
						if(numberofcpus < softcpumax && paddlimit > 0) {
							fetchtime(&ccpu->lrun);
							ccpu->cyclequeue = 0;
							numberofcpus++;
							paddlimit--;
						} else {
							if(paddlimit < softcpumax) paddlimit++;
						}
					}
					cux = 0;
					glccq = gccq;
					gccq = 0;
				}
			} else {
				numberofcpus = 1;
				cux = 0;
			}
			if(haltnow) {
				break;
			}
		}
		HWM_FreeAll(&cpl[0]);
		shutdown(fdserver, SHUT_RDWR);
		close(fdserver);
	}
	printf("\n");
	return 0;
}

