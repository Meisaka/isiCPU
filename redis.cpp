
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include "isitypes.h"
#include "netmsg.h"
#include <stdio.h>
#include <string>
#include <vector>
#include "platform.h"

extern std::vector<isiObjSlot> allobj;
extern std::vector<isiSession*> allses;

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

std::string redis_prefix;

struct redis_osp {
	int etype;
	int64_t len;
	int64_t itr;
	int mode;
	int cmode;
};

struct redis_istate {
	size_t parseheight;
	size_t rqsent;
	size_t rqstart;
	uint8_t * cptarget;
	int64_t cplen;
	int64_t zrlen;
	struct redis_osp parsestat[10];
};

class isiSessionRedis : public isiSession {
public:
	virtual int Recv(isi_time_t mtime);
	virtual int STick(isi_time_t mtime);
private:
	int fetch_data();
	int skip_data(uint8_t **rdpoint, int64_t *len);
	int load_data(uint8_t **rdpoint, redis_istate *istate);
	int get_element(uint8_t **rdpoint, redis_osp *osp, uint8_t **strout);
};

static isiClass<isiSessionRedis> isiSessionRedis_C(ISIT_SESSION_REDIS, "<isiSessionRedis>", "");
void isi_register_redis() {
	isi_register(&isiSessionRedis_C);
}

int redis_make_session(struct sockaddr *ripa, socklen_t rin) {
	fdesc_t fdn;
	int i;
	fdn = platform_socket(ripa->sa_family, SOCK_STREAM, 0);
	if(fdn == fdesc_empty) {
		isilogneterr("socket");
		return -1;
	}
	i = 1;
	setsockopt(fdn, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int));
	i = 1;
	if(setsockopt(fdn, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		isilogneterr("set'opt");
		close(fdn);
		return -1;
	}
	if(set_nonblock(fdn) < 0) {
		isilogneterr("fcntl");
		close(fdn);
		return -1;
	}
	i = connect(fdn, ripa, rin);
	if(i < 0 && errno != EINPROGRESS) {
		isilogneterr("connect");
		close(fdn);
		return -1;
	}
	isiSession *ses;
	if((i= isi_create_object(ISIT_SESSION_REDIS, NULL, (isiObject**)&ses))) {
		return i;
	}
	ses->in = make_msg();
	ses->istate = isi_calloc(sizeof(struct redis_istate));
	ses->cmdqlimit = 1024;
	ses->cmdqend = 0;
	ses->cmdq = (isiCommand *)isi_calloc(ses->cmdqlimit * sizeof(isiCommand));
	memset(ses->cmdq, 0, ses->cmdqlimit * sizeof(isiCommand));
	ses->sfd = fdn;
	ses->stype = 2;
	//ses->Recv = redis_handle_rd;
	//ses->STick = redis_handle_rq;
	// TODO redis session!
	ses->r_addr = (net_endpoint*)isi_calloc(sizeof(struct sockaddr_in));
	memcpy(ses->r_addr, ripa, sizeof(struct sockaddr_in));
	if(((sockaddr_in*)ses->r_addr)->sin_family == AF_INET) {
		union {
			uint8_t a[4];
			uint32_t la;
		} ipa;
		ipa.la = ((sockaddr_in*)ses->r_addr)->sin_addr.s_addr;
		isilog(L_INFO, "net-server: new IP Redis session to: %d.%d.%d.%d:%d\n"
			, ipa.a[0], ipa.a[1], ipa.a[2], ipa.a[3], ntohs(((sockaddr_in*)ses->r_addr)->sin_port)
		);
	} else {
		isilog(L_INFO, "net-server: new Redis session made\n");
	}
	allses.push_back(ses);
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
	std::string sport("6379");
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
	saddr = (char*)isi_calloc((aend-addr)+1);
	memcpy(saddr, addr, (aend-addr));
	saddr[aend-addr] = 0;
	if(aport) {
		sport = aport;
	}
	hint.ai_family = AF_UNSPEC;
	hint.ai_protocol = 0;
	hint.ai_socktype = SOCK_STREAM;
	getaddrinfo(saddr, sport.c_str(), &hint, &ritr);
	if(ritr) {
		size_t la;
		la = ritr->ai_addrlen;
		r = redis_make_session(ritr->ai_addr, la);
	} else {
		isilog(L_ERR, "net-param: Resolve failed on: [%s]:%s\n", saddr, sport.c_str());
	}
	freeaddrinfo(ritr);
	free(saddr);
	return r;
}

void isi_redis_test() {
	for(auto &obj : allobj) {
		if(obj.ptr && obj.ptr->otype == ISIT_SESSION_REDIS) {
			isiSession *ses = (isiSession *)obj.ptr;
			if(ses->stype == 2) {
				isiCommand *ncmd;
				if(ses->add_cmdq(&ncmd)) continue;
				ncmd->cmd = 1;
				if(ses->add_cmdq(&ncmd)) continue;
				ncmd->cmd = 2;
			}
		}
	}
}

int isiSessionRedis::fetch_data() {
	int i;
	errno = 0;
	i = -1;
	if(!this->in) {
		this->in = make_msg();
	}
	uint32_t limit = in->r_limit;
	while(i < 0) {
		if(!(limit - recv_index)) return 0;
		fprintf(stderr, "redis recv: %d - ", limit - recv_index);
		i = recv(sfd, (char*)(in->u8_head) + recv_index, limit - recv_index, MSG_WAITALL);
		fprintf(stderr, "%d\n", i);
		if(i < 0) {
			isilogneterr("socket read");
			if(errno == EAGAIN || errno == EWOULDBLOCK) return 1;
			return -1;
		}
	}
	recv_index += i;
	in->u8_head[recv_index] = 0;
	return 0;
}

int isiSessionRedis::skip_data(uint8_t **rdpoint, int64_t *len) {
	uint8_t *rdend = in->u8_head + recv_index;
	while(*len > 0 && *rdpoint < rdend) {
		if(*rdpoint + *len <= rdend) {
			*rdpoint += *len;
			*len = 0;
		} else if(*len > 0) {
			*len -= recv_index - (*rdpoint - in->u8_head);
			*rdpoint = in->u8_head;
			recv_index = 0;
			int r = fetch_data();;
			if(r) return r;
			rdend = in->u8_head + recv_index;
		}
	}
	return 0;
}

int isiSessionRedis::load_data(uint8_t **rdpoint, redis_istate *istate) {
	uint8_t *rdend = in->u8_head + recv_index;
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
			*rdpoint = in->u8_head;
			recv_index = 0;
			int r = fetch_data();
			if(r) return r;
			rdend = in->u8_head + recv_index;
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

int isiSessionRedis::get_element(uint8_t **rdpoint, struct redis_osp *osp, uint8_t **strout) {
	int64_t val = 0;
	int eoa = 0;
	int ss = 0;
	int nxm = -1;
	uint8_t *strst = 0;
	uint8_t *rdend = in->u8_head + recv_index;
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
			*rdpoint -= recv_index;
			recv_index = 0;
			int r = fetch_data();
			if(r) return r;
			rdend = in->u8_head + recv_index;
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

int isiSessionRedis::STick(isi_time_t mtime)
{
	struct redis_istate *istate = (struct redis_istate *)this->istate;
	isiCommand *ncmd = 0;
	const char *cvec;
	char tuid[16];
	int l, i;
	isiMsgRef out = make_msg();
	uint32_t limit = out->r_limit;
	uint32_t wr_offset = 0;
	get_cmdq(&ncmd, 0);
	if(!ncmd) return 0;
	if(istate->rqstart != this->cmdqstart) {
		istate->rqstart = this->cmdqstart;
		istate->rqsent = 0;
	}
	if(!istate->rqsent) {
		switch(ncmd->cmd) {
		case 0:
			get_cmdq(NULL, 1); /* cancel */
			return 0;
		case ISIC_TESTLIST:
			cvec = "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n";
			write(this->sfd, cvec, strlen(cvec));
			break;
		case ISIC_TESTDATA:
			cvec = "*2\r\n$3\r\nGET\r\n$6\r\ntest:3\r\n";
			write(this->sfd, cvec, strlen(cvec));
			break;
		case ISIC_LOADOBJECT:
		{
			struct isiPLoad *pld = (struct isiPLoad *)ncmd->rdata;
			std::string_view cname;
			size_t nlen;
			isi_get_name(pld->ncid, &cname);
			nlen = cname.size() + 12 + redis_prefix.size();
			isi_text_enc(tuid, 11, &pld->uuid, 8);
			isilog(L_DEBUG, "net-redis: requesting load for %s%s:%s:*\n",
					redis_prefix.c_str(), cname.data(), tuid);
			l = snprintf((char*)out->u8_head, limit, "*4\r\n$4\r\nMGET\r\n");
			wr_offset += l;
			for(i = 0; i < 3; i++) {
				l = snprintf((char*)out->u8_head + wr_offset, limit - wr_offset, "$%zd\r\n%s%s:%s:%s\r\n",
						strlen(redis_ptype[i].text)+1+nlen,
						redis_prefix.c_str(), cname.data(), tuid, redis_ptype[i].text);
				wr_offset += l;
			}
			write(this->sfd, (char*)out->u8_head, wr_offset);
		}
			break;
		case ISIC_DISKWRITE:
		case ISIC_DISKWRLD:
		{
			std::string_view cname;
			size_t nlen;
			uint64_t fullindex = 0;
			isiInfo *disk = (isiInfo *)ncmd->cptr;
			isi_get_name(disk->otype, &cname);
			nlen = cname.size() + 12 + redis_prefix.size();
			isi_text_enc(tuid, 11, &disk->uuid, 8);
			isilog(L_DEBUG, "net-redis: writing disk data %s%s:%s:blk\n",
					redis_prefix, cname.data(), tuid);
			wr_offset += snprintf((char*)out->u8_head, limit, "*4\r\n$8\r\nSETRANGE\r\n");
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit - wr_offset, "$%zd\r\n%s%s:%s:blk\r\n",
					4+nlen,	redis_prefix.c_str(), cname.data(), tuid);
			fullindex = ncmd->tid * 4096ull;
			l = snprintf(tuid, 16, "%llu", fullindex);
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit - wr_offset, "$%d\r\n%s\r\n", l, tuid);
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit - wr_offset, "$4096\r\n");
			void *blkdat;
			isi_disk_getblock(disk, &blkdat);
			memcpy(out->u8_head + wr_offset, blkdat, 4096);
			wr_offset += 4096;
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit-wr_offset, "\r\n");
			write(this->sfd, (char*)out->u8_head, wr_offset);
		}
			break;
		case ISIC_DISKLOAD:
		{
			std::string_view cname;
			size_t nlen;
			uint64_t fullindex = 0;
			isiInfo *disk = (isiInfo *)ncmd->cptr;
			isi_get_name(disk->otype, &cname);
			nlen = cname.size() + 12 + redis_prefix.size();
			isi_text_enc(tuid, 11, &disk->uuid, 8);
			isilog(L_DEBUG, "net-redis: requesting disk data %s%s:%s:blk\n",
					redis_prefix, cname.data(), tuid);
			wr_offset += snprintf((char*)out->u8_head, limit, "*4\r\n$8\r\nGETRANGE\r\n");
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit-wr_offset, "$%zd\r\n%s%s:%s:blk\r\n",
					4+nlen,	redis_prefix.c_str(), cname.data(), tuid);
			fullindex = ncmd->param * 4096ull;
			l = snprintf(tuid, 16, "%Iu", fullindex);
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit-wr_offset, "$%d\r\n%s\r\n", l, tuid);
			fullindex += 4095u;
			l = snprintf(tuid, 16, "%Iu", fullindex);
			wr_offset += snprintf((char*)out->u8_head+wr_offset, limit-wr_offset, "$%d\r\n%s\r\n", l, tuid);
			write(this->sfd, (char*)out->u8_head, wr_offset);
		}
			break;
		}
		istate->rqsent = 1;
	}
	return 0;
}

int isiSessionRedis::Recv(isi_time_t mtime)
{
	int i;
	struct redis_istate *istate = (struct redis_istate *)this->istate;
	i = fetch_data();
	if(i < 0) return i;
	if(i > 0) return 0;
	isilog(L_DEBUG, "net-redis: got message l=%d\n", recv_index);
	uint8_t *inv = in->u8_head;
	isiCommand *ncmd = 0;
	get_cmdq(&ncmd, 0);
	if(!ncmd) {
		isilog(L_DEBUG, "net-redis: no message handle command\n");
		fprintf(stderr, "net-redis: %s\n", in->u8_head);
	}
	int cmdnox = 0;
	while(istate->parseheight || inv < in->u8_head + recv_index) {
		uint8_t *outstr;
		struct redis_osp *osp;
		int fetch = 0;
		int etype = 0;
		int64_t eitr = 0;
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
			if(get_element(&inv, osp, &outstr)) {
				isilog(L_ERR, "net-redis: parse error\n");
				recv_index = 0;
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
				i = load_data(&inv, istate);
				if(!osp->len) {
					if(!istate->cplen && istate->zrlen) {
						memset(istate->cptarget, 0, istate->zrlen);
						istate->zrlen = 0;
					}
					etype = osp->etype = REDIS_DATAEND;
				}
				break;
			case REDIS_DATASKIP:
				i = skip_data(&inv, &osp->len);
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
					isilog(L_DEBUG, "net-redis: loadobject data %lld\n", eitr);
					{
						isiInfo *linfo = (isiInfo *)pld->obj;
						if(eitr == 2) {
							linfo->nvsize = osp->len;
							linfo->nvstate = isi_calloc(linfo->nvsize);
							redis_cmd_load_data(istate, (uint8_t*)linfo->nvstate, linfo->nvsize);
						}
					}
					ncmd->param++;
					break;
				case REDIS_DATAEND:
					isilog(L_DEBUG, "net-redis: loadobject data success %lld\n", eitr);
					break;
				case REDIS_NIL:
					isilog(L_DEBUG, "net-redis: loadobject nil data i=%lld\n", eitr);
					break;
				case REDIS_CMDEXIT:
					if(!ncmd->param) {
						isilog(L_DEBUG, "net-redis: loadobject fail\n");
						session_async_end(ncmd, ISIERR_NOTFOUND);
						isi_delete_object(pld->obj);
					} else {
						isiInfo *linfo = (isiInfo *)pld->obj;
						linfo->otype = pld->ncid;
						linfo->uuid = pld->uuid;
						linfo->Load();
						isilog(L_DEBUG, "net-redis: loadobject success\n");
						session_async_end(ncmd, 0);
					}
					break;
				}
				break;
			}
			case ISIC_DISKLOAD:
				isilog(L_DEBUG, "net-redis: diskload\n");
				if(etype == REDIS_DATA) {
					isiInfo *disk = (isiInfo *)ncmd->cptr;
					uint8_t *blkdat;
					isi_disk_getblock(disk, (void**)&blkdat);
					redis_cmd_load_data(istate, blkdat, 4096);
				} else if(etype == REDIS_CMDEXIT) {
					isiInfo *disk = (isiInfo *)ncmd->cptr;
					uint32_t dm[] = {0x22, ncmd->param, 0};
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
			get_cmdq(NULL, 1); /* remove a command normally */
			get_cmdq(&ncmd, 0);
		}
	}
	recv_index -= (inv - in->u8_head);
	if(recv_index) {
		memmove(in->u8_head, inv, recv_index);
		inv = in->u8_head;
	}
	return 0;
}

