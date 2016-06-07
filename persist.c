
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
	if(session_add_cmdq(ses, &ncmd)) return -1;
	r = isi_premake_object(cid, &con, &obj);
	if(r) return r;
	struct isiPLoad * plc = malloc(sizeof(struct isiPLoad));
	if(!plc) {
		isi_delete_object(obj);
		return -1;
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

