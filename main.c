
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include "dcpuhw.h"
#include "cputypes.h"
#include "netmsg.h"
#define CLOCK_MONOTONIC_RAW             4   /* since 2.6.28 */
#define CLOCK_REALTIME_COARSE           5   /* since 2.6.32 */
#define CLOCK_MONOTONIC_COARSE          6   /* since 2.6.32 */

static struct timespec LTUTime;
static int haltnow = 0;
static int loadendian = 0;
static int flagdbg = 0;
static int rqrun = 0;
static int flagsvr = 0;
static char * binf = 0;
static char * endf = 0;
static char * redisaddr = 0;
static char * diskf = 0;
static int listenportnumber = 58704;
static int verblevel = L_INFO;

int fdlog = STDERR_FILENO;
int usefs = 0;

static const int quantum = 1000000000 / 10000; // ns

enum {
	CPUSMAX = 800,
	CPUSMIN = 20
};
static uint32_t numberofcpus = 1;
static uint32_t softcpumax = 1;
static uint32_t softcpumin = 1;

extern struct isiDevTable alldev;
extern struct isiDevTable allcpu;
extern struct isiConTable allcon;
extern struct isiObjTable allobj;
struct isiSessionTable allses;

void isi_run_sync(struct timespec crun);
void isi_register_objects();
void isi_init_contable();
void isi_objtable_init();
int isi_scan_dir();
void isi_init_sestable();
int isi_delete_ses(struct isiSession *s);
int makeserver(int portnumber);
int redis_make_session_lu(const char * addrstr);
void showdisasm_dcpu(const struct isiInfo *info);
void showdiag_dcpu(const struct isiInfo* info, int fmt);

static const char * const gsc_usage =
"Usage:\n%s [-DFersv] [-p <portnum>] [-B <binid>] [-d <diskid>] [-R <connection>]\n%s -E <file>\n"
"Options:\n"
" -e  Assume file pointed to by <binid> is little-endian\n"
" -s  Enable server, if specified without -r then fork to background.\n"
" -p <portnum>  Listen on <portnum> instead of the default (valid with -s)\n"
" -r  Run interactively.\n"
" -v  increate verbosity, use multiple times to increase levels\n"
" -F  allow file system operations\n"
" -D  Enable debugging and single stepping DCPU\n"
" -B <binid>  Load file with <binid> into DCPU memory starting at 0x0000.\n"
"      File is assmued to contain 16 bit words, 2 octets each in big-endian\n"
"      Use the -e option to load little-endian files.\n"
" -d <diskid>  Load disk with <diskid> into the default floppy drive.\n"
" -l  List file IDs then exit.\n"
" -E <file>  Flip the bytes in each 16 bit word of <file> then exit.\n"
" -R <connection>  Connect to a redis database on startup\n"
"     where <connection> is an address[:port], port is optional. The address\n"
"     may be enclosed in brackets [] to include colon seperated IPv6 addresses\n"
;

#define ISILOGBUFFER 4096
static char logwrout[ISILOGBUFFER];
void isilogerr(const char * desc)
{
	int r;
	r = snprintf(logwrout, ISILOGBUFFER, "Error: %s: %s\n", desc, strerror(errno));
	if(r > 0) {
		write(fdlog, logwrout, r);
	}
}

void isilog(int level, const char *format, ...)
{
	va_list vl;
	int r;
	if(level > verblevel) return;
	va_start(vl, format);
	r = vsnprintf(logwrout, ISILOGBUFFER, format, vl);
	if(r < 0) {
		va_end(vl);
		return;
	}
	write(fdlog, logwrout, r);
	va_end(vl);
}

void sysfaulthdl(int ssn) {
	switch(ssn) {
	case SIGTERM:
		isilog(L_CRIT, "SHUTTING DOWN ON SIGTERM!\n");
		exit(4);
	case SIGINT:
		if(haltnow) {
			isilog(L_CRIT, "FORCED ABORT!\n");
			exit(4);
		}
		haltnow = 1;
		isilog(L_ERR, "SIGNAL CAUGHT!\n");
		break;
	case SIGHUP:
		isilog(L_WARN, "HUP RECEIVED\n");
		break;
	case SIGPIPE:
		isilog(L_WARN, "SOCKET SIGNALED!\n");
		break;
	default:
		isilog(L_ERR, "Unknown signal\n");
		break;
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
	while(t->tv_nsec >= 1000000000) {
		t->tv_nsec -= 1000000000;
		asec++;
	}
	t->tv_sec += asec;
}

int isi_time_lt(struct timespec *a, struct timespec *b) {
	return (a->tv_sec < b->tv_sec) || ((a->tv_sec == b->tv_sec) && (a->tv_nsec < b->tv_nsec));
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

int isi_addcpu()
{
	struct isiInfo *bus, *ninfo;
	struct isiCPUInfo *cpu;
	isiram16 nmem;
	isi_make_object(isi_lookup_name("dcpu"), (struct objtype**)&cpu, 0, 0);
	cpu->ctl = flagdbg ? (ISICTL_DEBUG | ISICTL_TRACE | ISICTL_STEP) : 0;
	isi_make_object(isi_lookup_name("memory_16x64k"), (struct objtype**)&nmem, 0, 0);
	isi_make_object(isi_lookup_name("dcpu_hwbus"), (struct objtype**)&bus, 0, 0);
	isi_attach(bus, 0, (struct isiInfo*)nmem, ISIAT_APPEND, 0, 0);
	isi_attach(bus, 0, (struct isiInfo*)cpu, ISIAT_UP, 0, 0);
	isi_make_object(isi_lookup_name("nya_lem"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("clock"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("speaker"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("keyboard"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("kaihic32"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);
	if(binf) {
		uint8_t ist[24];
		ist[0] = 0;
		uint64_t id = 0;
		isi_fname_id(binf, &id);
		isi_write_parameter(ist, 24, 2, &id, sizeof(uint64_t));
		if(loadendian) isi_write_parameter(ist, 24, 3, &id, 0);
		isi_make_object(isi_lookup_name("rom"), (struct objtype**)&ninfo, ist, 24);
		isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);
	}

	isi_make_object(isi_lookup_name("mack_35fd"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);
	if(diskf) {
		uint8_t ist[24];
		ist[0] = 0;
		uint64_t dsk = 0;
		struct isiInfo *ndsk;
		isi_fname_id(diskf, &dsk);
		isi_write_parameter(ist, 24, 1, &dsk, sizeof(uint64_t));
		isi_make_object(isi_lookup_name("disk"), (struct objtype**)&ndsk, ist, 24);
		isi_attach(ninfo, 0, ndsk, ISIAT_UP, 0, 0);
	}
	isi_make_object(isi_lookup_name("imva"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	return 0;
}

void isi_debug_dump_objtable()
{
	uint32_t u = 0;
	while(u < allobj.count) {
		fprintf(stderr, "obj-list: [%08x]: %x\n", allobj.table[u]->id, allobj.table[u]->objtype);
		u++;
	}
}

void isi_debug_dump_cputable()
{
	uint32_t u = 0;
	while(u < allcpu.count) {
		fprintf(stderr, "cpu-list: [%08x]: %x\n", allcpu.table[u]->id.id, allcpu.table[u]->id.objtype);
		u++;
	}
}
void isi_redis_test();

int handle_stdin(struct isiSession *ses, struct timespec mtime)
{
	int i;
	char cc[16];
	i = read(ses->sfd, cc, 16);
	if(i < 1) return 0;
	struct isiCPUInfo *cpu = NULL;
	if(allcpu.count) {
		cpu = (struct isiCPUInfo*)allcpu.table[0];
	}
	switch(cc[0]) {
	case 10:
		if(cpu && cpu->ctl & ISICTL_DEBUG) {
			cpu->ctl |= ISICTL_STEPE;
		}
		break;
	case 'c':
		if(cpu && cpu->ctl & ISICTL_DEBUG) {
			cpu->ctl &= ~(ISICTL_STEP | ((cpu->ctl & ISICTL_TRACEC) ? 0:ISICTL_TRACE));
			cpu->ctl |= ISICTL_STEPE;
		}
		break;
	case 'f':
		if(!cpu) break;
		{
			uint32_t t = 0;
			uint32_t ti = 0;
			int k;
			for(k = 1; k < i; k++) {
				ti = cc[k] - '0';
				if(ti < 10) {
					t *= 10;
					t += ti;
				}
			}
			if(t) {
				cpu->ctl &= ~(ISICTL_STEP | ISICTL_STEPE | ((cpu->ctl & ISICTL_TRACEC) ? 0:ISICTL_TRACE));
				cpu->ctl |= ISICTL_RUNFOR;
				cpu->rcycl = t;
			}
		}
		break;
	case 'r':
		if(!cpu) break;
		if(i < 5) break;
		{
			uint32_t t = 0;
			uint32_t ti = 0;
			int k;
			for(k = 1; k < 5; k++) {
				ti = cc[k] - '0';
				if(ti > 9) {
					ti -= ('A'-'0'-10);
				}
				if(ti > 15) {
					ti -= 'a' - 'A';
				}
				t = (t << 4) | (ti & 15);
			}
			ti = ((isiram16)allcpu.table[0]->mem)->ram[t];
			fprintf(stderr, "debug: read %04x:%04x\n", t, ti);
		}
		break;
	case 'b':
		if(!cpu) break;
		if(i < 5) break;
		{
			uint32_t t = 0;
			uint32_t ti = 0;
			int k;
			for(k = 1; k < 5; k++) {
				ti = cc[k] - '0';
				if(ti > 9) {
					ti -= ('A'-'0'-10);
				}
				if(ti > 15) {
					ti -= 'a' - 'A';
				}
				t = (t << 4) | (ti & 15);
			}
			isi_cpu_togglebrk((isiram16)allcpu.table[0]->mem, t);
			k = isi_cpu_isbrk((isiram16)allcpu.table[0]->mem, t);
			fprintf(stderr, "debug: Break point %sed at %04x\n", k?"enabl":"disabl",t);
		}
		break;
	case 'x':
		haltnow = 1;
		break;
	case 't':
		if(!cpu) break;
		cpu->ctl ^= ISICTL_TRACEC;
		fprintf(stderr, "debug: trace on continue %sed\n", (cpu->ctl & ISICTL_TRACEC)?"enabl":"disabl");
		break;
	case 'n':
		if(allcpu.count) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_synctable();
		break;
	case 'L':
		isi_redis_test();
		break;
	case 'l':
		if(allcpu.count) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_objtable();
		break;
	case 'p':
		if(allcpu.count) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_cputable();
		break;
	default:
		break;
	}
	return 0;
}

int make_interactive()
{
	struct isiSession *ses;
	int i;
	if((i= isi_create_object(ISIT_SESSION, (struct objtype **)&ses))) {
		return i;
	}
	if(fcntl(0, F_SETFL, O_NONBLOCK) < 0) {
		isilogerr("fcntl");
		return -1;
	}
	ses->sfd = 0;
	ses->Recv = handle_stdin;
	isi_pushses(ses);
	return 0;
}

struct stats {
	int quanta;
	int cpusched;
};

static int parse_args(int argc, char**argv)
{
	int i, k;
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
				case 'v':
					verblevel++;
					break;
				case 'l':
					isi_scan_dir();
					break;
				case 'F':
					usefs = 1;
					break;
				case 'd':
					k = -1;
					if(i+1<argc) {
						diskf = strdup(argv[++i]);
						usefs = 1;
					}
					break;
				case 'p':
					k = -1;
					if(i+1 < argc) {
						listenportnumber = atoi(argv[i+1]);
						if(listenportnumber < 1 || listenportnumber > 65535) {
							isilog(L_CRIT, "Invalid port number %d\n", listenportnumber);
							return 1;
						}
						i++;
					}
					break;
				case 'e':
					loadendian = 2;
					break;
				case 'E':
					k = -1;
					if(i+1<argc) {
						endf = strdup(argv[++i]);
					}
					break;
				case 's':
					flagsvr = 1;
					break;
				case 'D':
					flagdbg = 1;
					break;
				case 'R':
					k = -1;
					if(i+1<argc) {
						redisaddr = strdup(argv[++i]);
					}
					break;
				case 'B':
					k = -1;
					if(i+1<argc) {
						binf = strdup(argv[++i]);
					}
					break;
				}
			}
			break;
		default:
			isilog(L_ERR, "\"%s\" Ignored\n", argv[i]);
			break;
		}
	}
	return 0;
}

void open_log()
{
	int lf;
	if( (lf = open("/var/log/isi.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) > -1 ) {
		fdlog = lf;
	} else if( (lf = open("/var/lib/isicpu/isi.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) > -1 ) {
		fdlog = lf;
	} else if( (lf = open("isi.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) > -1 ) {
		fdlog = lf;
		isilog(L_WARN, "local dir log file\n");
	} else fdlog = -1;
}

void enter_service()
{
	pid_t pid, sid;
	pid = fork();
	if(pid < 0) {
		isilogerr("fork");
		exit(1);
	}
	if(pid > 0) {
		isilog(L_INFO, "entering service\n");
		exit(0);
	}
	umask(0);
	open_log();
	/*
	if( (chdir("/")) < 0 ) {
		isilogerr("chdir");
	}*/
	sid = setsid();
	if(sid < 0) {
		isilogerr("setsid");
		exit(1);
	}
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

int main(int argc, char**argv, char**envp)
{
	uint32_t cux;
	uintptr_t gccq = 0;
	uintptr_t lucycles = 0;
	uintptr_t lucpus = 0;
	struct stats sts = {0, };
	int paddlimit = 0;
	int premlimit = 0;

	isi_init_contable();
	isi_register_objects();
	isi_objtable_init();
	isi_init_sestable();
	isi_inittable(&alldev);
	isi_inittable(&allcpu);
	isi_synctable_init();
	int i;
	uint32_t k;

	if( argc > 1 ) {
		i = parse_args(argc, argv);
		if(i) return i;
	} else {
		fprintf(stderr, gsc_usage, argv[0], argv[0]);
		return 0;
	}

	if(endf) {
		unsigned char * mflip;
		uint32_t rsize;
		loadbinfile(endf, 1, &mflip, &rsize);
		savebinfile(endf, 0, mflip, rsize);
		return 0;
	}

	struct sigaction hnler;
	hnler.sa_handler = sysfaulthdl;
	hnler.sa_flags = 0;

	if(redisaddr) {
		redis_make_session_lu(redisaddr);
	}
	if(!rqrun && !flagsvr) {
		fprintf(stderr, "At least -r or -s must be specified.\n");
		return 0;
	}

	if(flagsvr) {
		if(!rqrun) enter_service();
		makeserver(listenportnumber);
	}
	if(rqrun) {
		make_interactive();
		isi_addcpu();
	}

	sigaction(SIGINT, &hnler, NULL);
	sigaction(SIGTERM, &hnler, NULL);
	sigaction(SIGHUP, &hnler, NULL);
	sigaction(SIGPIPE, &hnler, NULL);

	fetchtime(&LTUTime);
	lucycles = 0; // how many cycles ran (debugging)
	cux = 0; // CPU index - currently never changes
	if(rqrun && allcpu.count && ((struct isiCPUInfo*)allcpu.table[cux])->ctl & ISICTL_DEBUG) {
		showdiag_dcpu(allcpu.table[cux], 1);
	}
	while(!haltnow) {
		struct isiCPUInfo * ccpu;
		struct isiInfo * ccpi;
		struct timespec CRun;

		ccpu = (struct isiCPUInfo*)(ccpi = allcpu.table[cux]);
		fetchtime(&CRun);

		if(!allcpu.count) {
			struct timespec stime = {0, 10000};
			nanosleep(&stime, NULL);
		} else {
			int ccq = 0;
			int tcc = numberofcpus * 2;
			if(tcc < 20) tcc = 20;
			while(ccq < tcc && isi_time_lt(&ccpi->nrun, &CRun)) {
				sts.quanta++;
				ccpi->c->RunCycles(ccpi, CRun);
				//TODO some hardware may need to work at the same time
				lucycles += ccpu->cycl;
				if(rqrun && (ccpu->ctl & ISICTL_TRACE) && (ccpu->cycl)) {
					showdiag_dcpu(ccpi, 1);
				}
				ccpu->cycl = 0;
				fetchtime(&CRun);
				ccq++;
			}
			if(ccq >= tcc) gccq++;
			sts.cpusched++;
			fetchtime(&CRun);

			if(ccpi->updev.t && ccpi->updev.t->c->RunCycles) {
				ccpi->updev.t->c->RunCycles(ccpi->updev.t, CRun);
			}
		}
		fetchtime(&CRun);
		isi_run_sync(CRun);

		fetchtime(&CRun);
		if(CRun.tv_sec > LTUTime.tv_sec) { // roughly one second between status output
			if(rqrun && !flagdbg) { // interactive diag
			double clkdelta;
			float clkrate;
			if(allcpu.count) showdiag_dcpu(allcpu.table[0], 0);
			clkdelta = ((double)(CRun.tv_sec - LTUTime.tv_sec)) + (((double)CRun.tv_nsec) * 0.000000001);
			clkdelta-=(((double)LTUTime.tv_nsec) * 0.000000001);
			if(!lucpus) lucpus = 1;
			clkrate = ((((double)lucycles) * clkdelta) * 0.001) / numberofcpus;
			fprintf(stderr, "DC: %.4f sec, %d at % 9.3f kHz   (% 8ld) [Q:% 8d, S:% 8d, SC:% 8d]\r",
					clkdelta, numberofcpus, clkrate, gccq,
					sts.quanta, sts.cpusched, sts.cpusched / numberofcpus
					);
			if(allcpu.count) showdiag_up(4);
			}
			fetchtime(&LTUTime);
			if(gccq >= sts.cpusched / numberofcpus ) {
				if(numberofcpus > softcpumin) {
					numberofcpus--;
					if(rqrun) fprintf(stderr, "TODO: Offline a CPU\n");
					premlimit--;
					paddlimit = 0;
				}
			} else {
				if(numberofcpus < softcpumax) {
					//fetchtime(&allcpus[numberofcpus].nrun);
					//isi_addtime(&allcpus[numberofcpus].nrun, quantum);
					numberofcpus++;
					if(rqrun) fprintf(stderr, "TODO: Online a CPU\n");
				}
			}
			lucycles = 0;
			lucpus = 0;
			gccq = 0;
			memset(&sts, 0, sizeof(struct stats));
			if(premlimit < 20) premlimit+=10;
			if(paddlimit < 20) paddlimit+=2;
			for(k = 0; k < allses.count; k++) {
				struct isiSession *ses = allses.table[k];
				if(ses->LTick)
					ses->LTick(ses, CRun);
			}
		}

		if(allses.pcount != allses.count) {
			if(allses.ptable) {
				free(allses.ptable);
				allses.ptable = 0;
			}
			allses.pcount = allses.count;
			allses.ptable = (struct pollfd*)isi_alloc(sizeof(struct pollfd) * allses.pcount);
			if(!allses.ptable) {
				isilogerr("malloc poll table fails!");
			}
			i = 0;
			for(k = 0; k < allses.count; k++) {
				allses.ptable[i].fd = allses.table[k]->sfd;
				allses.ptable[i].events = POLLIN;
				i++;
			}
		}
		if(allses.ptable) {
			i = poll(allses.ptable, allses.pcount, 0);
		} else {
			i = 0;
		}
		for(k = 0; k < allses.count; k++) {
			struct isiSession *ses = allses.table[k];
			if(ses && ses->STick && (ses->cmdqstart != ses->cmdqend))
				ses->STick(ses, CRun);
		}
		if(i > 0) {
			for(k = 0; k < allses.pcount; k++) {
				if(!allses.ptable[k].revents) continue;
				struct isiSession *ses = allses.table[k];
				const char *etxt = 0;
				/* Here be dragons */
				switch(allses.ptable[k].revents) {
				case POLLERR: etxt = "poll: FD error\n"; goto sessionerror;
				case POLLHUP: etxt = "poll: FD hangup\n"; goto sessionerror;
				case POLLNVAL: etxt = "poll: FD invalid\n"; goto sessionerror;
				default:
					if(allses.ptable[k].revents & POLLIN) {
						if(ses->sfd == allses.ptable[k].fd) {
							if(ses->Recv(ses, CRun))
								goto sessionerror;
						} else {
							isilog(L_ERR, "netses: session ID error\n");
						}
					}
				}
				continue;
sessionerror:
				if(etxt) isilog(L_ERR, etxt);
				if(ses->sfd == allses.ptable[k].fd) {
					isi_delete_ses(ses);
					k = allses.pcount;
				}
			}
		}
		if(!flagdbg) {
			cux++;
			if(!(cux < allcpu.count)) {
				cux = 0;
			}
		} else {
			numberofcpus = 1;
			cux = 0;
		}
		if(haltnow) {
			break;
		}
	}
	if(flagsvr) {
		isilog(L_WARN, "closing connections\n");
		while(allses.count) {
			isi_delete_ses(allses.table[0]);
		}
	}
	while(allobj.count) {
		isi_delete_object(allobj.table[0]);
	}
	if(rqrun) printf("\n\n\n\n");
	return 0;
}

