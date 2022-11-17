
#include "isitypes.h"
#include <vector>
extern std::vector<isiSession*> allses;

int persist_find_session(isiSession **fses) {
	uint32_t u = 0;
	while(u < allses.size()) {
		if(allses[u] && allses[u]->otype == ISIT_SESSION) {
			isiSession *ses = (isiSession *)allses[u];
			if(ses->stype == 2) {
				*fses = ses;
				return 0;
			}
		}
		u++;
	}
	return ISIERR_NOTFOUND;
}

int persist_load_object(uint32_t session, uint32_t cid, uint64_t uuid, uint32_t tid) {
	const isiConstruct *con = 0;
	isiSession *ses;
	isiCommand *ncmd;
	isiObject *obj;
	int r;
	if(persist_find_session(&ses)) return -1;
	if(ses->add_cmdq(&ncmd)) return ISIERR_BUSY;
	if(!isi_find_uuid(cid, uuid, NULL)) return ISIERR_LOADED;
	r = isi_create_object(cid, &con, &obj);
	if(r) return r;
	isiPLoad * plc = (isiPLoad*)isi_calloc(sizeof(isiPLoad));
	if(!plc) {
		isi_delete_object(obj);
		return ISIERR_NOMEM;
	}
	plc->ncid = cid;
	plc->uuid = uuid;
	plc->obj = obj;
	ncmd->cmd = ISIC_LOADOBJECT;
	ncmd->id = session;
	ncmd->tid = tid;
	ncmd->rdata = plc;
	obj->otype = ISIT_PRELOAD;
	return 0;
}

int persist_disk(isiInfo *info, uint32_t rdblk, uint32_t wrblk, int mode) {
	isiSession *ses;
	isiCommand *ncmd;
	if(persist_find_session(&ses)) return -1;
	if(ses->add_cmdq(&ncmd)) return ISIERR_BUSY;
	switch(mode & 3) {
	case 0:
		return -1;
	case 1:
		ncmd->cmd = ISIC_DISKLOAD;
		break;
	case 2:
		ncmd->cmd = ISIC_DISKWRITE;
		break;
	case 3:
		ncmd->cmd = ISIC_DISKWRLD;
		break;
	}
	ncmd->id = info->id;
	ncmd->cptr = info;
	ncmd->tid = wrblk;
	ncmd->param = rdblk;
	return 0;
}

