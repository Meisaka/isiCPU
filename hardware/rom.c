
#include "../dcpuhw.h"

struct EEROM_rvstate {
	uint16_t sz;
};
ISIREFLECT(struct EEROM_rvstate,
	ISIR(EEROM_rvstate, uint16_t, sz)
)

static int EEROM_SIZE(int t, size_t *sz)
{
	switch(t) {
	case 0: return *sz = sizeof(struct EEROM_rvstate), 0;
	default: return 0;
	}
}

static int EEROM_Init(struct isiInfo *info);
static int EEROM_New(struct isiInfo *info, const uint8_t *cfg, size_t lcfg);
static struct isidcpudev EEROM_Meta = {0x0000,0x17400011,MF_ECIV};
static struct isiConstruct EEROM_Con = {
	.objtype = ISIT_HARDWARE,
	.name = "rom",
	.desc = "Embedded ROM",
	.PreInit = 0,
	.Init = EEROM_Init,
	.New = EEROM_New,
	.QuerySize = EEROM_SIZE,
	.rvproto = &ISIREFNAME(struct EEROM_rvstate),
	.svproto = NULL,
	.meta = &EEROM_Meta
};
void EEROM_Register()
{
	isi_register(&EEROM_Con);
}

static int EEROM_Reset(struct isiInfo *info, struct isiInfo *host, struct timespec mtime)
{
	if(!info->mem) return 1;
	if(!info->rvstate) return 1;
	size_t rsize = ((struct EEROM_rvstate *)info->rvstate)->sz << 1;
	if(rsize > info->nvsize) rsize = info->nvsize;
	memcpy( ((isiram16)info->mem)->ram, info->nvstate, rsize);
	return 0;
}

static int EEROM_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	msg[2] = ((struct EEROM_rvstate *)info->rvstate)->sz;
	return 0;
}

static int EEROM_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	size_t rsize = ((struct EEROM_rvstate *)info->rvstate)->sz << 1;
	if(rsize > info->nvsize) rsize = info->nvsize;
	switch(msg[0]) {
	case 0:
		memcpy( ((isiram16)info->mem)->ram, info->nvstate, rsize );
		break;
	case 1:
		memcpy( info->nvstate, ((isiram16)info->mem)->ram, rsize );
		break;
	}
	return 0;
}

static int EEROM_MsgIn(struct isiInfo *info, struct isiInfo *src, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	switch(msg[0]) {
	case ISE_RESET: return EEROM_Reset(info, src, mtime);
	case ISE_QINT: return EEROM_Query(info, src, msg+2, mtime);
	case ISE_XINT: return EEROM_HWI(info, src, msg+2, mtime);
	default: break;
	}
	return 1;
}

static struct isiInfoCalls EEROMCalls = {
	.MsgIn = EEROM_MsgIn
};

static int EEROM_Init(struct isiInfo *info)
{
	info->c = &EEROMCalls;
	struct EEROM_rvstate *rvrom = (struct EEROM_rvstate *)info->rvstate;
	if(info->nvsize) {
		uint32_t rqs = info->nvsize;
		if(rqs > 0x20000) rqs = 0x20000;
		rvrom->sz = rqs >> 1;
	}
	return 0;
}

static int EEROM_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	uint32_t rqs = 0;
	uint64_t mid = 0;
	uint8_t le = 0;
	char * fname = 0;
	struct EEROM_rvstate *rvrom = (struct EEROM_rvstate *)info->rvstate;
	rqs = 0;
	if(!isi_fetch_parameter(cfg, lcfg, 2, &mid, sizeof(uint64_t)) && mid) {
		isi_find_bin(mid, &fname);
	}
	if(isi_fetch_parameter(cfg, lcfg, 1, &rqs, sizeof(uint32_t))) {
		/* pick a default if we don't get the option */
		if(!fname) {
			rqs = 2048;
		} else {
			rqs = isi_fsize(fname);
		}
	}
	if(!isi_fetch_parameter(cfg, lcfg, 3, &le, 1)) {
		le = 1;
	}
	if(rqs > 0x20000) rqs = 0x20000;
	rvrom->sz = rqs >> 1;
	if(fname) {
		if(!info->nvstate) {
			info->nvstate = isi_alloc(info->nvsize = rqs);
		} else {
			if(rqs > info->nvsize) rqs = info->nvsize;
		}
		loadbinfileto(fname, le, (uint8_t*)(info->nvstate), rqs);
		free(fname);
	}
	return 0;
}

