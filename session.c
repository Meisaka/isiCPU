
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "cputypes.h"
#include "netmsg.h"

extern int fdlisten;
extern struct isiDevTable alldev;
extern struct isiDevTable allcpu;
extern struct isiConTable allcon;
extern struct isiSessionTable allses;
extern struct isiObjTable allobj;
int isi_create_object(int objtype, struct objtype **out);

int makeserver(int portnumber)
{
	int fdsvr, i;
	struct sockaddr_in lipn;

	memset(&lipn, 0, sizeof(lipn));
	lipn.sin_family = AF_INET;
	lipn.sin_port = htons(portnumber);
	fdsvr = socket(AF_INET, SOCK_STREAM, 0);
	if(fdsvr < 0) { isilogerr("socket"); return -1; }
	i = 1;
	setsockopt(fdsvr, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int));
	i = 1;
	if( setsockopt(fdsvr, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		isilogerr("set'opt");
		close(fdsvr);
		return -1;
	}
	if( bind(fdsvr, (struct sockaddr*)&lipn, sizeof(struct sockaddr_in)) < 0 ) {
		isilogerr("bind");
		return -1;
	}
	if( listen(fdsvr, 1) < 0 ) {
		isilogerr("listen");
		return -1;
	}
	isilog(L_INFO, "Listening on port %d ...\n", portnumber);
	fdlisten = fdsvr;
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
	close(s->sfd);
	free(s->in);
	free(s->out);
	isi_delete_object(&s->id);
	return 0;
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
		isilogerr("accept");
		fdn = 0;
		return -1;
	}
	i = 1;
	if( setsockopt(fdn, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		isilogerr("set'opt");
		close(fdn);
		return -1;
	}
	if( fcntl(fdn, F_SETFL, O_NONBLOCK) < 0) {
		isilogerr("fcntl");
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
	if(ripn.sin_family == AF_INET) {
		union {
			uint8_t a[4];
			uint32_t la;
		} ipa;
		ipa.la = ripn.sin_addr.s_addr;
		isilog(L_INFO, "net-server: new IP session from: %d.%d.%d.%d:%d\n"
			, ipa.a[0], ipa.a[1], ipa.a[2], ipa.a[3], ntohs(ripn.sin_port)
		);
	}
	isi_pushses(ses);
	return 0;
}

int session_write_msg(struct isiSession *ses)
{
	int len;
	len = (*(uint32_t*)(ses->out)) & 0x1fff;
	if(len > 1300) {
		len = 1300;
		*(uint32_t*)(ses->out) = ((*(uint32_t*)ses->out) & 0xfff00000) | len;
	}
	while(len & 3) {
		ses->out[4+len] = 0;
		len++;
	}
	(*(uint32_t*)(ses->out+4+len)) = 0xFF8859EA;
	return send(ses->sfd, ses->out, 8+len, 0);
}

int session_write_msgex(struct isiSession *ses, void *buf)
{
	int len;
	len = (*(uint32_t*)(buf)) & 0x1fff;
	if(len > 1300) {
		len = 1300;
		*(uint32_t*)(buf) = ((*(uint32_t*)buf) & 0xfff00000) | len;
	}
	while(len & 3) {
		((char*)buf)[4+len] = 0;
		len++;
	}
	(*(uint32_t*)(((char*)buf)+4+len)) = 0xFF8859EA;
	return send(ses->sfd, (char*)buf, 8+len, 0);
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
		l = ((*(uint32_t*)ses->in) & 0x1fff);
		if(l > 1400) l = 1400;
		if(l & 3) l += 4 - (l & 3);
		l += 8;
		if(ses->rcv < l) {
			i = read(ses->sfd, ses->in+ses->rcv, l - ses->rcv);
		} else {
			isilog(L_WARN, "net-session: improper read\n");
		}
	}
	if(i < 0) { isilogerr("socket read"); return -1; }
	if(i == 0) { isilog(L_WARN, "net-session: empty read\n"); return -1; }
	if(ses->rcv < 4) {
		ses->rcv += i;
		if(ses->rcv >= 4) {
			l = ((*(uint32_t*)ses->in) & 0x1fff);
			if(l > 1400) l = 1400;
			if(l & 3) l += 4 - (l & 3);
			l += 8;
			if(ses->rcv < l) goto readagain;
		}
	}
	ses->rcv += i;
	if(ses->rcv < l) return 0;
	mc = *(uint32_t*)ses->in;
	uint32_t *pm = (uint32_t*)ses->in;
	uint32_t *pr = (uint32_t*)ses->out;
	l = mc & 0x1fff; mc >>= 20;
	if(l > 1400) l = 1400;
	if(*(uint32_t*)(ses->in+l+((l&3)?8-(l&3):4)) != 0xFF8859EA) {
		isilog(L_WARN, "\nnet-session: message framing invalid\n\n");
		return -1;
	}
	/* handle message here */
	switch(mc) {
	case ISIM_PING: /* keepalive/ping */
		break;
	case ISIM_GETOBJ: /* get all accessable objects */
	{
		size_t i;
		uint32_t ec = 0;
		isilog(L_DEBUG, "net-msg: [%08x]: list obj\n", ses->id.id);
		for(i = 0; i < allobj.count; i++) {
			struct objtype *obj = allobj.table[i];
			if(obj && obj->objtype >= 0x2000) {
				pr[1+ec] = obj->id;
				pr[2+ec] = obj->objtype;
				ec+=2;
			}
			if(ec > 160) {
				pr[0] = ISIMSG(R_GETOBJ, 0, ec*4);
				session_write_msg(ses);
				ec = 0;
			}
		}
		pr[0] = ISIMSG(L_GETOBJ, 0, ec*4);
		session_write_msg(ses);
	}
		break;
	case ISIM_SYNCALL: /* sync everything! */
		isi_resync_all();
		break;
	case ISIM_GETCLASSES:
	{
		size_t i;
		uint32_t ec = 0;
		uint8_t *bm = ses->out + 4;
		isilog(L_DEBUG, "net-msg: [%08x]: list classes\n", ses->id.id);
		for(i = 0; i < allcon.count; i++) {
			struct isiConstruct *con = allcon.table[i];
			size_t mln = strlen(con->name) + 1;
			size_t mld = strlen(con->desc) + 1;
			size_t mlt = 8 + mln + mld;
			uint32_t eflag = 0;
			if(mlt > 512) {
				isilog(L_INFO, "net-msg: class %04x name+desc too long %ld\n", con->objtype, mlt);
				continue;
			}
			if(ec + mlt > 1300) {
				pr[0] = ISIMSG(R_GETCLASSES, 0, ec);
				session_write_msg(ses);
				ec = 0;
			}
			memcpy(bm+ec, &con->objtype, 4);
			memcpy(bm+ec+4, &eflag, 4);
			memcpy(bm+ec+8, con->name, mln);
			memcpy(bm+ec+8+mln, con->desc, mld);
			ec+=mlt;
		}
		pr[0] = ISIMSG(L_GETCLASSES, 0, ec);
		session_write_msg(ses);
	}
		break;
	case ISIM_GETHEIR: /* get heirarchy */
	{
		size_t i;
		uint32_t ec = 0;
		isilog(L_DEBUG, "net-msg: [%08x]: list heir\n", ses->id.id);
		for(i = 0; i < allobj.count; i++) {
			struct objtype *obj = allobj.table[i];
			struct isiInfo *info;
			if(obj && obj->objtype >= 0x2f00) {
				info = (struct isiInfo*)obj;
				pr[1+ec] = obj->id;
				pr[2+ec] = info->dndev ? info->dndev->id.id : 0;
				pr[3+ec] = info->updev ? info->updev->id.id : 0;
				pr[4+ec] = info->mem ? ((struct objtype*)info->mem)->id : 0;
				ec+=4;
			}
			if(ec > 80) {
				pr[0] = ISIMSG(R_GETHEIR, 0, ec*4);
				session_write_msg(ses);
				ec = 0;
			}
		}
		pr[0] = ISIMSG(L_GETHEIR, 0, (ec*4));
		session_write_msg(ses);
	}
		break;
	case ISIM_NEWOBJ:
		if(l < 4) break;
		pr[0] = ISIMSG(R_NEWOBJ, 0, 12);
		pr[1] = 0;
		pr[2] = pm[1];
		{
			struct objtype *a;
			a = 0;
			pr[3] = (uint32_t)isi_make_object(pm[1], &a, ses->in+8, l - 4);
			if(a) {
				pr[1] = a->id;
			}
		}
		session_write_msg(ses); /* TODO multisession */
		break;
	case ISIM_DELOBJ:
		if(l < 4) break;
		break;
	case ISIM_ATTACH:
		if(l < 8) break;
		pr[0] = ISIMSG(R_ATTACH, 0, 8);
		pr[1] = pm[1];
		{
			struct objtype *a;
			struct objtype *b;
			if(isi_find_obj(pm[1], &a) || isi_find_obj(pm[2], &b)) {
				pr[2] = (uint32_t)ISIERR_NOTFOUND;
			} else if(a->objtype < 0x2f00){
				pr[2] = (uint32_t)ISIERR_INVALIDPARAM;
			} else {
				pr[2] = (uint32_t)isi_attach((struct isiInfo*)a, (struct isiInfo*)b);
			}
		}
		session_write_msg(ses);
		break;
	case ISIM_DEATTACH:
		if(l < 8) break;
		pr[0] = ISIMSG(R_ATTACH, 0, 8);
		pr[1] = pm[1];
		pr[2] = (uint32_t)ISIERR_FAIL;
		session_write_msg(ses);
		break;
	case ISIM_START:
		if(l < 4) break;
	{
		pr[0] = ISIMSG(R_START, 0, 8);
		pr[1] = pm[1];
		pr[2] = (uint32_t)ISIERR_FAIL;
		struct isiInfo *a;
		if(isi_find_dev(&allcpu, pm[1], &a)) {
			pr[2] = (uint32_t)ISIERR_NOTFOUND;
			if(isi_find_obj(pm[1], (struct objtype**)&a)) {
				pr[2] = (uint32_t)ISIERR_NOTFOUND;
			} else if(a->id.objtype >= 0x3000 && a->id.objtype < 0x3fff) {
				isi_push_dev(&allcpu, a);
				if(a->Reset) a->Reset(a);
				fetchtime(&a->nrun);
				isilog(L_INFO, "net-session: enabling CPU\n");
				pr[2] = 0;
			} else {
				pr[2] = (uint32_t)ISIERR_INVALIDPARAM;
			}
		} else {
			if(a->Reset) a->Reset(a);
			isilog(L_INFO, "net-session: resetting CPU\n");
			pr[2] = 0;
		}
		session_write_msg(ses); /* TODO multisession */
	}
		break;
	case ISIM_STOP:
		if(l < 4) break;
		pr[0] = ISIMSG(R_STOP, 0, 8);
		pr[1] = pm[1];
		pr[2] = (uint32_t)ISIERR_FAIL;
		session_write_msg(ses); /* TODO multisession */
		break;
	case ISIM_ATTACHAT:
		if(l < 16) break;
		pr[0] = ISIMSG(R_ATTACH, 0, 8);
		pr[1] = pm[1];
		pr[2] = (uint32_t)ISIERR_FAIL;
		session_write_msg(ses);
		break;
	case ISIM_MSGOBJ:
		if(l < 6) break;
	{
		struct isiInfo *info;
		if(isi_find_obj(pm[1], (struct objtype**)&info)) {
			isilog(L_INFO, "net-msg: [%08x]: not found [%08x]\n", ses->id.id, pm[1]);
			break;
		}
		if(info->id.objtype >= 0x2000) {
			if(info->MsgIn) {
				info->MsgIn(info, info->updev, (uint16_t*)(pm+2), mtime);
			}
		}
	}
		break;
	case ISIM_SYNCMEM16:
	case ISIM_SYNCMEM32:
	case ISIM_SYNCRVS:
	case ISIM_SYNCSVS:
		break;
	default:
		isilog(L_DEBUG, "net-session: [%08x]: 0x%03x +%05x\n", ses->id.id, mc, l);
		break;
	}
	/* *** */
	ses->rcv = 0;
	return 0;
}

