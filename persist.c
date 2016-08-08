
#include "cputypes.h"
extern struct isiSessionTable allses;
int isi_premake_object(int objtype, struct isiConstruct **outcon, struct objtype **out);

int persist_find_session(struct isiSession **fses)
{
	uint32_t u = 0;
	while(u < allses.count) {
		if(allses.table[u] && allses.table[u]->id.objtype == ISIT_SESSION) {
			struct isiSession *ses = (struct isiSession *)allses.table[u];
			if(ses->stype == 2) {
				*fses = ses;
				return 0;
			}
		}
		u++;
	}
	return ISIERR_NOTFOUND;
}

int persist_load_object(uint32_t session, uint32_t cid, uint64_t uuid, uint32_t tid)
{
	struct isiConstruct *con = 0;
	struct isiSession *ses;
	struct sescommandset *ncmd;
	struct objtype *obj;
	int r;
	if(persist_find_session(&ses)) return -1;
	if(session_add_cmdq(ses, &ncmd)) return ISIERR_BUSY;
	if(!isi_find_uuid(cid, uuid, NULL)) return ISIERR_LOADED;
	r = isi_premake_object(cid, &con, &obj);
	if(r) return r;
	struct isiPLoad * plc = isi_alloc(sizeof(struct isiPLoad));
	if(!plc) {
		isi_delete_object(obj);
		return ISIERR_NOMEM;
	}
	memset(plc, 0, sizeof(struct isiPLoad));
	plc->ncid = cid;
	plc->uuid = uuid;
	plc->obj = obj;
	ncmd->cmd = ISIC_LOADOBJECT;
	ncmd->id = session;
	ncmd->tid = tid;
	ncmd->rdata = plc;
	obj->objtype = ISIT_PRELOAD;
	return 0;
}

int persist_disk(struct isiInfo *info, uint32_t rdblk, uint32_t wrblk, int mode)
{
	struct isiSession *ses;
	struct sescommandset *ncmd;
	if(persist_find_session(&ses)) return -1;
	if(session_add_cmdq(ses, &ncmd)) return ISIERR_BUSY;
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
	ncmd->id = info->id.id;
	ncmd->cptr = info;
	ncmd->tid = wrblk;
	ncmd->param = rdblk;
	return 0;
}

