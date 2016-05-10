
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <poll.h>
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
static int haltnow = 0;
static int loadendian = 0;
static int flagdbg = 0;
static int rqrun = 0;
static int flagsvr = 0;
static char * binf = 0;
static int listenportnumber = 58704;

static int fdlisten = -1;

static const int quantum = 1000000000 / 10000; // ns

enum {
	CPUSMAX = 800,
	CPUSMIN = 20
};
static uint32_t numberofcpus = 1;
static uint32_t softcpumax = 1;
static uint32_t softcpumin = 1;

static struct isiDevTable alldev;
static struct isiDevTable allcpu;

uint32_t maxsid = 0;
struct isiObjTable allobj;
struct isiSessionTable allses;

void isi_run_sync(struct timespec crun);

static const char * const gsc_usage =
"Usage:\n%s [-Desrm] [-p <portnum>] [-B <binfile>]\n\n"
"Options:\n"
" -e  Assume <binfile> is little-endian\n"
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

	memset(&lipn, 0, sizeof(lipn));
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
	fdlisten = fdsvr;
	return 0;
}

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize)
{
	int fd, i;
	struct stat fdi;
	size_t f, o;
	uint16_t eswp;
	uint8_t *mem;
	fd = open(path, O_RDONLY);
	if(fd < 0) { perror("open"); return -5; }
	if(fstat(fd, &fdi)) { perror("fstat"); close(fd); return -3; }
	f = fdi.st_size & (~1);
	if(f > 0x20000) f = 0x20000;
	mem = (uint8_t*)malloc(f);
	if(!mem) { close(fd); return -5; }
	o = 0;
	while((i = read(fd, mem+o, f - o) > 0) && o < f) {
		o += i;
	}
	if( i < 0 ) {
		perror("read");
		close(fd);
		return -1;
	}
	close(fd);

	o = 0;
	if(endian) {
		while(o < f) {
			eswp = *(uint16_t*)(mem+o);
			*(uint16_t*)(mem+o) = (eswp >> 8) | (eswp << 8);
			o += 2;
		}
	}
	*nmem = mem;
	if(nsize) *nsize = f;
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
	} else if(ssn == SIGPIPE) {
		fprintf(stderr, "SOCKET SIGNALED!\n");
	} else {
		fprintf(stderr, "Unknown signal\n");
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

void showdisasm_dcpu(const struct isiInfo *info)
{
	const DCPU * cpu = (const DCPU*)info->rvstate;
	uint16_t ma = cpu->PC;
	uint16_t m = (cpu->memptr)->ram[ma++];
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
		nwa = (cpu->memptr)->ram[ma++];
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
			nwb = (cpu->memptr)->ram[ma++];
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
		fprintf(stderr, "  b: %04x [%04x]", lub, cpu->memptr->ram[lub]);
	}
	if(lu & 1) {
		fprintf(stderr, "  a: %04x [%04x]", lua, cpu->memptr->ram[lua]);
	}
	fprintf(stderr, "\n");
}

void showdiag_dcpu(const struct isiInfo* info, int fmt)
{
	const struct isiCPUInfo *l_info = (const struct isiCPUInfo *)info;
	const DCPU * cpu = (const DCPU*)info->rvstate;
	const char *diagt;
	diagt = fmt ? DIAG_L2 : DIAG_L4;
	fprintf(stderr, diagt,
	cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3],cpu->R[4],cpu->R[5],
	cpu->R[6],cpu->R[7],cpu->PC,cpu->SP,cpu->EX,cpu->IA, l_info->cycl);
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

int isi_attach(struct isiInfo *item, struct isiInfo *dev)
{
	if(!item || !dev) return -1;
	if(item->id.objtype >= ISIT_BUSDEV && item->id.objtype < ISIT_ENDBUSDEV) {
	} else {
		item->outdev = dev;
	}
	dev->outdev = item;
	if(item->id.objtype >= ISIT_CPU && item->id.objtype < ISIT_ENDCPU) {
		dev->hostcpu = item;
		dev->mem = item->mem;
	} else {
		dev->hostcpu = item->hostcpu;
		dev->mem = item->mem;
	}
	if(item->Attach) item->Attach(item, dev);
	if(dev->Attached) dev->Attached(dev, item);
	if(dev->id.objtype >= ISIT_BUSDEV && dev->id.objtype < ISIT_ENDBUSDEV) {
		size_t k;
		size_t hs;
		struct isiBusInfo *bus = (struct isiBusInfo*)dev;
		hs = bus->busdev.count;
		for(k = 0; k < hs; k++) {
			if(bus->busdev.table[k]) {
				bus->busdev.table[k]->mem = dev->mem;
				bus->busdev.table[k]->hostcpu = dev->hostcpu;
			}
		}
	}
	return 0;
}

void isi_objtable_init()
{
	allobj.limit = 256;
	allobj.count = 0;
	allobj.table = (struct objtype**)malloc(allobj.limit * sizeof(void*));
}

int isi_objtable_add(struct objtype *obj)
{
	if(!obj) return -1;
	void *n;
	if(allobj.count >= allobj.limit) {
		n = realloc(allobj.table, (allobj.limit + allobj.limit) * sizeof(void*));
		if(!n) return -5;
		allobj.limit += allobj.limit;
		allobj.table = (struct objtype**)n;
	}
	allobj.table[allobj.count++] = obj;
	return 0;
}

int isi_find_obj(uint32_t id, struct objtype **target)
{
	size_t i;
	if(!id) return -1;
	for(i = 0; i < allobj.count; i++) {
		struct objtype *obj = allobj.table[i];
		if(obj && obj->id == id) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_get_type_size(int objtype, size_t *sz)
{
	size_t objsize = 0;
	if( (objtype >> 12) > 2 ) objtype &= ~0xfff;
	switch(objtype) {
	case ISIT_NONE: return -1;
	case ISIT_SESSION: objsize = sizeof(struct isiSession); break;
	case ISIT_NETSYNC: objsize = sizeof(struct isiNetSync); break;
	case ISIT_MEM6416: objsize = sizeof(struct memory64x16); break;
	case ISIT_CPU: objsize = sizeof(struct isiCPUInfo); break;
	case ISIT_BUSDEV: objsize = sizeof(struct isiBusInfo); break;
	case ISIT_DCPUHW: objsize = sizeof(struct isiInfo); break;
	}
	if(objsize) {
		*sz = objsize;
		return 0;
	}
	return -1;
}

int isi_create_object(int objtype, struct objtype **out)
{
	if(!out) return -1;
	struct objtype *ns;
	size_t objsize = 0;
	if(isi_get_type_size(objtype, &objsize)) return -2;
	ns = (struct objtype*)malloc(objsize);
	if(!ns) return -5;
	memset(ns, 0, objsize);
	ns->id = ++maxsid; // TODO make "better" ID numbers?
	ns->objtype = objtype;
	isi_objtable_add(ns);
	*out = ns;
	return 0;
}

int isi_delete_object(struct objtype *obj)
{
	if(!obj) return -1;
	struct isiObjTable *t = &allobj;
	free(obj);
	uint32_t i;
	for(i = 0; i < t->count; i++) {
		if(t->table[i] == obj) break;
	}
	if(i < t->count) t->count--; else return -1;
	while(i < t->count) {
		t->table[i] = t->table[i+1];
		i++;
	}
	return 0;
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

int isi_inittable(struct isiDevTable *t)
{
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiInfo**)malloc(t->limit * sizeof(void*));
	return 0;
}

void isi_init_sestable()
{
	struct isiSessionTable *t = &allses;
	t->limit = 32;
	t->count = 0;
	t->pcount = 0;
	t->table = (struct isiSession**)malloc(t->limit * sizeof(void*));
	t->ptable = 0;
}

int isi_pushses(struct isiSession *s)
{
	if(!s) return -1;
	struct isiSessionTable *t = &allses;
	void *n;
	if(t->count >= t->limit) {
		n = realloc(t->table, (t->limit + t->limit) * sizeof(void*));
		if(!n) return -5;
		t->limit += t->limit;
		t->table = (struct isiSession**)n;
	}
	t->table[t->count++] = s;
	return 0;
}

int isi_delete_ses(struct isiSession *s)
{
	if(!s) return -1;
	struct isiSessionTable *t = &allses;
	uint32_t i;
	for(i = 0; i < t->count; i++) {
		if(t->table[i] == s) break;
	}
	if(i < t->count) t->count--; else return -1;
	while(i < t->count) {
		t->table[i] = t->table[i+1];
		i++;
	}
	free(s->in);
	free(s->out);
	isi_delete_object(&s->id);
	return 0;
}

int isi_pushdev(struct isiDevTable *t, struct isiInfo *d)
{
	if(!d) return -1;
	void *n;
	if(!t->limit || !t->table) isi_inittable(t);
	if(t->count >= t->limit) {
		n = realloc(t->table, (t->limit + t->limit) * sizeof(void*));
		if(!n) return -5;
		t->limit += t->limit;
		t->table = (struct isiInfo**)n;
	}
	t->table[t->count++] = d;
	return 0;
}

int isi_createdev(struct isiInfo **ndev)
{
	return isi_create_object(ISIT_DCPUHW, (struct objtype**)ndev);
}

int isi_createcpu(struct isiCPUInfo **ndev)
{
	return isi_create_object(ISIT_CPU, (struct objtype**)ndev);
}

int isi_addcpu(struct isiCPUInfo *cpu, const char *cfg)
{
	struct isiInfo *bus, *ninfo;
	isi_setrate(cpu, 100000); // 100kHz
	cpu->ctl = flagdbg ? (ISICTL_DEBUG | ISICTL_STEP) : 0;
	isiram16 nmem;
	isi_create_object(ISIT_MEM6416, (struct objtype**)&nmem);
	DCPU_init((struct isiInfo*)cpu, nmem);
	isi_create_object(ISIT_BUSDEV, (struct objtype**)&bus);
	HWM_CreateBus(bus);
	isi_createdev(&ninfo);
	HWM_CreateDevice(ninfo, "nya_lem");
	isi_attach(bus, ninfo);

	isi_createdev(&ninfo);
	HWM_CreateDevice(ninfo, "clock");
	isi_attach(bus, ninfo);

	isi_createdev(&ninfo);
	HWM_CreateDevice(ninfo, "keyboard");
	isi_attach(bus, ninfo);
	uint8_t *rom;
	uint32_t rsize = 0;
	if(binf) {
		loadbinfile(binf, loadendian, &rom, &rsize);
		char *ist;
		asprintf(&ist, "rom:size=%u", rsize);
		isi_createdev(&ninfo);
		HWM_CreateDevice(ninfo, ist);
		memcpy(((char*)ninfo->rvstate)+2, rom, rsize);
		*((uint16_t*)ninfo->rvstate) = rsize;
		isi_attach(bus, ninfo);
		free(ist);
		free(rom);
	}
	isi_attach((struct isiInfo*)cpu, bus);
	return 0;
}

void handle_stdin()
{
	int i;
	uint32_t u;
	char cc;
	char ccv[10];
	i = read(0, &cc, 1);
	if(i < 1) return;
	switch(cc) {
	case 10:
		((struct isiCPUInfo*)allcpu.table[0])->ctl |= ISICTL_STEPE;
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
			ti = ((isiram16)allcpu.table[0]->mem)->ram[t];
			fprintf(stderr, "READ %04x:%04x\n", t, ti);
		}
		break;
	case 'x':
		haltnow = 1;
		break;
	case 'n':
		fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_synctable();
		break;
	case 'l':
		fprintf(stderr, "\n\n\n\n");
		u = 0;
		while(u < allobj.count) {
			fprintf(stderr, "obj-list: [%08x]: %x\n", allobj.table[u]->id, allobj.table[u]->objtype);
			u++;
		}
		break;
	default:
		break;
	}
}

int handle_newsessions()
{
	int fdn, i;
	socklen_t rin;
	struct sockaddr_in ripn;
	memset(&ripn, 0, sizeof(ripn));
	rin = sizeof(ripn);
	fdn = accept(fdlisten, (struct sockaddr*)&ripn, &rin);
	if(fdn < 0) {
		perror("accept");
		fdn = 0;
		return -1;
	}
	i = 1;
	if( setsockopt(fdn, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		perror("set'opt");
		close(fdn);
		return -1;
	}
	if( fcntl(fdn, F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl");
		close(fdn);
		return -1;
	}
	struct isiSession *ses;
	if((i= isi_create_object(ISIT_SESSION, (struct objtype **)&ses))) {
		return i;
	}
	ses->in = (uint8_t*)malloc(8192);
	ses->out = (uint8_t*)malloc(2048);
	ses->sfd = fdn;
	memcpy(&ses->r_addr, &ripn, sizeof(struct sockaddr_in));
	isi_pushses(ses);
	return 0;
}

int session_write(struct isiSession *ses, int len)
{
	return send(ses->sfd, ses->out, len, 0);
}

int session_write_msg(struct isiSession *ses)
{
	int len;
	len = (*(uint32_t*)(ses->out)) & 0xfffff;
	if(len > 1300) {
		len = 1300;
		*(uint32_t*)(ses->out) = ((*(uint32_t*)ses->out) & 0xfff00000) | len;
	}
	return send(ses->sfd, ses->out, 4+len, 0);
}

int session_write_msgex(struct isiSession *ses, void *buf)
{
	int len;
	len = (*(uint32_t*)(buf)) & 0xfffff;
	if(len > 1300) {
		len = 1300;
		*(uint32_t*)(buf) = ((*(uint32_t*)buf) & 0xfff00000) | len;
	}
	return send(ses->sfd, (char*)buf, 4+len, 0);
}

int session_write_buf(struct isiSession *ses, void *buf, int len)
{
	return send(ses->sfd, (char*)buf, len, 0);
}

int handle_session_rd(struct isiSession *ses, struct timespec mtime)
{
	int i;
	uint32_t l;
	uint32_t mc;
	if(ses->rcv < 4) {
		i = read(ses->sfd, ses->in+ses->rcv, 4-ses->rcv);
	} else {
readagain:
		l = ((*(uint32_t*)ses->in) & 0xfffff);
		if(ses->rcv < 4+l) {
			i = read(ses->sfd, ses->in+ses->rcv, (4+l)-ses->rcv);
		} else {
			fprintf(stderr, "net-session: improper read\n");
		}
	}
	if(i < 0) { perror("socket read"); return -1; }
	if(i == 0) { fprintf(stderr, "net-session: empty read\n"); return -1; }
	if(ses->rcv < 4) {
		ses->rcv += i;
		if(ses->rcv >= 4) {
			l = ((*(uint32_t*)ses->in) & 0xfffff);
			if(ses->rcv < 4+l) goto readagain;
		}
	}
	ses->rcv += i;
	if(ses->rcv < 4+l) return 0;
	mc = *(uint32_t*)ses->in;
	uint32_t *pm = (uint32_t*)ses->in;
	uint32_t *pr = (uint32_t*)ses->out;
	l = mc & 0xfffff; mc >>= 20;
	/* handle message here */
	switch(mc) {
	case 0x010: /* keepalive/ping */
		break;
	case 0x011: /* get all accessable objects */
	{
		size_t i;
		uint32_t ec = 0;
		fprintf(stderr, "net-msg: [%08x]: list obj\n", ses->id.id);
		for(i = 0; i < allobj.count; i++) {
			struct objtype *obj = allobj.table[i];
			if(obj && obj->objtype >= 0x2000) {
				pr[1+ec] = obj->id;
				pr[2+ec] = obj->objtype;
				ec+=2;
			}
			if(ec > 160) {
				pr[0] = 0x20000000 | (ec*4);
				session_write(ses, 4+(ec*4));
				ec = 0;
			}
		}
		pr[0] = 0x20100000 | (ec*4);
		session_write(ses, 4+(ec*4));
	}
		break;
	case 0x012: /* sync everything! */
		isi_resync_all();
		break;
	case 0x080:
	{
		struct isiInfo *info;
		if(l < 6) {
			fprintf(stderr, "net-msg: [%08x]: short 0x%03x +%05x\n", ses->id.id, mc, l);
			break;
		}
		if(isi_find_obj(pm[1], (struct objtype**)&info)) {
			fprintf(stderr, "net-msg: [%08x]: not found [%08x]\n", ses->id.id, pm[1]);
			break;
		}
		if(info->id.objtype >= 0x2000) {
			if(info->MsgIn) {
				info->MsgIn(info, info->outdev, (uint16_t*)(pm+2), mtime);
			}
		}
	}
		break;
	default:
		fprintf(stderr, "net-session: [%08x]: 0x%03x +%05x\n", ses->id.id, mc, l);
		break;
	}
	/* *** */
	ses->rcv = 0;
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
					loadendian = 2;
					break;
				case 's':
					flagsvr = 1;
					break;
				case 'D':
					flagdbg = 1;
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
			fprintf(stderr, "\"%s\" Ignored\n", argv[i]);
			break;
		}
	}
	return 0;
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
	int extrafds = 0;

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
		fprintf(stderr, gsc_usage, argv[0]);
		return 0;
	}

	struct sigaction hnler;
	hnler.sa_handler = sysfaulthdl;
	hnler.sa_flags = 0;

	if(!rqrun && !flagsvr) {
		fprintf(stderr, "At least -r or -s must be specified.\n");
		return 0;
	}

	for(cux = 0; cux < softcpumax; cux++) {
		struct isiCPUInfo *ncpu;
		isi_createcpu(&ncpu);
		isi_addcpu(ncpu, "");
		isi_pushdev(&allcpu, (struct isiInfo*)ncpu);
	}
	if(flagsvr) {
		makewaitserver();
	}

	sigaction(SIGINT, &hnler, NULL);
	sigaction(SIGPIPE, &hnler, NULL);

	for(cux = 0; cux < allcpu.count; cux++) {
		fetchtime(&allcpu.table[cux]->nrun);
		allcpu.table[cux]->Reset(allcpu.table[cux]);
	}
	fetchtime(&LTUTime);
	lucycles = 0; // how many cycles ran (debugging)
	cux = 0; // CPU index - currently never changes
	if(rqrun && ((struct isiCPUInfo*)allcpu.table[cux])->ctl & ISICTL_DEBUG) {
		showdiag_dcpu(allcpu.table[cux], 1);
	}
	extrafds = (flagsvr?1:0) + (rqrun?1:0);
	while(!haltnow) {
		struct isiCPUInfo * ccpu;
		struct isiInfo * ccpi;
		struct timespec CRun;

		ccpu = (struct isiCPUInfo*)(ccpi = allcpu.table[cux]);
		fetchtime(&CRun);

		{
			int ccq = 0;
			int tcc = numberofcpus * 2;
			if(tcc < 20) tcc = 20;
			while(ccq < tcc && isi_time_lt(&ccpi->nrun, &CRun)) {
				sts.quanta++;
				ccpi->RunCycles(ccpi, CRun);
				//TODO some hardware may need to work at the same time
				lucycles += ccpu->cycl;
				if(rqrun && (ccpu->ctl & ISICTL_DEBUG) && (ccpu->cycl)) {
					showdiag_dcpu(ccpi, 1);
				}
				ccpu->cycl = 0;
				fetchtime(&CRun);
				ccq++;
			}
			if(ccq >= tcc) gccq++;
			sts.cpusched++;
			fetchtime(&CRun);

			if(ccpi->outdev && ccpi->outdev->RunCycles) {
				ccpi->outdev->RunCycles(ccpi->outdev, CRun);
			}
		}
		fetchtime(&CRun);
		isi_run_sync(CRun);

		fetchtime(&CRun);
		if(CRun.tv_sec > LTUTime.tv_sec) { // roughly one second between status output
			if(rqrun) { // interactive diag
			double clkdelta;
			float clkrate;
			if(!flagdbg) showdiag_dcpu(allcpu.table[0], 0);
			clkdelta = ((double)(CRun.tv_sec - LTUTime.tv_sec)) + (((double)CRun.tv_nsec) * 0.000000001);
			clkdelta-=(((double)LTUTime.tv_nsec) * 0.000000001);
			if(!lucpus) lucpus = 1;
			clkrate = ((((double)lucycles) * clkdelta) * 0.001) / numberofcpus;
			fprintf(stderr, "DC: %.4f sec, %d at % 9.3f kHz   (% 8ld) [Q:% 8d, S:% 8d, SC:% 8d]\r",
					clkdelta, numberofcpus, clkrate, gccq,
					sts.quanta, sts.cpusched, sts.cpusched / numberofcpus
					);
			if(!flagdbg) showdiag_up(4);
			}
			fetchtime(&LTUTime);
			if(gccq >= sts.cpusched / numberofcpus ) {
				if(numberofcpus > softcpumin) {
					numberofcpus--;
					fprintf(stderr, "TODO: Offline a CPU\n");
					premlimit--;
					paddlimit = 0;
				}
			} else {
				if(numberofcpus < softcpumax) {
					//fetchtime(&allcpus[numberofcpus].nrun);
					//isi_addtime(&allcpus[numberofcpus].nrun, quantum);
					numberofcpus++;
					fprintf(stderr, "TODO: Online a CPU\n");
				}
			}
			lucycles = 0;
			lucpus = 0;
			gccq = 0;
			memset(&sts, 0, sizeof(struct stats));
			if(premlimit < 20) premlimit+=10;
			if(paddlimit < 20) paddlimit+=2;
			uint32_t pmsg = 0x01000000;
			for(k = 0; k < allses.count; k++) {
				write(allses.table[k]->sfd, &pmsg, 4);
				if(errno == EPIPE) {
					close(allses.table[k]->sfd);
					errno = 0;
				}
			}
		}

		if(allses.pcount != allses.count + extrafds) {
			if(allses.ptable) {
				free(allses.ptable);
				allses.ptable = 0;
			}
			allses.pcount = allses.count + extrafds;
			allses.ptable = (struct pollfd*)malloc(sizeof(struct pollfd) * allses.pcount);
			if(!allses.ptable) {
				perror("malloc poll table fails!");
			}
			i = 0;
			if(rqrun) {
				allses.ptable[i].fd = 0;
				allses.ptable[i].events = POLLIN;
				i++;
			}
			if(flagsvr) {
				allses.ptable[i].fd = fdlisten;
				allses.ptable[i].events = POLLIN;
				i++;
			}
			for(k = 0; k < allses.count; k++) {
				allses.ptable[i].fd = allses.table[k]->sfd;
				allses.ptable[i].events = POLLIN | POLLOUT;
				i++;
			}
		}
		if(allses.ptable) {
			i = poll(allses.ptable, allses.pcount, 0);
		} else {
			i = 0;
		}
		if(i > 0) {
			for(k = 0; k < allses.pcount; k++) {
				if(!allses.ptable[k].revents) continue;
				const char *etxt = 0;
				/* Here be dragons */
				switch(allses.ptable[k].revents) {
				case POLLERR: etxt = "poll: FD error\n"; goto sessionerror;
				case POLLHUP: etxt = "poll: FD hangup\n"; goto sessionerror;
				case POLLNVAL: etxt = "poll: FD invalid\n"; goto sessionerror;
				default:
					if(allses.ptable[k].revents & POLLIN) {
						if(allses.ptable[k].fd == 0) {
							handle_stdin();
						} else if(allses.ptable[k].fd == fdlisten) {
							handle_newsessions();
						} else if(allses.table[k - extrafds]->sfd == allses.ptable[k].fd) {
							if(handle_session_rd(allses.table[k - extrafds], CRun))
								goto sessionerror;
						} else {
							fprintf(stderr, "netses: session ID error\n");
						}
					}
					if(allses.ptable[k].revents & POLLOUT)
					break;
				}
				continue;
sessionerror:
				if(etxt) fprintf(stderr, etxt);
				if(allses.table[k - extrafds]->sfd == allses.ptable[k].fd) {
					isi_delete_ses(allses.table[k - extrafds]);
					k = allses.pcount;
				}
			}
		}
		if(!flagdbg) {
			cux++;
			if(cux > allcpu.count - 1) {
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
	if(fdlisten > -1) {
		close(fdlisten);
	}
	if(flagsvr) {
		fprintf(stderr, "closing connections\n");
		for(k = 0; k < allses.count; k++) {
			shutdown(allses.table[k]->sfd, SHUT_RDWR);
			close(allses.table[k]->sfd);
		}
	}
	if(rqrun) printf("\n\n\n\n");
	return 0;
}

