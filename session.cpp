
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include "isitypes.h"
#include "netmsg.h"
#include "platform.h"
#include <forward_list>

extern std::vector<isiInfo*> alldev;
extern std::vector<isiInfo*> allcpu;
extern std::vector<const isiConstruct*> allcon;
extern std::vector<isiObjSlot> allobj;
extern std::vector<isiSession*> allses;
int isi_pop_cpu(isiInfo *d);

isiMsg * msgfreelist = nullptr;

struct cemei_svstate {
	uint32_t sessionid;
	uint32_t index;
	isiSession *ses;
};
ISIREFLECT(struct cemei_svstate,
	ISIR(cemei_svstate, uint32_t, sessionid)
	ISIR(cemei_svstate, uint32_t, index)
)
struct CEMEI : public isiInfo {
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, uint32_t len, isi_time_t mtime);
	virtual int QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint);
};
int CEMEI::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, uint32_t len, isi_time_t mtime) {
	struct cemei_svstate *dev = (struct cemei_svstate*)this->svstate;
	isiSession *ses = dev->ses;
	if(!dev->sessionid || !ses || ses->otype != ISIT_SESSION || ses->id != dev->sessionid) return ISIERR_FAIL;
	if(lsindex < 0) return 0;
	isiMsgRef out = make_msg();
	memcpy(out->u32+1, msg, len * 2);
	out->length = 4 + (len * 2);
	out->code = ISIMSG(MSGCHAN, 0);
	out->u32[0] = (uint32_t)lsindex;
	ses->write_msg(out);
	return 0;
}

int CEMEI::QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint) {
	if(topoint == ISIAT_UP) return ISIERR_NOCOMPAT;
	return 0;
}

static uint32_t cemei_meta[] = {0,0,0,0};
static isiClass<CEMEI> CEMEI_C(
	ISIT_CEMEI, "<CEMEI>", "Message Exchange Interface",
	NULL, &ISIREFNAME(struct cemei_svstate), NULL, &cemei_meta);

void cemei_register() {
	isi_register(&CEMEI_C);
}

isiMsg::isiMsg() {
	next = nullptr;
	r_limit = 0x2020;
	buf_alloc = (uint32_t*)isi_calloc(r_limit);
	limit = 0x1fff;
	reset();
}
void isiMsg::reset() {
	code = 0;
	length = 0;
	txid = 0;
	sequence = 0;
	mark = nullptr;
	u32 = buf_alloc + 5;
	u32_head = buf_alloc + 4;
}
void push_free_msg(isiMsg *m) {
	m->next = msgfreelist;
	msgfreelist = m;
}
isiMsg * pop_free_msg() {
	isiMsg *m = msgfreelist;
	if(m) {
		msgfreelist = m->next;
		m->next = nullptr;
		m->reset();
	}
	return m;
}

void isiMessageReturn::operator()(isiMsg *m) noexcept {
	if(m) push_free_msg(m);
}

isiMsgRef make_msg() {
	if(!msgfreelist) {
		isilog(L_DEBUG, "msg-allocate: +32\n");
		for(int i = 0; i < 32; i++) {
			isiMsg *msg = new isiMsg();
			push_free_msg(msg);
		}
	}
	return isiMsgRef(pop_free_msg());
}

int makeserver(int portnumber) {
	fdesc_t fdsvr;
	int i;
	struct sockaddr_in lipn;

	memset(&lipn, 0, sizeof(lipn));
	lipn.sin_family = AF_INET;
	lipn.sin_port = htons(portnumber);
	fdsvr = platform_socket(AF_INET, SOCK_STREAM, 0);
	if(fdsvr == fdesc_empty) { isilogneterr("socket"); return -1; }
	i = 1;
	setsockopt(fdsvr, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int));
	i = 1;
	if( setsockopt(fdsvr, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		isilogneterr("set'opt");
		close(fdsvr);
		return -1;
	}
	if( bind(fdsvr, (struct sockaddr*)&lipn, sizeof(struct sockaddr_in)) < 0 ) {
		isilogneterr("bind");
		return -1;
	}
	if( listen(fdsvr, 1) < 0 ) {
		isilogneterr("listen");
		return -1;
	}
	isilog(L_INFO, "Listening on port %d ...\n", portnumber);
	isiSession *ses;
	if((i= isi_create_object(ISIT_SESSION_SERVER, NULL, (isiObject **)&ses))) {
		return i;
	}
	ses->r_addr = (net_endpoint*)isi_calloc(sizeof(struct sockaddr_in));
	memcpy(ses->r_addr, &lipn, sizeof(struct sockaddr_in));
	ses->sfd = fdsvr;
	allses.push_back(ses);
	return 0;
}

int isi_message_ses(struct isiSessionRef *sesref, uint32_t oid, uint32_t *msg, uint32_t len) {
	if(!sesref->id) {
		isilog(L_DEBUG, "msg-ses: session not set\n");
		return -1;
	}
	isiSession *ses = NULL;
	if(sesref->index >= allses.size() || nullptr == (ses = allses[sesref->index]) || sesref->id != ses->id) {
		uint32_t nindex = ~0;
		/* index is bad, try and find it again */
		for(uint32_t i = 0; i < allses.size(); i++) {
			if(nullptr != (ses = allses[i])) {
				if(ses->id == sesref->id) {
					nindex = i;
					break;
				}
			}
		}
		if(nindex == ~0) {
			/* if not found, the session went away */
			sesref->id = 0;
			sesref->index = 0;
			isilog(L_INFO, "msg-ses: Session find failed\n");
			return -1;
		}
	}
	if(!ses || ses->otype != ISIT_SESSION) {
		isilog(L_WARN, "msg-ses: Session type invalid\n");
		return -1; /* just in case */
	}
	isiMsgRef out = make_msg();
	constexpr uint32_t word_width = sizeof(uint32_t);
	if((len * word_width) > out->limit) len = out->limit / word_width;
	out->code = ISIMSG(MSGOBJ, 0);
	out->length = len * word_width;
	out->u32[0] = oid;
	memcpy(out->u32+1, msg, len * word_width);
	ses->write_msg(out);
	return 0;
}

int session_async_end(isiCommand *cmd, int result) {
	uint32_t sid = cmd->id;
	for(auto &ses : allses) {
		if(ses && ses->id == sid)
			return ses->AsyncDone(cmd, result);
	}
	return -1;
}

int isi_delete_ses(isiSession *s) {
	if(!s) return -1;
	auto itr = allses.cbegin();
	for(; itr != allses.cend();itr++) {
		if(*itr == s) {
			allses.erase(itr);
			break;
		}
	}
	shutdown(s->sfd, SHUT_RDWR); /* shutdown on buffered streams */
	if(s->ccmei && s->ccmei->otype == ISIT_CEMEI && s->ccmei->svstate) {
		struct cemei_svstate *dev = (struct cemei_svstate*)s->ccmei->svstate;
		dev->ses = NULL;
		dev->sessionid = 0;
	}
	free(s->r_addr);
	close(s->sfd);
	if(s->cmdq) free(s->cmdq);
	isi_delete_object(s);
	return 0;
}

class isiSessionServer : public isiSession {
public:
	int Recv(isi_time_t mtime);
	int STick(isi_time_t mtime) { return 1; }
	int LTick(isi_time_t mtime) { return 1; }
	int AsyncDone(isiCommand *cmd, int result) { return 0; }
};
class isiSessionClient : public isiSession {
public:
	isiSessionClient() {}
	int Recv(isi_time_t mtime);
	int LTick(isi_time_t mtime);
	int AsyncDone(isiCommand *cmd, int result);
private:
	int handle_hello(isiMsgRef, isi_time_t mtime);
	int handle_recv(isiMsgRef, isi_time_t mtime);
	bool read_hello;
};
static isiClass<isiSessionClient> isiSession_C(ISIT_SESSION, "<isiClient>", "");
static isiClass<isiSessionServer> isiSessionServer_C(ISIT_SESSION_SERVER, "<isiSessionServer>", "");
void isi_register_server() {
	isi_register(&isiSession_C);
	isi_register(&isiSessionServer_C);
}

int isiSessionServer::Recv(isi_time_t mtime) {
	fdesc_t fdn;
	int i;
	socklen_t rin;
	struct sockaddr_in ripn;
	memset(&ripn, 0, sizeof(ripn));
	rin = sizeof(ripn);
	fdn = accept(this->sfd, (struct sockaddr*)&ripn, &rin);
	if(fdn == fdesc_empty) {
		isilogneterr("accept");
		return -1;
	}
	i = 1;
	if( setsockopt(fdn, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) < 0) {
		isilogneterr("set'opt");
		close(fdn);
		return -1;
	}
	if(set_nonblock(fdn)) {
		isilogneterr("fcntl");
		close(fdn);
		return -1;
	}
	isiSession *ses;
	if((i= isi_create_object(ISIT_SESSION, NULL, (isiObject **)&ses))) {
		return i;
	}
	ses->sfd = fdn;
	ses->r_addr = (net_endpoint*)isi_calloc(sizeof(struct sockaddr_in));
	memcpy(ses->r_addr, &ripn, sizeof(struct sockaddr_in));
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
	ses->stype = 1;
	allses.push_back(ses);
	return 0;
}

isiCommand::~isiCommand() {
	if(rdata) {
		free(rdata);
		rdata = nullptr;
	}
	if(xdata) {
		free(xdata);
		xdata = nullptr;
	}
}

int isiSession::get_cmdq(isiCommand **ncmd, int remove) {
	if(!cmdq || !cmdqlimit) return -1;
	if(cmdqstart == cmdqend) return -1; /* ring is empty */
	if(ncmd) {
		*ncmd = cmdq + cmdqstart;
	}
	if(remove) {
		if(!ncmd) {
			(cmdq + cmdqstart)->~isiCommand();
		}
		uint32_t ncp = cmdqstart + 1;
		if(ncp >= cmdqlimit) ncp = 0;
		cmdqstart = ncp;
	}
	return 0;
}

int isiSession::add_cmdq(isiCommand **ncmd) {
	if(!cmdq || !cmdqlimit) return -1;
	uint32_t ncp = cmdqend + 1;
	if(ncp >= cmdqlimit) ncp = 0;
	if(ncp == cmdqstart) return -1; /* ring is full */
	if(!ncmd) return 0; /* ncmd not specified, just return if possible to add */
	memset(cmdq+ncp, 0, sizeof(isiCommand));
	*ncmd = cmdq + cmdqend;
	cmdqend = ncp;
	return 0;
}

int isiSession::LTick(isi_time_t mtime) { return 0; }
int isiSession::STick(isi_time_t mtime) { return 0; }
int isiSession::Recv(isi_time_t mtime) { return -1; }
int isiSession::AsyncDone(isiCommand *cmd, int result) { return -1; }

int isiSession::write_msg(isiMsgRef &msgref) {
	if(this->stype != 1) return -1;
	isiMsg *msg = msgref.release();
	msg->length = msg->length & 0x1fffu;
	msg->u32_head[0] = (msg->code & 0xfff00000) | msg->length;
	while(msg->length & 3) {
		msg->u8[msg->length++] = 0;
	}
	*(uint32_t*)(msg->u8 + msg->length) = 0xFF8859EA;
	// TODO cache remaining message when result < length
	int sent_result = send(this->sfd, (char*)msg->u32_head, 8 + msg->length, 0);
	push_free_msg(msg);
	return sent_result;
}

void isiSession::multi_write_msg(const isiMsg *msg) {
	if(this->stype != 1) return;
	// TODO cache remaining message when result < length
	int sent_result = send(this->sfd, (char*)msg->u32_head, msg->length, 0);
}

void session_multicast_msg(isiMsgRef &msgref) {
	isiMsg *msg = msgref.release();
	msg->length = msg->length & 0x1fffu;
	msg->u32_head[0] = (msg->code & 0xfff00000) | msg->length;
	while(msg->length & 3) {
		msg->u8[msg->length++] = 0;
	}
	*(uint32_t*)(msg->u8 + msg->length) = 0xFF8859EA;
	msg->length += 8;
	for(auto ses : allses) {
		if(!ses) continue;
		ses->multi_write_msg(msg);
	}
}

int isiSessionClient::LTick(isi_time_t mtime) {
	isiMsgRef out = make_msg();
	out->code = ISIMSG(PING, 0);
	out->length = 0;
	return write_msg(out);
}

int isiSessionClient::AsyncDone(isiCommand *cmd, int result) {
	isiMsgRef out = make_msg();
	switch(cmd->cmd) {
	case ISIC_LOADOBJECT:
	{
		struct isiPLoad *pld = (struct isiPLoad *)cmd->rdata;
		out->code = ISIMSG(R_LOADOBJ, 0);
		out->u32[0] = cmd->tid;
		out->u32[1] = result;
		out->u32[2] = result? 0 : pld->obj->id;
		out->u32[3] = pld->ncid;
		*(uint64_t*)(out->u32+4) = pld->uuid;
		out->length = 24;
		write_msg(out);
	}
		break;
	}
	return 0;
}

int isiSessionClient::Recv(isi_time_t mtime) {
	int i;
	if(!in) {
		in = make_msg();
		recv_index = 0;
		frame_length = 0;
		in->u32_head = in->buf_alloc;
	}
	i = recv(this->sfd, (char *)(in->buf_alloc) + recv_index, in->r_limit - recv_index, 0);
	if(i <= 0) {
		if(i == 0) {
			isilog(L_WARN, "net-session: empty read\n");
			return -1;
		}
		i = get_net_error();
		if(i == EAGAIN || i == EWOULDBLOCK) return 0;
		isilogneterr("socket read");
		return -1;
	}
	recv_index += i;
	if((frame_length == 0) && (recv_index >= 4)) {
		uint32_t f_length = in->u32_head[0] & 0x1fff;
		if(f_length & 3) f_length += 4 - (f_length & 3);
		frame_length = f_length + 8;
	}
	if(recv_index < frame_length) return 0;
	if(recv_index > frame_length) {
		// TODO re:re:write this to split messages if we over receive.
		isilog(L_ERR, "\nnet-session: TODO multi message framing\n\n");
	}
	
	in->length = in->u32_head[0] & 0x1fff;
	in->code = in->u32_head[0] >> 20;
	in->u32 = in->u32_head + 1;
	if(in->u32[((in->length & 3) ? 1 : 0) + (in->length >> 2)] != 0xFF8859EA) {
		isilog(L_WARN, "\nnet-session: message framing invalid\n\n");
		return -1;
	}
	if(read_hello)
		i = handle_recv(std::move(in), mtime);
	else
		i = handle_hello(std::move(in), mtime);
	recv_index = 0;
	frame_length = 0;
	return i;
}

/* handle hello/pre-hello here */
int isiSessionClient::handle_hello(isiMsgRef inref, isi_time_t mtime) {
	isiMsgRef out = make_msg();
	isiMsg *in = inref.get();
	switch(in->code) {
	case ISIM_R_PING: /* ping response */
		/* ignore these, the session is alive */
		break;
	case ISIM_KEEPALIVE:
	case ISIM_PING: /* keepalive/ping */
	{
		if(in->length > 16) in->length = 16;
		for(size_t i = 0; i < (in->length >> 2); i++) {
			out->u32[i] = in->u32[i];
		}
		out->code = ISIMSG(R_PING, 0);
		out->length = in->length;
		write_msg(out);
		break;
	}
	case ISIM_HELLO:
	{
		out->code = ISIMSG(HELLO, 0);
		out->length = 22;
		out->u16[0] = 2;
		out->u16[1] = 0;
		out->u32[1] = 0xEC000151;
		out->u32[2] = in->u32[2];
		out->u32[3] = in->u32[3];
		out->u8[20] = 0;
		out->u8[21] = 0;
		if((in->length != 22) ||
			(in->u16[0] != 2) ||
			(in->u16[1] != 0)) {
			out->u32[4] = 0;
		} else {
			out->u32[4] = this->id;
			read_hello = true;
		}
		write_msg(std::move(out));
		break;
	}
	default:
		out->code = ISIMSG(GOODBYE, 0);
		out->length = 4;
		out->u32[0] = 1;
		write_msg(std::move(out));
		isilog(L_DEBUG, "net-session: [%08x]: 0x%03x +%05x before hello\n", this->id, in->code, in->length);
		return -1;
	}
	return 0;
}
/* handle message here */
int isiSessionClient::handle_recv(isiMsgRef inref, isi_time_t mtime) {
	isiMsgRef out = make_msg();
	isiMsg *in = inref.get();
	switch(in->code) {
	case ISIM_R_PING: /* ping response */
		/* ignore these, the session is alive */
		break;
	case ISIM_PING: /* keepalive/ping */
	{
		if(in->length > 16) in->length = 16;
		for(size_t i = 0; i < (in->length >> 2); i++) {
			out->u32[i] = in->u32[i];
		}
		out->code = ISIMSG(R_PING, 0);
		out->length = in->length;
		write_msg(out);
	}
		break;
	case ISIM_GETOBJ: /* get all accessable objects */
	{
		uint32_t *r_ptr = out->u32;
		isilog(L_DEBUG, "net-msg: [%08x]: list obj\n", this->id);
		for(auto &oslot : allobj) {
			isiObject *obj = oslot.ptr;
			if(!obj || obj->otype == 0) {
				continue;
			}
			if((out->length + 8) > out->limit) {
				out->code = ISIMSG(R_GETOBJ, 0);
				write_msg(out);
				out = make_msg();
				r_ptr = out->u32;
			}
			r_ptr[0] = obj->id;
			r_ptr[1] = obj->otype;
			r_ptr += 2;
			out->length += 8;
		}
		out->code = ISIMSG(R_GETOBJ, 0);
		write_msg(out);
	}
		break;
	case ISIM_SYNCALL: /* sync everything! */
		isi_resync_all();
		break;
	case ISIM_GETCLASSES:
	{
		uint32_t offset = 0;
		isilog(L_DEBUG, "net-msg: [%08x]: list classes\n", this->id);
		for(auto con : allcon) {
			size_t namelen = con->name.size()+1;
			size_t desclen = con->desc.size()+1;
			size_t reclen = 8 + namelen + desclen;
			uint32_t eflag = 0;
			if(reclen > 512) {
				isilog(L_INFO, "net-msg: class %04x name+desc too long %ld\n", con->otype, reclen);
				continue;
			}
			if(offset + reclen > out->limit) {
				out->code = ISIMSG(R_GETCLASSES, 0);
				out->length = offset;
				write_msg(out);
				out = make_msg();
				offset = 0;
			}
			memcpy(out->u8+offset, &con->objtype, sizeof(uint32_t));
			memcpy(out->u8+offset+4, &eflag, sizeof(uint32_t));
			memcpy(out->u8+offset+8, con->name.data(), namelen);
			memcpy(out->u8+offset+8+namelen, con->desc.data(), desclen);
			offset += reclen;
		}
		out->code = ISIMSG(R_GETCLASSES, 0);
		out->length = offset;
		write_msg(out);
	}
		break;
	case ISIM_GETHEIR: /* get heirarchy */
	{
		uint32_t *r_ptr = out->u32;
		constexpr uint32_t rec_size = sizeof(uint32_t) * 3;
		isilog(L_DEBUG, "net-msg: [%08x]: list heir\n", this->id);
		for(auto &oslot : allobj) {
			isiObject *obj = oslot.ptr;
			isiInfo *info;
			if(!obj) continue;
			if((out->length + rec_size) > out->limit) {
				out->code = ISIMSG(R_GETHEIR, 0);
				write_msg(out);
				out = make_msg();
				r_ptr = out->u32;
			}
			if(isi_is_infodev(obj)) {
				info = (isiInfo*)obj;
				r_ptr[0] = obj->id;
				r_ptr[1] = info->updev.t ? info->updev.t->id : 0;
				r_ptr[2] = info->mem ? ((isiObject*)info->mem)->id : 0;
				r_ptr += 3;
				out->length += rec_size;
			}
		}
		out->code = ISIMSG(R_GETHEIR, 0);
		write_msg(out);
	}
		break;
	case ISIM_REQNVS:
	{
		if(in->length < 8) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			out->u32[0] = in->u32[0];
			out->u32[1] = 0;
			out->code = ISIMSG(SYNCNVSO, 0);
			out->length = 8;
			break; /* not found */
		} else if(isi_is_infodev(a)) {
			uint32_t offs = 0;
			size_t rqlen = a->nvsize;
			rqlen = in->u32[1];
			if(in->length >= 12) offs = in->u32[2];
			out->u32[0] = in->u32[0];
			if(rqlen + offs > a->nvsize) {
				rqlen = a->nvsize - offs;
			}
			if(!rqlen || offs + rqlen > a->nvsize || !a->nvstate || !a->nvsize) {
				out->u32[0] = in->u32[0];
				out->u32[1] = 0;
				out->code = ISIMSG(SYNCNVSO, 0);
				out->length = 8;
				break; /* empty nvstate found or parameters bad */
			}
			/* TODO: try and break this */
			while(rqlen && offs < a->nvsize && a->nvstate) {
				out->u32[1] = offs;
				uint32_t flen = rqlen;
				if(flen > a->nvsize - offs) flen = a->nvsize - offs;
				if(flen > out->limit) flen = out->limit;
				memcpy(out->u32+2, ((const uint8_t*)a->nvstate) + offs, flen);
				rqlen -= flen; offs += flen;
				out->code = ISIMSG(SYNCNVSO, 0);
				out->length = 8 + flen;
				write_msg(out);
			}
		}
	}
		break;
	case ISIM_NEWOBJ:
	{
		if(in->length < 4) break;
		out->code = ISIMSG(R_NEWOBJ, 0);
		out->length = 12;
		out->u32[1] = 0;
		out->u32[2] = in->u32[0];
		isiObject *a;
		a = 0;
		out->u32[0] = (uint32_t)isi_make_object(in->u32[0], &a, (uint8_t*)(in->u32+1), in->length - 4);
		if(a) {
			out->u32[1] = a->id;
		} else {
			isilog(L_WARN, "net-session: create object: error %08x\n", in->u32[2]);
		}
		write_msg(out); /* TODO multisession */
		break;
	}
	case ISIM_DELOBJ:
		if(in->length < 4) break;
		isilog(L_WARN, "net-session: unimplemented: delete object\n");
		break;
	case ISIM_ATTACH:
	{
		if(in->length < 8) break;
		out->code = ISIMSG(R_ATTACH, 0);
		out->length = 20;
		out->u32[1] = in->u32[0];
		out->u32[2] = in->u32[1];
		out->u32[3] = (uint32_t)ISIAT_APPEND;
		out->u32[4] = (uint32_t)ISIAT_UP;
		isiObject *a;
		isiObject *b;
		if(isi_find_obj(in->u32[0], &a) || isi_find_obj(in->u32[1], &b)) {
			out->u32[0] = (uint32_t)ISIERR_NOTFOUND;
		} else if(!isi_is_infodev(a)){
			out->u32[0] = (uint32_t)ISIERR_INVALIDPARAM;
		} else {
			out->u32[0] = (uint32_t)isi_attach((isiInfo*)a, ISIAT_APPEND, (isiInfo*)b, ISIAT_UP, (int32_t*)(out->u32+3), (int32_t*)(out->u32+4));
		}
		write_msg(out);
		break;
	}
	case ISIM_DEATTACH:
	{
		if(in->length < 8) break;
		out->code = ISIMSG(R_ATTACH, 0);
		out->length = 8;
		out->u32[1] = in->u32[0];
		out->u32[0] = (uint32_t)ISIERR_NOTSUPPORTED;
		isiObject *a;
		if(isi_find_obj(in->u32[0], &a)) {
			out->u32[0] = (uint32_t)ISIERR_NOTFOUND;
		} else {
			out->u32[0] = (uint32_t)isi_deattach((isiInfo*)a, (int32_t)in->u32[1]);
		}
		write_msg(out);
		break;
	}
	case ISIM_START:
	{
		if(in->length < 4) break;
		out->code = ISIMSG(R_START, 0);
		out->length = 8;
		out->u32[0] = (uint32_t)ISIERR_FAIL;
		out->u32[1] = in->u32[0];
		isiInfo *a;
		if(isi_find_cpu(in->u32[0], &a, 0)) {
			if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
				out->u32[0] = (uint32_t)ISIERR_NOTFOUND;
				isilog(L_WARN, "net-session: reset: not-found %08x\n", out->u32[0]);
			} else if(isi_is_cpu(a)) {
				//pr[2] = 0; // if RunCycles
				out->u32[0] = (uint32_t)a->Reset();
				if(!out->u32[0]) {
					allcpu.push_back(a);
					isi_fetch_time(&a->nrun);
					isilog(L_INFO, "net-session: enabling CPU\n");
				}
			} else {
				out->u32[0] = (uint32_t)ISIERR_INVALIDPARAM;
				isilog(L_WARN, "net-session: reset: invalid-param %08x\n", out->u32[0]);
			}
		} else {
			isilog(L_INFO, "net-session: resetting CPU\n");
			out->u32[0] = (uint32_t)a->Reset();
		}
		write_msg(out); /* TODO multisession */
		break;
	}
	case ISIM_STOP:
	{
		if(in->length != 4) break;
		out->code = ISIMSG(R_STOP, 0);
		out->length = 8;
		out->u32[1] = in->u32[0];
		out->u32[0] = (uint32_t)ISIERR_FAIL;
		isiInfo *a;
		size_t index = 0;
		if(!isi_find_cpu(in->u32[0], &a, &index)) {
			isilog(L_INFO, "net-session: stopping CPU\n");
			a->Reset();
			out->u32[0] = (uint32_t)isi_pop_cpu(a);
		} else {
			out->u32[0] = (uint32_t)ISIERR_NOTFOUND;
		}
		write_msg(out); /* TODO multisession */
		break;
	}
	case ISIM_ATTACHAT:
	{
		if(in->length != 16) break;
		out->code = ISIMSG(R_ATTACH, 0);
		out->length = 20;
		out->u32[1] = in->u32[0];
		out->u32[2] = in->u32[1];
		out->u32[3] = in->u32[2];
		out->u32[4] = in->u32[3];
		isiObject *a;
		isiObject *b;
		if(isi_find_obj(in->u32[0], &a) || isi_find_obj(in->u32[1], &b)) {
			out->u32[0] = (uint32_t)ISIERR_NOTFOUND;
		} else {
			out->u32[0] = (uint32_t)isi_attach((isiInfo*)a, (int32_t)in->u32[2], (isiInfo*)b, (int32_t)in->u32[3], (int32_t*)(out->u32+3), (int32_t*)(out->u32+4));
		}
		write_msg(out);
		break;
	}
	case ISIM_LOADOBJ:
		if(in->length != 12) break;
		out->u32[0] = (uint32_t)persist_load_object(this->id, in->u32[0], *(uint64_t*)(in->u32+1), in->txid);
		if(out->u32[0]) {
			out->txid = in->u32[0];
			out->u32[1] = 0;
			out->u32[2] = in->u32[1];
			out->u32[3] = out->u32[4] = 0;
			out->code = ISIMSG(R_LOADOBJ, 0);
			out->length = 20;
			write_msg(out);
		}
		break;
	case ISIM_MSGOBJ:
	{
		if(in->length < 6) break;
		isiInfo *info;
		if(isi_find_obj(in->u32[0], (isiObject**)&info)) {
			isilog(L_INFO, "net-msg: [%08x]: not found [%08x]\n", this->id, in->u32[0]);
			break;
		}
		if(((uint16_t*)(in->u32+1))[0] == ISE_SUBSCRIBE) {
			if(info->otype == ISIT_CEMEI) {
				struct cemei_svstate *cem = (struct cemei_svstate*)info->svstate;
				if(cem->sessionid && cem->ses) {
					if(cem->ses->otype == ISIT_SESSION && cem->ses->id == cem->sessionid) {
						cem->ses->ccmei = NULL;
						cem->ses = NULL;
						cem->sessionid = 0;
					}
				}
				cem->ses = this;
				cem->sessionid = this->id;
				this->ccmei = info;
			} else if(isi_is_infodev(info)) {
				isilog(L_INFO, "net-session: [%08x]: EMEI subscribe\n", this->id);
				info->sesref.id = this->id;
				info->sesref.index = 0;
			}
		} else if(isi_is_infodev(info)) {
			isi_message_dev(info, -1, in->u32+1, 5, mtime);
		}
	}
		break;
	case ISIM_MSGCHAN:
	{
		if(in->length < 6) break;
		int32_t chan;
		chan = (int32_t)in->u32[0];
		if(chan < 0) {
			isilog(L_INFO, "net-msg: bad channel (%d)\n", chan);
			break;
		}
		if(this->ccmei) {
			isi_message_dev(this->ccmei, chan, in->u32+1, (in->length - 4) / 4, mtime);
		} else {
			isilog(L_INFO, "net-session: [%08x]: no CEMEI subscribed\n", this->id);
		}
	}
		break;
	case ISIM_SYNCMEM16:
	{
		if(in->length < 9) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_memory(a)) {
			uint8_t *psm = in->u8 + 4;
			uint8_t *pse = in->u8 + in->length;
			isiMemory *mem = (isiMemory*)a;
			while(psm < pse) {
				uint32_t addr, mlen;
				addr = (*(uint16_t*)psm); psm += 2;
				mlen = (*(uint16_t*)psm); psm += 2;
				if(psm + mlen < pse) {
					mem->sync_wrblock(addr, mlen, psm);
				}
				psm += mlen;
			}
		}
		break;
	}
	case ISIM_SYNCMEM32:
	{
		if(in->length < 11) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_memory(a)) {
			uint8_t *psm = in->u8 + 4;
			uint8_t *pse = in->u8 + in->length;
			isiMemory *mem = (isiMemory*)a;
			while(psm < pse) {
				uint32_t addr, mlen;
				addr = (*(uint32_t*)psm); psm += 4;
				mlen = (*(uint16_t*)psm); psm += 2;
				if(psm + mlen < pse) {
					mem->sync_wrblock(addr, mlen, psm);
				}
				psm += mlen;
			}
		}
		break;
	}
	case ISIM_SYNCRVS:
	{
		if(in->length < 5) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_infodev(a)) {
			size_t plen = in->length - 4;
			if(a->meta->rvproto && plen < a->meta->rvproto->length && a->rvstate) {
				memcpy(a->rvstate, in->u32 + 1, plen);
			}
		}
		break;
	}
	case ISIM_SYNCSVS:
	{
		if(in->length < 5) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_infodev(a)) {
			size_t plen = in->length - 4;
			if(a->meta->svproto && plen < a->meta->svproto->length && a->svstate) {
				memcpy(a->svstate, in->u32 + 1, plen);
			}
		}
		break;
	}
	case ISIM_SYNCNVSO:
	{
		if(in->length < 9) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_infodev(a)) {
			size_t plen = in->length - 8;
			size_t offs = in->u32[1];
			if(plen + offs < a->nvsize && a->nvstate) {
				memcpy(((uint8_t*)a->nvstate) + offs, in->u32 + 2, plen);
			}
		}
		break;
	}
	case ISIM_SYNCRVSO:
	{
		if(in->length < 9) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_infodev(a)) {
			size_t plen = in->length - 8;
			size_t offs = in->u32[1];
			if(a->meta->rvproto && plen + offs < a->meta->rvproto->length && a->rvstate) {
				memcpy(((uint8_t*)a->rvstate) + offs, in->u32 + 2, plen);
			}
		}
		break;
	}
	case ISIM_SYNCSVSO:
	{
		if(in->length < 9) break;
		isiInfo *a;
		if(isi_find_obj(in->u32[0], (isiObject**)&a)) {
			break; /* not found */
		} else if(isi_is_infodev(a)) {
			size_t plen = in->length - 8;
			size_t offs = in->u32[1];
			if(a->meta->svproto && plen + offs < a->meta->svproto->length && a->svstate) {
				memcpy(((uint8_t*)a->svstate) + offs, in->u32 + 2, plen);
			}
		}
		break;
	}
	default:
		isilog(L_DEBUG, "net-session: [%08x]: 0x%03x +%05x\n", this->id, in->code, in->length);
		break;
	}
	return 0;
}

