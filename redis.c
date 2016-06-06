
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "cputypes.h"
#include "netmsg.h"
#include <stdio.h>

extern struct isiDevTable alldev;
extern struct isiConTable allcon;
extern struct isiObjTable allobj;
int isi_create_object(int objtype, struct objtype **out);
static int redis_handle_rd(struct isiSession *ses, struct timespec mtime);
int isi_premake_object(int objtype, struct isiConstruct **outcon, struct objtype **out);

#define REDIS_NIL 0
#define REDIS_STR 1
#define REDIS_ERR 2
#define REDIS_INT 3
#define REDIS_DATA 4
#define REDIS_ARRAY 5
#define REDIS_TYPEMAX 5

#define REDIS_DATACOPY 6
#define REDIS_DATASKIP 7

struct redis_osp {
	int etype;
	int64_t len;
	int mode;
	int cmode;
};

int redis_make_session(struct sockaddr *ripa, socklen_t rin)
{
	int fdn, i;
	fdn = socket(ripa->sa_family, SOCK_STREAM, 0);
	if(fdn < 0) {
		isilogerr("socket");
		return -1;
	}
	i = 1;
	setsockopt(fdn, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int));
	i = 1;
	if( setsockopt(fdn, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0 ) {
		isilogerr("set'opt");
		close(fdn);
		return -1;
	}
	if( fcntl(fdn, F_SETFL, O_NONBLOCK) < 0 ) {
		isilogerr("fcntl");
		close(fdn);
		return -1;
	}
	i = connect(fdn, ripa, rin);
	if( i < 0 && errno != EINPROGRESS ) {
		isilogerr("connect");
		close(fdn);
		return -1;
	}
	struct isiSession *ses;
	if((i= isi_create_object(ISIT_SESSION, (struct objtype **)&ses))) {
		return i;
	}
	ses->in = (uint8_t*)malloc(8192);
	ses->out = (uint8_t*)malloc(8192);
	ses->cmdqlimit = 1024;
	ses->cmdqend = 0;
	ses->cmdq = (struct sescommandset *)malloc(ses->cmdqlimit * sizeof(struct sescommandset));
	memset(ses->cmdq, 0, ses->cmdqlimit * sizeof(struct sescommandset));
	ses->sfd = fdn;
	ses->stype = 2;
	ses->Recv = redis_handle_rd;
	memcpy(&ses->r_addr, ripa, sizeof(struct sockaddr_in));
	if(ses->r_addr.sin_family == AF_INET) {
		union {
			uint8_t a[4];
			uint32_t la;
		} ipa;
		ipa.la = ses->r_addr.sin_addr.s_addr;
		isilog(L_INFO, "net-server: new IP Redis session to: %d.%d.%d.%d:%d\n"
			, ipa.a[0], ipa.a[1], ipa.a[2], ipa.a[3], ntohs(ses->r_addr.sin_port)
		);
	} else {
		isilog(L_INFO, "net-server: new Redis session made\n");
	}
	isi_pushses(ses);
	return 0;
}

int redis_make_session_lu(const char *addr)
{
	struct addrinfo hint;
	struct addrinfo *ritr = 0;
	const char * aport = 0;
	const char * aend = 0;
	const char * ac = 0;
	char * saddr = 0;
	char * sport = 0;
	int brack = 0;
	int r = -1;
	memset(&hint, 0, sizeof(struct addrinfo));
	for(ac = addr; *ac; ac++) {
		if(*ac == '[' && brack == 0) {
			addr = ac + 1;
			brack = 1;
		}
		if(*ac == ']' && brack == 1) {
			brack = 2;
		}
		if(brack < 2) aend = ac + 1;
		if((brack == 0 || brack == 2) && *ac == ':') { aport = ac + 1; brack = 3; }
	}
	if(!aend || addr == aend) {
		isilog(L_ERR, "net-param: Connect Invalid input: [%s]\n", addr);
		return -1;
	}
	saddr = (char*)malloc((aend-addr)+1);
	memcpy(saddr, addr, (aend-addr));
	saddr[aend-addr] = 0;
	if(!aport) {
		sport = strdup("6379");
	} else {
		sport = strdup(aport);
	}
	hint.ai_family = AF_UNSPEC;
	hint.ai_protocol = 0;
	hint.ai_socktype = SOCK_STREAM;
	getaddrinfo(saddr, sport, &hint, &ritr);
	if(ritr) {
		size_t la;
		la = ritr->ai_addrlen;
		r = redis_make_session(ritr->ai_addr, la);
	} else {
		isilog(L_ERR, "net-param: Resolve failed on: [%s]:%s\n", saddr, sport);
	}
	freeaddrinfo(ritr);
	free(saddr);
	free(sport);
	return r;
}

void isi_redis_test()
{
	uint32_t u = 0;
	while(u < allobj.count) {
		if(allobj.table[u] && allobj.table[u]->objtype == ISIT_SESSION) {
			struct isiSession *ses = (struct isiSession *)allobj.table[u];
			if(ses->stype == 2) {
				struct sescommandset *ncmd;
				if(session_add_cmdq(ses, &ncmd)) continue;
				ncmd->cmd = 1;
				const char *testvec = "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n";
				write(ses->sfd, testvec, strlen(testvec));
				if(session_add_cmdq(ses, &ncmd)) continue;
				ncmd->cmd = 2;
				const char *testvec2 = "*2\r\n$3\r\nGET\r\n$6\r\ntest:3\r\n";
				write(ses->sfd, testvec2, strlen(testvec2));
			}
		}
		u++;
	}
}

int persist_load_object(uint32_t session, uint32_t cid, uint64_t uuid, uint32_t tid)
{
	struct isiConstruct *con = 0;
	struct objtype *obj;
	int r = isi_premake_object(cid, &con, &obj);
	return r;
}

static int redis_fetch_data(struct isiSession *ses)
{
	int i;
	int l;
	l = 1524;
	errno = 0;
	i = -1;
	while(i < 0) {
		if(!(l - ses->rcv)) return 0;
		fprintf(stderr, "redis recv: %d - ", l-ses->rcv);
		i = recv(ses->sfd, ses->in+ses->rcv, l-ses->rcv, MSG_WAITALL);
		fprintf(stderr, "%d\n", i);
		if(i < 0) {
			isilogerr("socket read");
			if(errno == EAGAIN || errno == EWOULDBLOCK) return 1;
			return -1;
		}
	}
	ses->rcv += i;
	ses->in[ses->rcv] = 0;
	return 0;
}

int redis_skip_data(struct isiSession *ses, uint8_t **rdpoint, int64_t *len)
{
	uint8_t *rdend = ses->in + ses->rcv;
	while(*len > 0 && *rdpoint < rdend) {
		if(*rdpoint + *len <= rdend) {
			*rdpoint += *len;
			*len = 0;
		} else if(*len > 0) {
			*len -= ses->rcv - (*rdpoint - ses->in);
			*rdpoint = ses->in;
			ses->rcv = 0;
			int r = redis_fetch_data(ses);
			if(r) return r;
			rdend = ses->in + ses->rcv;
		}
	}
	return 0;
}

static int redis_load_data(struct isiSession *ses, uint8_t **rdpoint, uint8_t *target, int64_t *len)
{
	uint8_t *rdend = ses->in + ses->rcv;
	fprintf(stderr, "data:");
	while(*len > 0 && *rdpoint < rdend) {
		if(*len > 2) fprintf(stderr, "%02x", **rdpoint);
		else fprintf(stderr, " [%02x]", **rdpoint);
		++*rdpoint; --*len;
		if(*len > 0 && *rdpoint == rdend) {
			*rdpoint = ses->in;
			ses->rcv = 0;
			int r = redis_fetch_data(ses);
			if(r) return r;
			rdend = ses->in + ses->rcv;
		}
	}
	putc(10, stderr);
	return 0;
}

static void redis_cmd_load_data(struct redis_osp *osp, uint8_t *target, int64_t len)
{
	if(osp->etype == REDIS_DATA && osp->mode < 2) osp->mode = 1;
}

static int redis_get_element(struct isiSession *ses, uint8_t **rdpoint, struct redis_osp *osp, uint8_t **strout)
{
	int64_t val = 0;
	int eoa = 0;
	int ss = 0;
	int nxm = -1;
	uint8_t *strst = 0;
	uint8_t *rdend = ses->in + ses->rcv;
	if(!osp) return -1;
	osp->len = 0;
	while(!eoa && *rdpoint < rdend) {
		switch(ss) {
		case 0:
			ss = 1;
			switch(**rdpoint) {
			case '*': nxm = REDIS_ARRAY; break;
			case '+': nxm = REDIS_STR; ss = 5; break;
			case '-': nxm = REDIS_ERR; ss = 5; break;
			case ':': nxm = REDIS_INT; break;
			case '$': nxm = REDIS_DATA; break;
			default:
				return -1;
			}
			break;
		case 1:
			if(**rdpoint == '-') { ss = 3; break; }
			ss = 2;
		case 2:
		case 3:
			if((**rdpoint) >= '0') {
				val = (val * 10) + ((**rdpoint) - '0');
			} else {
				if(ss == 3) val = -val;
				ss = 4;
			}
			break;
		case 4:
			osp->etype = nxm;
			if((nxm == REDIS_ARRAY || nxm == REDIS_DATA) && (val < 0)) {
				osp->etype = REDIS_NIL;
			}
			if(nxm == REDIS_DATA && (val > -1)) {
				val += 2;
			}
			eoa = 1;
			break;
		case 5:
			strst = *rdpoint;
			ss = 6;
		case 6:
			if(**rdpoint == '\r') {
				**rdpoint = 0;
				if(strout) *strout = strst;
				val = *rdpoint - strst;
				ss = 4;
			}
			break;
		}
		++*rdpoint;
		if(!eoa && *rdpoint >= rdend) {
			*rdpoint -= ses->rcv;
			ses->rcv = 0;
			int r = redis_fetch_data(ses);
			if(r) return r;
			rdend = ses->in + ses->rcv;
		}
	}
	osp->len = val;
	return 0;
}

static struct redis_osp parsestat[10];
static int parseheight = 0;
static int redis_handle_rd(struct isiSession *ses, struct timespec mtime)
{
	int i;
	i = redis_fetch_data(ses);
	if(i < 0) return i;
	if(i > 0) return 0;
	isilog(L_DEBUG, "net-redis: got message l=%d\n", ses->rcv);
	uint8_t *inv = ses->in;
	struct sescommandset *ncmd = 0;
	session_get_cmdq(ses, &ncmd, 0);
	if(!ncmd) {
		isilog(L_DEBUG, "net-redis: no message handle command\n");
		fprintf(stderr, "net-redis: %s\n", ses->in);
	}
	while(parseheight || inv < ses->in + ses->rcv) {
		uint8_t *outstr;
		struct redis_osp *osp;
		int fetch = 0;
		if(parseheight) {
			osp = parsestat + (parseheight - 1);
			switch(osp->etype) {
			case REDIS_ARRAY:
				if(osp->len > 0) {
					fetch = 1;
					osp->len--;
				} else {
					fetch = 2;
				}
				break;
			case REDIS_DATA:
				if(osp->mode == 1) osp->etype = REDIS_DATACOPY;
				else if(osp->mode == 2) fetch = 2;
				else osp->etype = REDIS_DATASKIP;
				osp->mode = 0;
				break;
			case REDIS_DATASKIP:
				fetch = 2;
				break;
			}
		} else {
			fetch = 1;
		}
		if(fetch == 2) {
			parseheight--;
			if(!parseheight) {
				ncmd = 0;
				session_get_cmdq(ses, NULL, 1); /* remove a command normally */
				session_get_cmdq(ses, &ncmd, 0);
			}
			continue;
		} else if(fetch) {
			osp = parsestat + parseheight++;
			if(redis_get_element(ses, &inv, osp, &outstr)) {
				isilog(L_ERR, "net-redis: parse error\n");
				ses->rcv = 0;
				return 0;
			}
			if(osp->etype == REDIS_ERR) {
				isilog(L_ERR, "net-redis-remote: Error: %s\n", outstr);
			}
		}
		if(osp->etype > REDIS_TYPEMAX) {
			switch(osp->etype) {
			case REDIS_DATACOPY:
				i = redis_load_data(ses, &inv, NULL, &osp->len);
				if(!osp->len) {
					osp->etype = REDIS_DATA;
					osp->mode = 2;
				}
				break;
			case REDIS_DATASKIP:
				i = redis_skip_data(ses, &inv, &osp->len);
				break;
			}
		}
		if(ncmd && osp->etype <= REDIS_TYPEMAX) {
			switch(ncmd->cmd) {
			case 1:
				switch(parseheight) {
				case 1:
					isilog(L_DEBUG, "net-redis: test message\n");
					if(osp->etype != REDIS_ARRAY) {
						isilog(L_ERR, "net-redis: nil array %d\n", osp->etype);
						ncmd = 0;
						break;
					}
					osp->cmode = 1;
					break;
				case 2:
					if(osp->etype == REDIS_DATA) {
						redis_cmd_load_data(osp, NULL, osp->len);
					}
					break;
				}
				break;
			case 2:
				isilog(L_DEBUG, "net-redis: load test data type=%d,%d\n", osp->etype, osp->mode);
				if(osp->etype == REDIS_DATA) {
					if(osp->mode != 2) {
						redis_cmd_load_data(osp, NULL, osp->len);
					} else {
						isilog(L_DEBUG, "net-redis: test data success\n");
					}
				} else {
					isilog(L_DEBUG, "net-redis: wrong type\n");
				}
				break;
			default:
				isilog(L_DEBUG, "net-redis: message handle command (%d) unknown\n", ncmd->cmd);
				break;
			}
		}
	}
	ses->rcv -= (inv - ses->in);
	if(ses->rcv) {
		memmove(ses->in, inv, ses->rcv);
		inv = ses->in;
	}
	return 0;
}

