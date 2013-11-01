
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

static CPUSlot allcpus[500];
static int cpucount;

static const int listenportnumber = 58704;

enum {
	CPUSMAX = 300
};
static const int numberofcpus = CPUSMAX;

	DCPU cpl[CPUSMAX];
	short mem[CPUSMAX][0x10000];

static const char * gsc_usage = 
"Usage:\n%s [-Desr] [-c <asmfile>] [-B <binfile>]\n\n"
"Options:\n -D  Enable debug\n -e  Assume <binfile> is little-endian\n"
" -s  Enable server and wait for connection before\n"
"     starting emulation. (Valid with -r)\n"
" -r  Run a DCPU emulation.\n"
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

	while(!feof(infl))
	{
		i = getc(infl);
		if(i != -1)
		{
			if(tsz >= msz)
			{
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
		close(fdserver);
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
	while(i = read(fd, bf, 2) > 0)
	{
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
	long long ccq;
	long long lucycles;
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

	if( argc > 1 )
	{
		for(i = 1; i < argc; i++)
		{
			switch(argv[i][0])
			{
			case '-':
				for(k = 1; k > 0 && argv[i][k] > 31; k++) {
				switch(argv[i][k])
				{
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
	}
	else
	{
	fprintf(stderr, gsc_usage, argv[0]);
	return 0;
	}

	/*
	mem[0] = 0x7c01;
	mem[1] = 0x0032;
	mem[2] = 0xa00d;
	mem[3] = 0x7461;
	mem[4] = 0x8b83;
//	mem[5] = 0x7c07;
//	mem[6] = 0xfff9;
//	*/
	
	struct sigaction hnler;
	hnler.sa_handler = sysfaulthdl;
	//hnler.sa_mask = SA_NODEFER;
	hnler.sa_flags = 0;


	if(rqrun == 2) { // Test mode operation /////////////////////////
	tcc = 0;
	rr= time();
	printf(" A    B    C    X    Y    Z    I    J    PC   SP   EX   IA\n");
	DCPU_init(&cpl[0], mem[0]);
	for(k = 0; k < 1000; k++){
		// Randomize RAM
		rr += cpl[0].PC + cpl[0].R[0] + k;
		//fprintf(stderr,"RS[%x]", rr);
		srand(rr);
		//srand(0x510fc318);
		for(i = 0; i < 0x0000ffff; i++) {
			mem[0][i] = rand() ^ (0x00ffff - rand());
		}
		// Run the DCPU
	tcc = 0;
	while(tcc < 100000) {
		//fprintf(stderr,"%04x: %04x [%04x]", cpl[0].PC, mem[cpl[0].PC]);
		DCPU_run(&cpl[0],mem[0]);
		if(cpl[0].cycl > 1000) {
		tcc += cpl[0].cycl;
		cpl[0].cycl = 0;
		usleep(400);
		}
	}
	// Debug info
	fprintf(stderr, "%04x %04x %04x %04x %04x %04x %04x %04x ",cpl[0].R[0],cpl[0].R[1],cpl[0].R[2],cpl[0].R[3],cpl[0].R[4],cpl[0].R[5],cpl[0].R[6],cpl[0].R[7]);
	fprintf(stderr, "%04x %04x %04x %04x ",cpl[0].PC,cpl[0].SP,cpl[0].EX,cpl[0].IA);
	fprintf(stderr, "C: %d:%d (%d)\r", cpl[0].cycl, k);
	tcc += cpl[0].cycl;
	cpl[0].cycl = 0;
	}
	} else if(rqrun == 1) { // normal operation /////////////////////////
	
	for(cux = 0; cux < numberofcpus; cux++) {
	allcpus[cux].archtype = ARCH_DCPU;
	allcpus[cux].memsize = 0x10000;
	allcpus[cux].cpustate = &cpl[cux];
	allcpus[cux].memptr = mem[cux];
	allcpus[cux].runrate = 100000; // Nano seconds per cycle (100kHz)
	//allcpus[cux].RunCycles = DCPU_run; // TODO: not used yet
	allcpus[cux].cyclequeue = 0; // Should always be reset
	DCPU_init(allcpus[cux].cpustate, allcpus[cux].memptr);
	HWM_InitLoadout(allcpus[cux].cpustate, 3);
	HWM_DeviceAdd(allcpus[cux].cpustate, 2);
	HWM_DeviceAdd(allcpus[cux].cpustate, 0);
	HWM_DeviceAdd(allcpus[cux].cpustate, 1);
	HWM_InitAll(allcpus[cux].cpustate);
	DCPU_sethwqcallback(allcpus[cux].cpustate, HWM_Query);
	DCPU_sethwicallback(allcpus[cux].cpustate, HWM_HWI);
	if(binf) { loadbinfile(binf, endian, (char*)allcpus[cux].memptr); }
		}
	if(ssvr) {
		makewaitserver();
		sleep(3);
	}
	
	sigaction(SIGINT, &hnler, NULL);

	printf(" A    B    C    X    Y    Z    I    J    PC   SP   EX   IA\n");
	clock_gettime(CLOCK_MONOTONIC_RAW, &LUTime);
	allcpus[cux].lrun.tv_sec = LUTime.tv_sec;
	allcpus[cux].lrun.tv_nsec= LUTime.tv_nsec;
	clock_gettime(CLOCK_MONOTONIC_RAW, &LTUTime);
	tcc = 0;
	lucycles = 0; // how many cycles ran (debugging)
	cux = 0; // CPU index - currently never changes
	while(!haltnow) {

		if(((DCPU*)allcpus[cux].cpustate)->MODE != BURNING) {
		LUTime.tv_sec = allcpus[cux].lrun.tv_sec;
		LUTime.tv_nsec = allcpus[cux].lrun.tv_nsec;
		clock_gettime(CLOCK_MONOTONIC_RAW, &CUTime);
		DeltaTime.tv_sec = CUTime.tv_sec - LUTime.tv_sec;
		DeltaTime.tv_nsec = ((CUTime.tv_nsec/* + (DeltaTime.tv_sec * 1000000000)*/) - LUTime.tv_nsec);
		double clkdelta;
		clkdelta = ((double)(CUTime.tv_sec - LUTime.tv_sec)) + (((double)CUTime.tv_nsec) * 0.000000001);
		clkdelta-=(((double)LUTime.tv_nsec) * 0.000000001);
		if(DeltaTime.tv_sec) {
			DeltaTime.tv_nsec += 1000000000;
		}
		nsec_err = 0;
		if(allcpus[cux].runrate > 0) {
			ccq = (DeltaTime.tv_nsec / allcpus[cux].runrate);
			ccq = clkdelta * (((double)allcpus[cux].runrate) * 15.0);
			//nsec_err = DeltaTime.tv_nsec - (ccq * allcpus[cux].runrate);
			tcc += DeltaTime.tv_nsec;
		} else {
			ccq = 0;
		}

		if(ccq > 0) {
			ccq += allcpus[cux].cyclequeue;
		}
		if(ccq > 0) {
			for(j = 0; ccq > 0 && j < 1001; j++) { // limit calls
				//TODO dynamic / multiple CPUs: call the right one.
				// with memory and state.
				DCPU_run(allcpus[cux].cpustate,allcpus[cux].memptr);
				//TODO some hardware may need to work at the same time
				//tcc += cpl[0].cycl;
				//if(cux == 0) {
				lucycles += ((DCPU*)allcpus[cux].cpustate)->cycl;
				//}
				ccq -= ((DCPU*)allcpus[cux].cpustate)->cycl; // each op uses cycles, can be negative
				((DCPU*)allcpus[cux].cpustate)->cycl = 0;
			}
		}
		allcpus[cux].cyclequeue = ccq; // save when done
		/*
		nsec_err = lucycles * allcpus[cux].runrate;
		allcpus[cux].lrun.tv_nsec += nsec_err;
		if(allcpus[cux].lrun.tv_nsec > 1000000000) {
			allcpus[cux].lrun.tv_nsec -= 1000000000;
			allcpus[cux].lrun.tv_sec++;
		}
		*/
		clock_gettime(CLOCK_MONOTONIC_RAW, &allcpus[cux].lrun);
		//allcpus[cux].lrun.tv_nsec -= nsec_err;

		// If the queue has cycles left over then the server may be overloaded
		// some CPUs should be slowed or moved to a different server/thread

		HWM_TickAll(allcpus[cux].cpustate, fdserver,0);
		}

		cux++; if(cux > numberofcpus - 1) cux = 0;
	if(!dbg) {
		double clkdelta;
		float clkrate;
	clock_gettime(CLOCK_MONOTONIC_RAW, &TUTime);
	if(TUTime.tv_sec > LTUTime.tv_sec) { // roughly one second between debug output
	fprintf(stderr, "%04x %04x %04x %04x %04x %04x %04x %04x ",cpl[0].R[0],cpl[0].R[1],cpl[0].R[2],cpl[0].R[3],cpl[0].R[4],cpl[0].R[5],cpl[0].R[6],cpl[0].R[7]);
	fprintf(stderr, "%04x %04x %04x %04x ",cpl[0].PC,cpl[0].SP,cpl[0].EX,cpl[0].IA);
	clkdelta = ((double)(TUTime.tv_sec - LTUTime.tv_sec)) + (((double)TUTime.tv_nsec) * 0.000000001);
	clkdelta-=(((double)LTUTime.tv_nsec) * 0.000000001);
	clkrate = ((((double)lucycles) / clkdelta) * 0.001) / numberofcpus;
	lucycles = 0;
	fprintf(stderr, "DC: %.4f sec, %d at %.3f kHz  \r", clkdelta, numberofcpus, clkrate);
	tcc = 0;
	clock_gettime(CLOCK_MONOTONIC_RAW, &LTUTime);
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
	fprintf(stderr," A    B    C    X    Y    Z    I    J    PC   SP   EX   IA\n");
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
	}
	if(dbg && getc(stdin) == 'x') {
	break;
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
