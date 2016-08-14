
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
static int redis_handle_rq(struct isiSession *ses, struct timespec mtime);

#define REDIS_NIL 0
#define REDIS_STR 1
#define REDIS_ERR 2
#define REDIS_INT 3
#define REDIS_DATA 4
#define REDIS_ARRAY 5
/* status codes */
#define REDIS_DATAEND 6
#define REDIS_CMDEXIT 7
#define REDIS_TYPEMAX 7

#define REDIS_DATACOPY 8
#define REDIS_DATASKIP 9

char *redis_prefix = 0;
int redis_pfxlen = 0;

struct redis_osp {
	int etype;
	int64_t len;
	int64_t itr;
	int mode;
	int cmode;
};

struct redis_istate {
	int parseheight;
	int rqsent;
	int rqstart;
	uint8_t * cptarget;
	int64_t cplen;
	int64_t zrlen;
	struct redis_osp parsestat[10];
};

int redis_make_session(struct sockaddr *ripa, socklen_t rin)
{
	int fdn, i;
	if(!redis_prefix) {
		redis_prefix = strdup("");
		redis_pfxlen = 0;
	}
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
	ses->in = (uint8_t*)isi_alloc(8192);
	ses->out = (uint8_t*)isi_alloc(8192);
	ses->istate = isi_alloc(sizeof(struct redis_istate));
	ses->cmdqlimit = 1024;
	ses->cmdqend = 0;
	ses->cmdq = (struct sescommandset *)isi_alloc(ses->cmdqlimit * sizeof(struct sescommandset));
	memset(ses->cmdq, 0, ses->cmdqlimit * sizeof(struct sescommandset));
	ses->sfd = fdn;
	ses->stype = 2;
	ses->Recv = redis_handle_rd;
	ses->STick = redis_handle_rq;
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
	saddr = (char*)isi_alloc((aend-addr)+1);
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
				if(session_add_cmdq(ses, &ncmd)) continue;
				ncmd->cmd = 2;
			}
		}
		u++;
	}
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

static int redis_load_data(struct isiSession *ses, uint8_t **rdpoint, struct redis_istate *istate)
{
	uint8_t *rdend = ses->in + ses->rcv;
	fprintf(stderr, "data:");
	struct redis_osp *osp = istate->parsestat + (istate->parseheight - 1);
	while(osp->len > 0 && *rdpoint < rdend) {
		if(osp->len > 2) {
			if(istate->cptarget && istate->cplen) {
				*(istate->cptarget++) = **rdpoint;
				--istate->cplen;
			} else fprintf(stderr, "%02x", **rdpoint);
		}
		else fprintf(stderr, " [%02x]", **rdpoint);
		++*rdpoint; --osp->len;
		if(osp->len > 0 && *rdpoint == rdend) {
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

static void redis_cmd_load_data(struct redis_istate *ist, uint8_t *target, int64_t len)
{
	if(!ist || !ist->parseheight) return;
	struct redis_osp *osp = ist->parsestat + (ist->parseheight - 1);
	if(osp->etype == REDIS_DATA && osp->mode < 2) {
		osp->mode = 1;
		ist->cptarget = target;
		ist->cplen = len;
		if(osp->len < len) {
			ist->cplen = osp->len;
			ist->zrlen = len - osp->len;
		}
	}
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
	memset(osp, 0, sizeof(struct redis_osp));
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

struct redis_persist_type {
	const char *text;
} redis_ptype[] =
{
	{"desc"},
	{"rA"},
	{"nA"}
};

static int redis_handle_rq(struct isiSession *ses, struct timespec mtime)
{
	struct redis_istate *istate = (struct redis_istate *)ses->istate;
	struct sescommandset *ncmd = 0;
	const char *cvec;
	char tuid[16];
	int l, i;
	int lim;
	int lo;
	lim = 8192;
	lo = 0;
	session_get_cmdq(ses, &ncmd, 0);
	if(!ncmd) return 0;
	if(istate->rqstart != ses->cmdqstart) {
		istate->rqstart = ses->cmdqstart;
		istate->rqsent = 0;
	}
	if(!istate->rqsent) {
		switch(ncmd->cmd) {
		case 0:
			session_get_cmdq(ses, NULL, 1); /* cancel */
			return 0;
		case ISIC_TESTLIST:
			cvec = "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n";
			write(ses->sfd, cvec, strlen(cvec));
			break;
		case ISIC_TESTDATA:
			cvec = "*2\r\n$3\r\nGET\r\n$6\r\ntest:3\r\n";
			write(ses->sfd, cvec, strlen(cvec));
			break;
		case ISIC_LOADOBJECT:
		{
			struct isiPLoad *pld = (struct isiPLoad *)ncmd->rdata;
			const char *cname = 0;
			size_t nlen;
			isi_get_name(pld->ncid, &cname);
			nlen = strlen(cname) + 12 + redis_pfxlen;
			isi_text_enc(tuid, 11, &pld->uuid, 8);
			isilog(L_DEBUG, "net-redis: requesting load for %s%s:%s:*\n",
					redis_prefix, cname, tuid);
			l = snprintf((char*)ses->out, lim, "*4\r\n$4\r\nMGET\r\n");
			lo += l;
			for(i = 0; i < 3; i++) {
				l = snprintf((char*)ses->out+lo, lim-lo, "$%ld\r\n%s%s:%s:%s\r\n",
						strlen(redis_ptype[i].text)+1+nlen,
						redis_prefix, cname, tuid, redis_ptype[i].text);
				lo += l;
			}
			write(ses->sfd, ses->out, lo);
		}
			break;
		case ISIC_DISKWRITE:
		case ISIC_DISKWRLD:
		{
			const char *cname = 0;
			size_t nlen;
			uint64_t fullindex = 0;
			struct isiInfo *disk = (struct isiInfo *)ncmd->cptr;
			isi_get_name(disk->id.objtype, &cname);
			nlen = strlen(cname) + 12 + redis_pfxlen;
			isi_text_enc(tuid, 11, &disk->id.uuid, 8);
			isilog(L_DEBUG, "net-redis: writing disk data %s%s:%s:blk\n",
					redis_prefix, cname, tuid);
			lo += snprintf((char*)ses->out, lim, "*4\r\n$8\r\nSETRANGE\r\n");
			lo += snprintf((char*)ses->out+lo, lim-lo, "$%ld\r\n%s%s:%s:blk\r\n",
					4+nlen,	redis_prefix, cname, tuid);
			fullindex = ncmd->tid * 4096ull;
			l = snprintf(tuid, 16, "%lu", fullindex);
			lo += snprintf((char*)ses->out+lo, lim-lo, "$%d\r\n%s\r\n", l, tuid);
			lo += snprintf((char*)ses->out+lo, lim-lo, "$4096\r\n");
			void *blkdat;
			isi_disk_getblock(disk, &blkdat);
			memcpy(ses->out+lo, blkdat, 4096);
			lo += 4096;
			lo += snprintf((char*)ses->out+lo, lim-lo, "\r\n");
			write(ses->sfd, ses->out, lo);
		}
			break;
		case ISIC_DISKLOAD:
		{
			const char *cname = 0;
			size_t nlen;
			uint64_t fullindex = 0;
			struct isiInfo *disk = (struct isiInfo *)ncmd->cptr;
			isi_get_name(disk->id.objtype, &cname);
			nlen = strlen(cname) + 12 + redis_pfxlen;
			isi_text_enc(tuid, 11, &disk->id.uuid, 8);
			isilog(L_DEBUG, "net-redis: requesting disk data %s%s:%s:blk\n",
					redis_prefix, cname, tuid);
			lo += snprintf((char*)ses->out, lim, "*4\r\n$8\r\nGETRANGE\r\n");
			lo += snprintf((char*)ses->out+lo, lim-lo, "$%ld\r\n%s%s:%s:blk\r\n",
					4+nlen,	redis_prefix, cname, tuid);
			fullindex = ncmd->param * 4096ull;
			l = snprintf(tuid, 16, "%lu", fullindex);
			lo += snprintf((char*)ses->out+lo, lim-lo, "$%d\r\n%s\r\n", l, tuid);
			fullindex += 4095u;
			l = snprintf(tuid, 16, "%lu", fullindex);
			lo += snprintf((char*)ses->out+lo, lim-lo, "$%d\r\n%s\r\n", l, tuid);
			write(ses->sfd, ses->out, lo);
		}
			break;
		}
		istate->rqsent = 1;
	}
	return 0;
}

static int redis_handle_rd(struct isiSession *ses, struct timespec mtime)
{
	int i;
	struct redis_istate *istate = (struct redis_istate *)ses->istate;
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
	int cmdnox = 0;
	while(istate->parseheight || inv < ses->in + ses->rcv) {
		uint8_t *outstr;
		struct redis_osp *osp;
		int fetch = 0;
		int etype = 0;
		int eitr = 0;
		if(istate->parseheight) {
			osp = istate->parsestat + (istate->parseheight - 1);
			if(istate->parseheight > 1) {
				eitr = istate->parsestat[istate->parseheight-2].itr;
			}
			switch(osp->etype) {
			case REDIS_ARRAY:
				if(osp->itr < osp->len) {
					fetch = 1;
					eitr = osp->itr++;
				} else {
					fetch = 2;
				}
				break;
			case REDIS_DATA:
				if(osp->mode == 1) osp->etype = REDIS_DATACOPY;
				else osp->etype = REDIS_DATASKIP;
				osp->mode = 0;
				break;
			case REDIS_DATASKIP:
				fetch = 2;
				break;
			default:
				fetch = 2;
				break;
			}
		} else {
			fetch = 1;
		}
		etype = osp->etype;
		if(fetch == 2) {
			istate->parseheight--;
			if(!istate->parseheight) {
				if(cmdnox) {
					istate->rqsent = 0;
					break;
				}
				etype = REDIS_CMDEXIT;
			} else continue;
		} else if(fetch) {
			osp = istate->parsestat + istate->parseheight++;
			if(redis_get_element(ses, &inv, osp, &outstr)) {
				isilog(L_ERR, "net-redis: parse error\n");
				ses->rcv = 0;
				return 0;
			}
			if(osp->etype == REDIS_ERR) {
				isilog(L_ERR, "net-redis-remote: Error: %s\n", outstr);
			}
			etype = osp->etype;
		}
		if(etype > REDIS_TYPEMAX) {
			switch(osp->etype) {
			case REDIS_DATACOPY:
				i = redis_load_data(ses, &inv, istate);
				if(!osp->len) {
					if(!istate->cplen && istate->zrlen) {
						memset(istate->cptarget, 0, istate->zrlen);
						istate->zrlen = 0;
					}
					etype = osp->etype = REDIS_DATAEND;
				}
				break;
			case REDIS_DATASKIP:
				i = redis_skip_data(ses, &inv, &osp->len);
				break;
			}
		}
		if(ncmd && etype <= REDIS_TYPEMAX) {
			switch(ncmd->cmd) {
			case ISIC_TESTLIST:
				switch(istate->parseheight) {
				case 1:
					isilog(L_DEBUG, "net-redis: test message\n");
					if(etype != REDIS_ARRAY) {
						isilog(L_ERR, "net-redis: nil array %d\n", etype);
						ncmd = 0;
						break;
					}
					osp->cmode = 1;
					break;
				case 2:
					if(etype == REDIS_DATA) {
						redis_cmd_load_data(istate, NULL, osp->len);
					}
					break;
				}
				break;
			case ISIC_TESTDATA:
				isilog(L_DEBUG, "net-redis: load test data type=%d,%d\n", etype, osp->mode);
				if(etype == REDIS_DATA) {
					redis_cmd_load_data(istate, NULL, osp->len);
				} else if(etype == REDIS_DATAEND) {
					isilog(L_DEBUG, "net-redis: test data success\n");
				} else {
					isilog(L_DEBUG, "net-redis: wrong type\n");
				}
				break;
			case ISIC_LOADOBJECT:
			{
				struct isiPLoad *pld = (struct isiPLoad *)ncmd->rdata;
				switch(etype) {
				case REDIS_DATA:
					isilog(L_DEBUG, "net-redis: loadobject data %ld\n", eitr);
					{
						struct isiInfo *linfo = (struct isiInfo *)pld->obj;
						if(eitr == 2) {
							linfo->nvsize = osp->len;
							linfo->nvstate = isi_alloc(linfo->nvsize);
							redis_cmd_load_data(istate, linfo->nvstate, linfo->nvsize);
						}
					}
					ncmd->param++;
					break;
				case REDIS_DATAEND:
					isilog(L_DEBUG, "net-redis: loadobject data success %ld\n", eitr);
					break;
				case REDIS_NIL:
					isilog(L_DEBUG, "net-redis: loadobject nil data i=%ld\n", eitr);
					break;
				case REDIS_CMDEXIT:
					if(!ncmd->param) {
						isilog(L_DEBUG, "net-redis: loadobject fail\n");
						session_async_end(ncmd, ISIERR_NOTFOUND);
						isi_delete_object(pld->obj);
					} else {
						struct isiInfo *linfo = (struct isiInfo *)pld->obj;
						linfo->id.objtype = pld->ncid;
						linfo->id.uuid = pld->uuid;
						if(linfo->meta->Init) {
							linfo->meta->Init(linfo);
						}
						isilog(L_DEBUG, "net-redis: loadobject success\n");
						session_async_end(ncmd, 0);
					}
					break;
				}
			}
				break;
			case ISIC_DISKLOAD:
				isilog(L_DEBUG, "net-redis: diskload\n");
				if(etype == REDIS_DATA) {
					struct isiInfo *disk = (struct isiInfo *)ncmd->cptr;
					void *blkdat;
					isi_disk_getblock(disk, &blkdat);
					redis_cmd_load_data(istate, blkdat, 4096);
				} else if(etype == REDIS_CMDEXIT) {
					struct isiInfo *disk = (struct isiInfo *)ncmd->cptr;
					uint16_t dm[4] = {0x22, ncmd->param, ncmd->param >> 16, 0};
					isi_message_dev(disk, -1, dm, 3, mtime);
				}
				break;
			case ISIC_DISKWRITE:
				isilog(L_DEBUG, "net-redis: diskwrite\n");
				break;
			case ISIC_DISKWRLD:
				isilog(L_DEBUG, "net-redis: diskwrite-load\n");
				if(etype == REDIS_INT) {
					cmdnox = 1;
					ncmd->cmd = ISIC_DISKLOAD;
				}
				break;
			default:
				isilog(L_DEBUG, "net-redis: message handle command (%d) unknown\n", ncmd->cmd);
				break;
			}
		}
		if(etype == REDIS_CMDEXIT) {
			ncmd = 0;
			session_get_cmdq(ses, NULL, 1); /* remove a command normally */
			session_get_cmdq(ses, &ncmd, 0);
		}
	}
	ses->rcv -= (inv - ses->in);
	if(ses->rcv) {
		memmove(ses->in, inv, ses->rcv);
		inv = ses->in;
	}
	return 0;
}

