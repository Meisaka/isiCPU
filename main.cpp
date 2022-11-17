
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string>
#include <vector>
#include "dcpuhw.h"
#include "isitypes.h"
#include "netmsg.h"
#include "platform.h"

static isi_time_t LTUTime;
static uint32_t numberofcpus = 1;
static int haltnow = 0;
static int loadendian = 0;
static int flagdbg = 0;
static int rqrun = 0;
static int flagsvr = 0;
static std::string binf;
static std::string endf;
static std::string redisaddr;
static std::string diskf;
static int listenportnumber = 58704;
static int verblevel = L_INFO;
static uintptr_t gccq = 0;

fdesc_t fdlog = STDERR_FILENO;
int usefs = 0;

static const int quantum = 1000000000 / 10000; // ns

enum {
	CPUSMAX = 800,
	CPUSMIN = 20
};

extern std::vector<isiInfo*> allcpu;
extern std::vector<isiObjSlot> allobj;
extern std::vector<isiSession*> allses;

std::vector<struct pollfd> polltable;

void isi_run_sync(isi_time_t crun);
void isi_register_objects();
int isi_scan_dir();
int isi_delete_ses(isiSession *s);
int makeserver(int portnumber);
int redis_make_session_lu(const char * addrstr);
void showdisasm_dcpu(const isiInfo *info);
void showdiag_dcpu(const isiInfo* info, int fmt);

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
void isilogneterr(const char * desc) {
	int r;
#ifdef WIN32
	r = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
		WSAGetLastError(), 0,
		logwrout, ISILOGBUFFER,
		nullptr);
#else
	r = snprintf(logwrout, ISILOGBUFFER, "Error: %s: %s\n", desc, strerror(errno));
#endif
	if(r > 0) {
		write(fdlog, logwrout, r);
	}
}
void isilogerr(const char * desc) {
	int r;
	int ec = errno;
#ifdef WIN32
	if(ec == 0) {
		ec = WSAGetLastError();
	}
#endif
	r = snprintf(logwrout, ISILOGBUFFER, "Error: %s: %s\n", desc, strerror(ec));
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

#ifndef _MSC_VER
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
#endif

void showdiag_up(int l)
{
	fprintf(stderr, "\x1b[%dA", l);
}

void isi_add_time(isi_time_t *t, size_t nsec) {
	*t += nsec;
}

int isi_time_lt(isi_time_t const *a, isi_time_t const *b) {
	return (*a < *b);
}

void isi_setrate(isiCPUInfo *info, size_t rate) {
	info->runrate = 1000000000 / rate; // Nano seconds per cycle (100kHz)
	if(info->runrate > quantum) {
		info->itvl = (info->runrate / quantum); // quantums per cycle
		info->rate = 1;
	} else {
		info->rate = (quantum / info->runrate); // cycles per quantum
		info->itvl = 1;
	}
}

int add_dcpu_prefab() {
	isiInfo *bus, *ninfo;
	isiCPUInfo *cpu = nullptr;
	isiMemory *nmem;
	isi_make_object(isi_lookup_name("dcpu"), (isiObject**)&cpu, 0, 0);
	if(!cpu) return -1;
	cpu->ctl = flagdbg ? (ISICTL_DEBUG | ISICTL_TRACE | ISICTL_STEP) : 0;
	isi_make_object(isi_lookup_name("memory_64kx16"), (isiObject**)&nmem, 0, 0);
	isi_make_object(isi_lookup_name("dcpu_hwbus"), (isiObject**)&bus, 0, 0);
	isi_attach(bus, 0, (isiInfo*)nmem, ISIAT_APPEND, 0, 0);
	isi_attach(bus, 0, (isiInfo*)cpu, ISIAT_UP, 0, 0);
	isi_make_object(isi_lookup_name("tcm_nya_lem"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("txc_gen_clock"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("trk_gen_speaker"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("tcm_gen_keyboard"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	isi_make_object(isi_lookup_name("tcm_kai_hic32"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);
	if(!binf.empty()) {
		uint8_t ist[24];
		ist[0] = 0;
		uint64_t id = 0;
		isi_fname_id(binf.c_str(), &id);
		isi_write_parameter(ist, 24, 2, &id, sizeof(uint64_t));
		if(loadendian) isi_write_parameter(ist, 24, 3, &id, 0);
		isi_make_object(isi_lookup_name("trk_gen_eprom"), (isiObject**)&ninfo, ist, 24);
		isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);
	}

	isi_make_object(isi_lookup_name("txc_mack_35fd"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);
	if(!diskf.empty()) {
		uint8_t ist[24];
		ist[0] = 0;
		uint64_t dsk = 0;
		isiInfo *ndsk;
		isi_fname_id(diskf.c_str(), &dsk);
		isi_write_parameter(ist, 24, 1, &dsk, sizeof(uint64_t));
		isi_make_object(isi_lookup_name("disk"), (isiObject**)&ndsk, ist, 24);
		isi_attach(ninfo, 0, ndsk, ISIAT_UP, 0, 0);
	}
	isi_make_object(isi_lookup_name("trk_mei_imva"), (isiObject**)&ninfo, 0, 0);
	isi_attach(bus, ISIAT_APPEND, ninfo, ISIAT_UP, 0, 0);

	return 0;
}

void isi_debug_dump_objtable() {
	uint32_t u = 0;
	while(u < allobj.size()) {
		if(allobj[u].ptr)
			fprintf(stderr, "obj-list: [%08x]: %x\n", allobj[u].ptr->id, allobj[u].ptr->otype);
		u++;
	}
}

void isi_debug_dump_cputable() {
	uint32_t u = 0;
	while(u < allcpu.size()) {
		fprintf(stderr, "cpu-list: [%08x]: %x\n", allcpu[u]->id, allcpu[u]->otype);
		u++;
	}
}
void isi_redis_test();

class isiSessionTTY : public isiSession {
	virtual int Recv(isi_time_t mtime);
	virtual int STick(isi_time_t mtime) { return 0; }
	virtual int LTick(isi_time_t mtime) { return 0; }
	virtual int AsyncDone(isiCommand *cmd, int result) { return 0; }
};
static isiClass<isiSessionTTY> isiSessionTTY_C(ISIT_SESSION_TTY, "<isiSessionTTY>", "");

int isiSessionTTY::Recv(isi_time_t mtime)
{
	int i;
	char cc[16];
	i = read(this->sfd, cc, 16);
	if(i < 1) return 0;
	isiCPUInfo *cpu = NULL;
	if(allcpu.size()) {
		cpu = (isiCPUInfo*)allcpu[0];
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
			ti = allcpu[0]->mem->d_rd(t);
			fprintf(stderr, "\x1b[Kdebug: read %04x:%04x\n", t, ti);
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
			allcpu[0]->mem->togglebrk(t);
			k = allcpu[0]->mem->isbrk(t);
			fprintf(stderr, "\x1b[Kdebug: Break point %sabled at %04x\n", k?"en":"dis",t);
		}
		break;
	case 'x':
		haltnow = 1;
		break;
	case 't':
		if(!cpu) break;
		cpu->ctl ^= ISICTL_TRACEC;
		fprintf(stderr, "\x1b[Kdebug: trace on continue %sabled\n", (cpu->ctl & ISICTL_TRACEC)?"en":"dis");
		break;
	case 'n':
		if(allcpu.size()) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_synctable();
		break;
	case 'L':
		isi_redis_test();
		break;
	case 'l':
		if(allcpu.size()) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_objtable();
		break;
	case 'p':
		if(allcpu.size()) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_cputable();
		break;
	default:
		break;
	}
	return 0;
}

int make_interactive() {
	isiSession *ses;
	int i;
	if((i= isi_create_object(ISIT_SESSION_TTY, NULL, (isiObject**)&ses))) {
		return i;
	}
	if(set_nonblock(STDIN_FILENO) < 0) {
		isilogerr("fcntl");
		return -1;
	}
	ses->sfd = STDIN_FILENO;
	// TODO tty session class!
	allses.push_back(ses);
	return 0;
}

struct stats {
	int quanta;
	int cpusched;
};

void platform_test();

struct stats allstats = {0, };
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
					verblevel = L_DEBUG;
					isi_scan_dir();
					return 1;
				case 'F':
					usefs = 1;
					break;
				case 'd':
					k = -1;
					if(i+1<argc) {
						diskf = argv[++i];
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
						endf = argv[++i];
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
						redisaddr = argv[++i];
					}
					break;
				case 'B':
					k = -1;
					if(i+1<argc) {
						binf = argv[++i];
					}
					break;
				case 'Q':
					platform_test();
					return 1;
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
	fdesc_t lf;
	if( (lf = open("/var/log/isi.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) != fdesc_empty ) {
		fdlog = lf;
	} else if( (lf = open("/var/lib/isicpu/isi.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) != fdesc_empty ) {
		fdlog = lf;
	} else if( (lf = open("isi.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) != fdesc_empty ) {
		fdlog = lf;
		isilog(L_WARN, "local dir log file\n");
	} else fdlog = fdesc_empty;
}

static size_t isi_run_cpu(uint32_t cpunumber)
{
	isiCPUInfo * ccpu;
	isiInfo * ccpi;
	ccpu = (isiCPUInfo*)(ccpi = allcpu[cpunumber]);
	isi_time_t CRun;
	int ccq = 0;
	int tcc = numberofcpus * 2;
	size_t acccycles = 0;
	if(tcc < 20) tcc = 20;
	isi_fetch_time(&CRun);
	while(ccq < tcc && isi_time_lt(&ccpi->nrun, &CRun)) {
		allstats.quanta++;
		ccpi->Run(CRun);
		//TODO some hardware may need to work at the same time
		acccycles += ccpu->cycl;
		if(rqrun && (ccpu->ctl & ISICTL_TRACE) && (ccpu->cycl)) {
			showdiag_dcpu(ccpi, 1);
		}
		ccpu->cycl = 0;
		ccq++;
	}
	if(ccq >= tcc) gccq++;
	allstats.cpusched++;
	isi_fetch_time(&CRun);

	if(ccpi->updev.t) {
		ccpi->updev.t->Run(CRun);
	}
	return acccycles;
}
void isi_register_server();
void isi_register_redis();
void cemei_register();
void isi_register_netsync();

int main(int argc, char**argv, char**envp)
{
	int i;
	i = platform_startup();
	if(i) return i;
	uint32_t cux;
	uintptr_t lucycles = 0;
	int paddlimit = 0;
	int premlimit = 0;

	isi_register_server();
	isi_register(&isiSessionTTY_C);
	isi_register_redis();
	cemei_register();
	isi_register_netsync();
	isi_register_objects();
	uint32_t k;

	if( argc > 1 ) {
		i = parse_args(argc, argv);
		if(i) return i;
	} else {
		fprintf(stderr, gsc_usage, argv[0], argv[0]);
		return 0;
	}

	if(!endf.empty()) {
		unsigned char * mflip;
		uint32_t rsize;
		loadbinfile(endf.c_str(), 1, &mflip, &rsize);
		savebinfile(endf.c_str(), 0, mflip, rsize);
		return 0;
	}

#ifndef WIN32
	struct sigaction hnler;
	hnler.sa_handler = sysfaulthdl;
	hnler.sa_flags = 0;
#endif

	if(!rqrun && !flagsvr) {
		fprintf(stderr, "At least -r or -s must be specified.\n");
		return 0;
	}

	if(!redisaddr.empty()) {
		redis_make_session_lu(redisaddr.c_str());
	}

	if(flagsvr) {
		if(!rqrun) enter_service();
		makeserver(listenportnumber);
	}
	if(rqrun) {
		make_interactive();
		add_dcpu_prefab();
	}

#ifndef WIN32
	sigaction(SIGINT, &hnler, NULL);
	sigaction(SIGTERM, &hnler, NULL);
	sigaction(SIGHUP, &hnler, NULL);
	sigaction(SIGPIPE, &hnler, NULL);
#endif

	isi_fetch_time(&LTUTime);
	lucycles = 0; // how many cycles ran (debugging)
	cux = 0; // CPU index - currently never changes
	if(rqrun && allcpu.size() && ((isiCPUInfo*)allcpu[cux])->ctl & ISICTL_DEBUG) {
		showdiag_dcpu(allcpu[cux], 1);
	}
	while(!haltnow) {
		isi_time_t CRun;

		isi_fetch_time(&CRun);

		if(!allcpu.size()) {
			sleep(0);
		} else {
			lucycles += isi_run_cpu(cux);
		}
		isi_fetch_time(&CRun);
		isi_run_sync(CRun);

		isi_fetch_time(&CRun);
		if(CRun > LTUTime) { // one second between status output
			if(rqrun && !flagdbg) { // interactive diag
			double clkdelta;
			double clkrate;
			if(allcpu.size()) showdiag_dcpu(allcpu[0], 0);
			clkdelta = 1.0 + ((double)(CRun - LTUTime)) * 0.000000001;
			clkrate = ((((double)lucycles) * clkdelta) * 0.001) / numberofcpus;
			fprintf(stderr, "DC: %.5f sec, n=%d CPUs at % 9.3lf kHz avg (% 8lld) [Q:% 8d, S:% 8d, S/n:% 8d]\r",
					clkdelta, numberofcpus, clkrate, gccq,
					allstats.quanta, allstats.cpusched, allstats.cpusched / numberofcpus
					);
			if(allcpu.size()) showdiag_up(4);
			}
			isi_add_time(&LTUTime, 1000000000);
			lucycles = 0;
			gccq = 0;
			memset(&allstats, 0, sizeof(struct stats));
			if(premlimit < 20) premlimit+=10;
			if(paddlimit < 20) paddlimit+=2;
			for(k = 0; k < allses.size(); k++) {
				allses[k]->LTick(CRun);
			}
		}

		if(polltable.size() != allses.size()) {
			polltable.resize(allses.size());
			i = 0;
			for(k = 0; k < allses.size(); k++) {
				polltable[i].fd = allses[k]->sfd;
				polltable[i].events = POLLIN;
				i++;
			}
		}
		if(polltable.size() > 0) {
			i = poll(&polltable[0], polltable.size(), 0);
		} else {
			i = 0;
		}
		for(auto ses : allses) {
			if(ses && (ses->cmdqstart != ses->cmdqend))
				ses->STick(CRun);
		}
		if(i > 0) {
			for(k = 0; k < polltable.size(); k++) {
				int ev = polltable[k].revents;
				if(!ev) continue;
				isiSession *ses = allses[k];
				const char *etxt = 0;
				/* Here be dragons */
				if(ev & POLLERR) { etxt = "poll: FD error\n"; goto sessionerror; }
				if(ev & POLLHUP) { etxt = "poll: FD hangup\n"; goto sessionerror; }
				if(ev & POLLNVAL) { etxt = "poll: FD invalid\n"; goto sessionerror; }
				if(ev & POLLIN) {
					if(ses->sfd == polltable[k].fd) {
						if(ses->Recv(CRun))
							goto sessionerror;
					} else {
						isilog(L_ERR, "netses: session ID error\n");
					}
				}
				continue;
sessionerror:
				if(etxt) isilog(L_ERR, etxt);
				if(ses->sfd == polltable[k].fd) {
					isi_delete_ses(ses);
					break;
				}
			}
		}
		if(!flagdbg) {
			cux++;
			if(!(cux < allcpu.size())) {
				cux = 0;
			}
			numberofcpus = allcpu.size();
			if(numberofcpus < 1) numberofcpus = 1;
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
		while(allses.size()) {
			isi_delete_ses(allses[0]);
		}
	}
	while(allobj.size()) {
		size_t i;
		for(i = 0; i < allobj.size(); i++) {
			if(allobj[i].ptr) {
				isi_delete_object(allobj[i].ptr);
			}
		}
	}
	if(rqrun) printf("\n\n\n\n");
	return 0;
}

