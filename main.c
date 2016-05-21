
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
static char * diskf = 0;
static int listenportnumber = 58704;

int fdlisten = -1;
int fdlog = STDERR_FILENO;

static const int quantum = 1000000000 / 10000; // ns

enum {
	CPUSMAX = 800,
	CPUSMIN = 20
};
static uint32_t numberofcpus = 1;
static uint32_t softcpumax = 1;
static uint32_t softcpumin = 1;

struct isiDevTable alldev;
struct isiDevTable allcpu;

struct isiConTable allcon;

uint32_t maxsid = 0;
struct isiObjTable allobj;
struct isiSessionTable allses;

void isi_run_sync(struct timespec crun);
void isi_register_objects();
int isi_scan_dir();
void isi_init_sestable();
int isi_delete_ses(struct isiSession *s);
int handle_newsessions();
int handle_session_rd(struct isiSession *ses, struct timespec mtime);
int makeserver(int portnumber);
void showdisasm_dcpu(const struct isiInfo *info);
void showdiag_dcpu(const struct isiInfo* info, int fmt);

static const char * const gsc_usage =
"Usage:\n%s [-Desr] [-p <portnum>] [-B <binid>] [-d <diskid>]\n%s -E <file>\n"
"Options:\n"
" -e  Assume file pointed to by <binid> is little-endian\n"
" -s  Enable server, if specified without -r then fork to background.\n"
" -p <portnum>  Listen on <portnum> instead of the default (valid with -s)\n"
" -r  Run interactively.\n"
" -D  Enable debugging and single stepping DCPU\n"
" -B <binid>  Load file with <binid> into DCPU memory starting at 0x0000.\n"
"      File is assmued to contain 16 bit words, 2 octets each in big-endian\n"
"      Use the -e option to load little-endian files.\n"
" -d <diskid>  Load disk with <diskid> into the default floppy drive.\n"
" -l  List file IDs then exit.\n"
" -E <file>  Flip the bytes in each 16 bit word of <file> then exit.\n";

#define ISILOGBUFFER 4096
static char logwrout[ISILOGBUFFER];
void isilogerr(const char * desc)
{
	int r;
	r = snprintf(logwrout, ISILOGBUFFER, "Error: %s: %s", desc, strerror(errno));
	if(r > 0) {
		write(fdlog, logwrout, r);
	}
}

void isilog(int level, const char *format, ...)
{
	va_list vl;
	int r;
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

int isi_attach(struct isiInfo *item, struct isiInfo *dev)
{
	int e = 0;
	if(!item || !dev) return -1;
	if(item->id.objtype < 0x2f00) return -1;
	if(item->id.objtype >= ISIT_CPU && item->id.objtype < ISIT_ENDCPU) {
		if(dev->id.objtype > 0x2000 && dev->id.objtype < 0x2f00) {
			if(item->QueryAttach) {
				if((e = item->QueryAttach(item, dev))) {
					return e;
				}
			}
			item->mem = dev;
			if(item->Attach) e = item->Attach(item, dev);
			return e;
		}
	}
	if(dev->id.objtype < 0x2f00) return -1;
	if(item->QueryAttach) {
		if((e = item->QueryAttach(item, dev))) {
			return e;
		}
	}
	if(item->id.objtype >= ISIT_BUSDEV && item->id.objtype < ISIT_ENDBUSDEV) {
	} else {
		item->dndev = dev;
	}
	dev->updev = item;
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

int isi_write_parameter(uint8_t *p, int plen, int code, const void *in, int limit)
{
	if(!p || plen < 1 || !code) return -1;
	int found = 0;
	int flen = 0;
	uint8_t *uo = (uint8_t*)in;
	for(int i = plen; i--; p++) {
		switch(found) {
		case 0:
			if(*p == 0) {
				found = 1;
				*p = code;
			}
			else found = 2;
			break;
		case 1:
			flen = *p = (uint8_t)limit;
			if(flen) found += 2;
			else return 0;
			break;
		case 2:
			flen = *p;
			if(flen) found += 2;
			else found = 0;
			break;
		case 3:
			if(limit && flen) {
				*p = *uo;
				uo++;
				limit--;
				flen--;
				if(!limit && !i) return 0;
			} else {
				*p = 0;
				return 0;
			}
			break;
		case 4:
			flen--;
			if(!flen) found = 0;
			break;
		default:
			found = 0;
			break;
		}
	}
	return 1;
}

int isi_fetch_parameter(const uint8_t *p, int plen, int code, void *out, int limit)
{
	if(!p || plen < 1 || !code) return -1;
	int found = 0;
	int flen = 0;
	uint8_t *uo = (uint8_t*)out;
	for(int i = plen; i--; p++) {
		switch(found) {
		case 0:
			if((*p) == code) found = 1;
			else found = 2;
			if(!*p) return 1;
			break;
		case 1:
			flen = *p;
			if(flen) found += 2;
			else return 0;
			break;
		case 2:
			flen = *p;
			if(flen) found += 2;
			else found = 0;
			break;
		case 3:
			if(limit && flen) {
				*uo = *p;
				uo++;
				limit--;
				flen--;
				if(!limit) return 0;
			} else {
				return 0;
			}
			break;
		case 4:
			flen--;
			if(!flen) found = 0;
			break;
		default:
			found = 0;
			break;
		}
	}
	return 1;
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
	case ISIT_NONE: return ISIERR_NOTFOUND;
	case ISIT_SESSION: objsize = sizeof(struct isiSession); break;
	case ISIT_NETSYNC: objsize = sizeof(struct isiNetSync); break;
	case ISIT_DISK: objsize = sizeof(struct isiDisk); break;
	case ISIT_MEM6416: objsize = sizeof(struct memory64x16); break;
	case ISIT_CPU: objsize = sizeof(struct isiCPUInfo); break;
	case ISIT_BUSDEV: objsize = sizeof(struct isiBusInfo); break;
	case ISIT_HARDWARE: objsize = sizeof(struct isiInfo); break;
	}
	if(objsize) {
		*sz = objsize;
		return 0;
	}
	return ISIERR_NOTFOUND;
}

int isi_create_object(int objtype, struct objtype **out)
{
	if(!out) return -1;
	struct objtype *ns;
	size_t objsize = 0;
	if(isi_get_type_size(objtype, &objsize)) return ISIERR_INVALIDPARAM;
	ns = (struct objtype*)malloc(objsize);
	if(!ns) return ISIERR_NOMEM;
	memset(ns, 0, objsize);
	ns->id = ++maxsid; // TODO make "better" ID numbers?
	ns->objtype = objtype;
	isi_objtable_add(ns);
	*out = ns;
	return 0;
}

int isi_make_object(int objtype, struct objtype **out, const uint8_t *cfg, size_t lcfg)
{
	uint32_t x;
	int i;
	struct isiConstruct *con = NULL;
	if(!objtype || !out) return ISIERR_INVALIDPARAM;
	for(x = 0; x < allcon.count; x++) {
		if(allcon.table[x]->objtype == objtype) {
			con = allcon.table[x];
			break;
		}
	}
	if(!con) return ISIERR_NOTFOUND;
	struct objtype *ndev;
	i = isi_create_object(objtype, &ndev);
	if(i) return i;
	struct isiInfo *info = (struct isiInfo*)ndev;
	if(objtype < 0x2f00) {
		*out = ndev;
		return 0;
	}
	info->meta = con;
	info->rvproto = con->rvproto;
	info->svproto = con->svproto;
	if(con->PreInit) con->PreInit(info, cfg, lcfg);
	if(objtype > 0x2f00 && con->QuerySize) {
		size_t sz = 0;
		con->QuerySize(0, cfg, lcfg, &sz);
		if(sz) {
			info->rvstate = malloc(sz);
			memset(info->rvstate, 0, sz);
		}
		sz = 0;
		con->QuerySize(1, cfg, lcfg, &sz);
		if(sz) {
			info->svstate = malloc(sz);
			memset(info->svstate, 0, sz);
		}
	} else {
		if(con->rvproto && con->rvproto->length) {
			info->rvstate = malloc(con->rvproto->length);
			memset(info->rvstate, 0, con->rvproto->length);
		}
		if(con->svproto && con->svproto->length) {
			info->svstate = malloc(con->svproto->length);
			memset(info->svstate, 0, con->svproto->length);
		}
	}
	if(con->Init) con->Init(info, cfg, lcfg);
	*out = ndev;
	return 0;
}

int isi_delete_object(struct objtype *obj)
{
	if(!obj) return ISIERR_INVALIDPARAM;
	struct isiObjTable *t = &allobj;
	if(obj->objtype >= 0x2fff) {
		struct isiInfo *info = (struct isiInfo *)obj;
		if(info->Delete) {
			info->Delete(info);
		}
		if(info->rvstate) {
			free(info->rvstate);
		}
		if(info->svstate) {
			free(info->svstate);
		}
		memset(info, 0, sizeof(struct isiInfo));
	}
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

int isi_init_contable()
{
	struct isiConTable *t = &allcon;
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiConstruct**)malloc(t->limit * sizeof(void*));
	return 0;
}

int isi_contable_add(struct isiConstruct *obj)
{
	if(!obj) return -1;
	void *n;
	if(allcon.count >= allcon.limit) {
		n = realloc(allcon.table, (allcon.limit + allcon.limit) * sizeof(void*));
		if(!n) return -5;
		allcon.limit += allcon.limit;
		allcon.table = (struct isiConstruct**)n;
	}
	allcon.table[allcon.count++] = obj;
	return 0;
}

int isi_register(struct isiConstruct *obj)
{
	int itype;
	int inum;
	if(!obj) return -1;
	if(!obj->name) return -1;
	if(!obj->objtype) return -1;
	itype = obj->objtype & 0xf000;
	inum = obj->objtype & 0x0fff;
	for(uint32_t i = 0; i < allcon.count; i++) {
		int ltype = allcon.table[i]->objtype & 0xf000;
		int lnum = allcon.table[i]->objtype & 0x0fff;
		if(ltype == itype) {
			if(lnum >= inum) {
				inum = lnum + 1;
			}
		}
	}
	obj->objtype = (itype & 0xf000) | (inum & 0x0fff);
	isi_contable_add(obj);
	isilog(L_INFO, "object: %s registered as %04x\n", obj->name, obj->objtype);
	return 0;
}

uint32_t isi_lookup_name(const char * name)
{
	if(!name) return 0;
	for(uint32_t i = 0; i < allcon.count; i++) {
		if(!strcmp(allcon.table[i]->name, name)) {
			return allcon.table[i]->objtype;
		}
	}
	isilog(L_INFO, "object: lookup name failed for: %s\n", name);
	return 0;
}

int isi_inittable(struct isiDevTable *t)
{
	t->limit = 32;
	t->count = 0;
	t->table = (struct isiInfo**)malloc(t->limit * sizeof(void*));
	return 0;
}

int isi_push_dev(struct isiDevTable *t, struct isiInfo *d)
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

int isi_find_dev(struct isiDevTable *t, uint32_t id, struct isiInfo **target)
{
	size_t i;
	if(!id) return -1;
	for(i = 0; i < t->count; i++) {
		struct isiInfo *obj = t->table[i];
		if(obj && obj->id.id == id) {
			if(target) {
				*target = obj;
			}
			return 0;
		}
	}
	return 1;
}

int isi_createdev(struct isiInfo **ndev)
{
	return isi_create_object(ISIT_HARDWARE, (struct objtype**)ndev);
}

int isi_createcpu(struct isiCPUInfo **ndev)
{
	return isi_create_object(ISIT_CPU, (struct objtype**)ndev);
}

int isi_addcpu()
{
	struct isiInfo *bus, *ninfo;
	struct isiCPUInfo *cpu;
	isiram16 nmem;
	isi_make_object(isi_lookup_name("dcpu"), (struct objtype**)&cpu, 0, 0);
	cpu->ctl = flagdbg ? (ISICTL_DEBUG | ISICTL_STEP) : 0;
	isi_make_object(isi_lookup_name("memory_16x64k"), (struct objtype**)&nmem, 0, 0);
	isi_attach((struct isiInfo*)cpu, (struct isiInfo*)nmem);
	isi_make_object(isi_lookup_name("dcpu_hwbus"), (struct objtype**)&bus, 0, 0);
	isi_make_object(isi_lookup_name("nya_lem"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ninfo);

	isi_make_object(isi_lookup_name("clock"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ninfo);

	isi_make_object(isi_lookup_name("keyboard"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ninfo);
	if(binf) {
		uint8_t ist[24];
		ist[0] = 0;
		uint64_t id = 0;
		isi_fname_id(binf, &id);
		isi_write_parameter(ist, 24, 2, &id, sizeof(uint64_t));
		if(loadendian) isi_write_parameter(ist, 24, 3, &id, 0);
		isi_make_object(isi_lookup_name("rom"), (struct objtype**)&ninfo, ist, 24);
		isi_attach(bus, ninfo);
	}
	isi_attach((struct isiInfo*)cpu, bus);

	isi_make_object(isi_lookup_name("mack_35fd"), (struct objtype**)&ninfo, 0, 0);
	isi_attach(bus, ninfo);
	if(diskf) {
		uint64_t dsk = 0;
		struct isiInfo *ndsk;
		isi_text_dec(diskf, strlen(diskf), 11, &dsk, 8);
		isi_create_disk(dsk, &ndsk);
		isi_attach(ninfo, ndsk);
	}

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
		if(allcpu.count && ((struct isiCPUInfo*)allcpu.table[0])->ctl & ISICTL_DEBUG) {
			((struct isiCPUInfo*)allcpu.table[0])->ctl |= ISICTL_STEPE;
		}
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
			if(allcpu.count) {
				ti = ((isiram16)allcpu.table[0]->mem)->ram[t];
			} else {
				t = ti = 0;
			}
			fprintf(stderr, "READ %04x:%04x\n", t, ti);
		}
		break;
	case 'x':
		haltnow = 1;
		i = read(0, ccv, 1);
		break;
	case 'n':
		if(allcpu.count) fprintf(stderr, "\n\n\n\n");
		isi_debug_dump_synctable();
		break;
	case 'l':
		if(allcpu.count) fprintf(stderr, "\n\n\n\n");
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
				case 'l':
					isi_scan_dir();
					break;
				case 'd':
					k = -1;
					if(i+1<argc) {
						diskf = strdup(argv[++i]);
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
	int extrafds = 0;

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

	if(!rqrun && !flagsvr) {
		fprintf(stderr, "At least -r or -s must be specified.\n");
		return 0;
	}

	for(cux = 0; cux < softcpumax; cux++) {
		isi_addcpu();
	}
	if(flagsvr) {
		if(!rqrun) enter_service();
		makeserver(listenportnumber);
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
	extrafds = (flagsvr?1:0) + (rqrun?1:0);
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

			if(ccpi->dndev && ccpi->dndev->RunCycles) {
				ccpi->dndev->RunCycles(ccpi->dndev, CRun);
			}
		}
		fetchtime(&CRun);
		isi_run_sync(CRun);

		fetchtime(&CRun);
		if(CRun.tv_sec > LTUTime.tv_sec) { // roughly one second between status output
			if(rqrun) { // interactive diag
			double clkdelta;
			float clkrate;
			if(!flagdbg && allcpu.count) showdiag_dcpu(allcpu.table[0], 0);
			clkdelta = ((double)(CRun.tv_sec - LTUTime.tv_sec)) + (((double)CRun.tv_nsec) * 0.000000001);
			clkdelta-=(((double)LTUTime.tv_nsec) * 0.000000001);
			if(!lucpus) lucpus = 1;
			clkrate = ((((double)lucycles) * clkdelta) * 0.001) / numberofcpus;
			fprintf(stderr, "DC: %.4f sec, %d at % 9.3f kHz   (% 8ld) [Q:% 8d, S:% 8d, SC:% 8d]\r",
					clkdelta, numberofcpus, clkrate, gccq,
					sts.quanta, sts.cpusched, sts.cpusched / numberofcpus
					);
			if(!flagdbg && allcpu.count) showdiag_up(4);
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
				*((uint32_t*)allses.table[k]->out) = ISIMSG(PING, 0, 0);
				session_write_msg(allses.table[k]);
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
				isilogerr("malloc poll table fails!");
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
						if(rqrun && allses.ptable[k].fd == 0) {
							handle_stdin();
						} else if(allses.ptable[k].fd == fdlisten) {
							handle_newsessions();
						} else if(allses.table[k - extrafds]->sfd == allses.ptable[k].fd) {
							if(handle_session_rd(allses.table[k - extrafds], CRun))
								goto sessionerror;
						} else {
							isilog(L_ERR, "netses: session ID error\n");
						}
					}
					if(allses.ptable[k].revents & POLLOUT)
					break;
				}
				continue;
sessionerror:
				if(etxt) isilog(L_ERR, etxt);
				if(allses.table[k - extrafds]->sfd == allses.ptable[k].fd) {
					isi_delete_ses(allses.table[k - extrafds]);
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
	if(fdlisten > -1) {
		close(fdlisten);
	}
	if(flagsvr) {
		isilog(L_WARN, "closing connections\n");
		while(allses.count) {
			shutdown(allses.table[0]->sfd, SHUT_RDWR);
			isi_delete_ses(allses.table[0]);
		}
	}
	while(allobj.count) {
		isi_delete_object(allobj.table[0]);
	}
	if(rqrun) printf("\n\n\n\n");
	return 0;
}

