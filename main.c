
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include "dcpu.h"
#include "dcpuhw.h"
#include "cputypes.h"
#define CLOCK_MONOTONIC_RAW             4   /* since 2.6.28 */
#define CLOCK_REALTIME_COARSE           5   /* since 2.6.32 */
#define CLOCK_MONOTONIC_COARSE          6   /* since 2.6.32 */

static struct timespec LTUTime;
static int fdserver = 0;
static int haltnow = 0;

static int listenportnumber = 58704;

static const int quantum = 1000000000 / 10000; // ns

enum {
	CPUSMAX = 800,
	CPUSMIN = 20
};
static int numberofcpus = 1;
static int softcpumax = 1;
static int softcpumin = 1;

static struct isiCPUInfo allcpus[CPUSMAX];
DCPU cpl[CPUSMAX];
uint16_t mem[CPUSMAX][0x10000];

static const char * const gsc_usage =
"Usage:\n%s [-Desrm] [-p <portnum>] [-B <binfile>]\n\n"
"Options:\n -D  Enable debug\n -e  Assume <binfile> is little-endian\n"
" -s  Enable server and wait for connection before\n"
"     starting emulation. (Valid with -r)\n"
" -p <portnum>  Listen on <portnum> instead of the default (valid with -s)\n"
" -r  Run a DCPU emulation (interactively).\n"
" -m  Emulate multiple DCPUs (test mode)\n"
" -D  Enable debugging and single stepping DCPU\n"
" -B <binfile>  Load <binfile> into DCPU memory starting at 0x0000.\n"
"      File is assmued to contain 16 bit words, 2 octets each in big-endian\n"
"      Use the -e option to load little-endian files.\n";

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
	while((i = read(fd, bf, 2)) > 0) {
		if(f < 0x20000) {
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
		if(haltnow) {
			fprintf(stderr, "FORCED ABORT!\n");
			exit(4);
		}
		haltnow = 1;
		fprintf(stderr, "SIGNAL CAUGHT!\n");
	}
}

static const char * DCPUOP[] = {
	"---", "SET", "ADD", "SUB", "MUL", "MLI", "DIV", "DVI",
	"MOD", "MDI", "AND", "BOR", "XOR", "SHR", "ASR", "SHL",
	"IFB", "IFC", "IFE", "IFN", "IFG", "IFA", "IFL", "IFU",
	"!18", "!19", "ADX", "SBX", "!1C", "!1D", "STI", "STD",
	"---", "JSR", "!02", "!03", "!04", "!05", "!06", "!07",
	"INT", "IAG", "IAS", "RFI", "IAQ", "!0d", "!0e", "!0f",
	"HWN", "HWQ", "HWI", "!13", "!14", "!15", "!16", "!17",
	"!18", "!19", "!1a", "!1b", "!1c", "!1d", "!1e", "!1f",
};
static const char * DCPUP[] = {
	"A", "B", "C", "X", "Y", "Z", "I", "J",
	"[A]", "[B]", "[C]", "[X]", "[Y]", "[Z]", "[I]", "[J]",
	"[A+%04x]", "[B+%04x]", "[C+%04x]", "[X+%04x]", "[Y+%04x]", "[Z+%04x]", "[I+%04x]", "[J+%04x]",
	"PSHPOP", "[SP]", "[SP+%04x]", "SP", "PC", "EX", "[%04x]", "%04x",
	"-1", " 0", " 1", " 2", " 3", " 4", " 5", " 6",
	" 7", " 8", " 9", "10", "11", "12", "13", "14",
	"15", "16", "17", "18", "19", "20", "21", "22",
	"23", "24", "25", "26", "27", "28", "29", "30",
};

static const char * DIAG_L4 =
	" A    B    C    X    Y    Z   \n"
	"%04x %04x %04x %04x %04x %04x \n"
	" I    J    PC   SP   EX   IA   CL\n"
	"%04x %04x %04x %04x %04x %04x % 2d \n";
static const char * DIAG_L2 =
	" A    B    C    X    Y    Z    I    J    PC   SP   EX   IA   CyL \n"
	"%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x % 2d  >";

void showdisasm_dcpu(const struct isiCPUInfo *info)
{
	const DCPU * cpu = (const DCPU*)info->cpustate;
	uint16_t ma = cpu->PC;
	uint16_t m = (cpu->memptr)[ma++];
	int op = m & 0x1f;
	int ob = (m >> 5) & 0x1f;
	int oa = (m >> 10) & 0x3f;
	int nwa, nwb, lu;
	uint16_t lua, lub;
	lu = 0;
	if(oa >= 8 && oa < 16) {
		lu |= 1;
		lua = cpu->R[oa - 8];
	} else if((oa >= 16 && oa < 24) || oa == 26 || oa == 30 || oa == 31) {
		nwa = (cpu->memptr)[ma++];
		if(oa != 31) lu |= 1;
		if(oa >= 16 && oa < 24) {
			lua = cpu->R[oa - 16] + nwa;
		} else if(oa == 24 || oa == 25) {
			lua = cpu->SP;
		} else if(oa == 26) {
			lua = cpu->SP + nwa;
		} else if(oa == 30) {
			lua = nwa;
		}
	}
	if(op) {
		if(ob >= 8 && ob < 16) {
			lu |= 2;
			lub = cpu->R[ob - 8];
		} else if((ob >= 16 && ob < 24) || ob == 26 || ob == 30 || ob == 31) {
			nwb = (cpu->memptr)[ma++];
			if(ob != 31) lu |= 2;
			if(ob >= 16 && ob < 24) {
				lub = cpu->R[ob - 16] + nwb;
			} else if(ob == 24) {
				lub = cpu->SP - 1;
			} else if(ob == 25) {
				lub = cpu->SP;
			} else if(ob == 26) {
				lub = cpu->SP + nwb;
			} else if(ob == 30) {
				lub = nwb;
			}
		}
		fprintf(stderr, "%s ", DCPUOP[op]);
		fprintf(stderr, DCPUP[ob], nwb);
		fprintf(stderr, ", ");
		fprintf(stderr, DCPUP[oa], nwa);
	} else {
		fprintf(stderr, "%s ", DCPUOP[32+ob]);
		fprintf(stderr, DCPUP[oa], nwa);
	}
	if(lu & 2) {
		fprintf(stderr, "  b: %04x [%04x]", lub, (cpu->memptr)[lub]);
	}
	if(lu & 1) {
		fprintf(stderr, "  a: %04x [%04x]", lua, (cpu->memptr)[lua]);
	}
	fprintf(stderr, "\n");
}

void showdiag_dcpu(const struct isiCPUInfo* info, int fmt)
{
	const DCPU * cpu = (const DCPU*)info->cpustate;
	const char *diagt;
	diagt = fmt ? DIAG_L2 : DIAG_L4;
	fprintf(stderr, diagt,
	cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3],cpu->R[4],cpu->R[5],
	cpu->R[6],cpu->R[7],cpu->PC,cpu->SP,cpu->EX,cpu->IA, info->cycl);
	if(fmt) {
		showdisasm_dcpu(info);
	}
}
void showdiag_up(int l)
{
	fprintf(stderr, "\e[%dA", l);
}

void fetchtime(struct timespec * t)
{
	clock_gettime(CLOCK_MONOTONIC_RAW, t);
}

void isi_addtime(struct timespec * t, size_t nsec) {
	size_t asec, ansec;
	asec  = nsec / 1000000000;
	ansec = nsec % 1000000000;
	t->tv_nsec += ansec;
	while(t->tv_nsec > 1000000000) {
		t->tv_nsec -= 1000000000;
		asec++;
	}
	t->tv_sec += asec;
}

void isi_setrate(struct isiCPUInfo *info, size_t rate) {
	info->runrate = 1000000000 / rate; // Nano seconds per cycle (100kHz)
	if(info->runrate > quantum) {
		info->itvl = (info->runrate / quantum); // quantums per cycle
		info->rate = 1;
	} else {
		info->rate = (quantum / info->runrate); // cycles per quantum
		info->itvl = 1;
	}
}

struct stats {
	int quanta;
	int cpusched;
};

int main(int argc, char**argv, char**envp)
{
	char * binf;
	int rqrun;
	int endian;
	int ssvr;
	int cux;
	uintptr_t gccq = 0;
	uintptr_t glccq = 0;
	uintptr_t lucycles = 0;
	uintptr_t lucpus = 0;
	struct stats sts = {0, };
	int paddlimit = 0;
	int premlimit = 0;
	int dbg;

	struct timeval ltv;
	fd_set fds;
	char cc;
	char ccv[10];

	dbg = 0;
	rqrun = 0;
	endian = 0;
	ssvr = 0;
	haltnow= 0;
	binf = NULL;
	memset(mem, 0, sizeof(short)*0x10000);
	memset(allcpus, 0, sizeof(allcpus));
	int i,k;

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
					case 'm':
						softcpumax = CPUSMAX;
						softcpumin = CPUSMIN;
						break;
					case 'p':
						k = -1;
						if(i+1 < argc) {
							listenportnumber = atoi(argv[i+1]);
							if(listenportnumber < 1 || listenportnumber > 65535) {
								fprintf(stderr, "Invalid port number %d\n", listenportnumber);
								return 1;
							}
							i++;
						}
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
			allcpus[cux].cpustate = &cpl[cux];
			isi_setrate(allcpus+cux, 100000); // 100kHz
			allcpus[cux].ctl = dbg ? (ISICTL_DEBUG | ISICTL_STEP) : 0;
			DCPU_init(allcpus+cux, mem[cux]);
			HWM_InitLoadout(allcpus[cux].cpustate, 4);
			HWM_DeviceAdd(allcpus[cux].cpustate, 2);
			HWM_DeviceAdd(allcpus[cux].cpustate, 0);
			HWM_DeviceAdd(allcpus[cux].cpustate, 1);
			HWM_DeviceAdd(allcpus[cux].cpustate, 5);
			HWM_InitAll(allcpus[cux].cpustate);
			DCPU_sethwqcallback(allcpus[cux].cpustate, HWM_Query);
			DCPU_sethwicallback(allcpus[cux].cpustate, HWM_HWI);
			if(binf) {
				loadbinfile(binf, endian, (unsigned char*)mem[cux]);
			}
		}
		if(ssvr) {
			makewaitserver();
			sleep(3);
		}

		sigaction(SIGINT, &hnler, NULL);

		for(cux = 0; cux < numberofcpus; cux++) {
			fetchtime(&allcpus[cux].nrun);
		}
		fetchtime(&LTUTime);
		lucycles = 0; // how many cycles ran (debugging)
		cux = 0; // CPU index - currently never changes
		if(allcpus[cux].ctl & ISICTL_DEBUG) {
			showdiag_dcpu(allcpus+cux, 1);
		}
		while(!haltnow) {
			struct isiCPUInfo * ccpu;
			struct timespec CRun;

			ccpu = allcpus + cux;
			fetchtime(&CRun);

			{
				int ccq = 0;
				int tcc = numberofcpus * 2;
				if(tcc < 20) tcc = 20;
				while(ccq < tcc && (ccpu->nrun.tv_sec < CRun.tv_sec || ccpu->nrun.tv_nsec < CRun.tv_nsec)) {
					sts.quanta++;
					ccpu->RunCycles(ccpu, CRun);
					//TODO some hardware may need to work at the same time
					lucycles += ccpu->cycl;
					if((ccpu->ctl & ISICTL_DEBUG) && (ccpu->cycl)) {
						showdiag_dcpu(ccpu, 1);
					}
					ccpu->cycl = 0;
					fetchtime(&CRun);
					ccq++;
				}
				if(ccq >= tcc) gccq++;
				sts.cpusched++;
				fetchtime(&CRun);

				HWM_TickAll(ccpu->cpustate, CRun, cux == 0 ? fdserver : 0, 0);
			}

			fetchtime(&CRun);
			if(CRun.tv_sec > LTUTime.tv_sec) { // roughly one second between status output
				double clkdelta;
				float clkrate;
				if(!dbg) showdiag_dcpu(&allcpus[0], 0);
				clkdelta = ((double)(CRun.tv_sec - LTUTime.tv_sec)) + (((double)CRun.tv_nsec) * 0.000000001);
				clkdelta-=(((double)LTUTime.tv_nsec) * 0.000000001);
				if(!lucpus) lucpus = 1;
				clkrate = ((((double)lucycles) * clkdelta) * 0.001) / numberofcpus;
				fprintf(stderr, "DC: %.4f sec, %d at % 9.3f kHz   (% 8d) [Q:% 8d, S:% 8d, SC:% 8d]\r",
						clkdelta, numberofcpus, clkrate, gccq,
						sts.quanta, sts.cpusched, sts.cpusched / numberofcpus
						);
				if(!dbg) showdiag_up(4);
				fetchtime(&LTUTime);
				if(gccq >= sts.cpusched / numberofcpus ) {
					if(numberofcpus > softcpumin) {
						numberofcpus--;
						premlimit--;
						paddlimit = 0;
					}
				} else {
					if(numberofcpus < softcpumax) {
						fetchtime(&allcpus[numberofcpus].nrun);
						isi_addtime(&allcpus[numberofcpus].nrun, quantum);
						numberofcpus++;
					}
				}
				lucycles = 0;
				lucpus = 0;
				gccq = 0;
				memset(&sts, 0, sizeof(struct stats));
				if(premlimit < 20) premlimit+=10;
				if(paddlimit < 20) paddlimit+=2;
			}

			ltv.tv_sec = 0;
			ltv.tv_usec = 0;
			FD_ZERO(&fds);
			FD_SET(0, &fds); // stdin
			i = select(1, &fds, NULL, NULL, &ltv);
			if(i > 0) {
				i = read(0, &cc, 1);
				if(i > 0) {
					struct timespec CRun;
					fetchtime(&CRun);
					switch(cc) {
					case 10:
						allcpus[0].ctl |= ISICTL_STEPE;
						break;
					case 'r':
						i = read(0, ccv, 5);
						if(i < 5) break;
						{
							uint32_t t = 0;
							uint32_t ti = 0;
							int k;
							for(k = 0; k < 4; k++) {
								ti = ccv[k] - '0';
								if(ti > 9) {
									ti -= ('A'-'0'-10);
								}
								if(ti > 15) {
									ti -= 'a' - 'A';
								}
								t = (t << 4) | (ti & 15);
							}
							ti = mem[0][t];
							fprintf(stderr, "READ %04x:%04x\n", t, ti);
						}
						break;
					case 'l':
						HWM_TickAll(&cpl[0], CRun, fdserver,0x3400);
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
					cux = 0;
					glccq = gccq;
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

