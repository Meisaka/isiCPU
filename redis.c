
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
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
				const char *testvec = "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n";
				write(ses->sfd, testvec, strlen(testvec));
			}
		}
		u++;
	}
}

static int redis_handle_rd(struct isiSession *ses, struct timespec mtime)
{
	int i;
	int l;
	l = 1024;
	i = read(ses->sfd, ses->in+ses->rcv, l-ses->rcv);
	if(i < 0) {
		if(i == EAGAIN || i == EWOULDBLOCK) return 0;
		isilogerr("socket read");
		return -1;
	}
	ses->in[i] = 0;
	isilog(L_DEBUG, "net-redis: got message l=%d\n", i);
	fprintf(stderr, "net-redis: %s\n", ses->in);
	return 0;
}

