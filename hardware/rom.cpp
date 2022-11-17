
#include "../dcpuhw.h"

struct EEROM_rvstate {
	uint16_t sz;
};
ISIREFLECT(struct EEROM_rvstate,
	ISIR(EEROM_rvstate, uint16_t, sz)
)

class EEROM : public isiInfo {
public:
	virtual int Init(const uint8_t *, size_t);
	virtual size_t get_rv_size() const;
	virtual int Load();
	//virtual int Unload();
	//virtual int Run(isi_time_t crun);
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	//virtual int QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	//virtual int Attach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	//virtual int Deattach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Reset();

	int HWQ(isiInfo *src, uint32_t *msg, isi_time_t crun);
	int HWI(isiInfo *src, uint32_t *msg, isi_time_t crun);
};
static struct isidcpudev EEROM_Meta = {0x0000,0x17400011,MF_ECIV};
static isiClass<EEROM> EEROM_Con(
	ISIT_HARDWARE, "trk_gen_eprom", "Embedded ROM",
	&ISIREFNAME(struct EEROM_rvstate),
	NULL,
	NULL,
	&EEROM_Meta);
void EEROM_Register() {
	isi_register(&EEROM_Con);
}

size_t EEROM::get_rv_size() const {
	return sizeof(struct EEROM_rvstate);
}

int EEROM::Reset()
{
	if(!this->mem) return 1;
	if(!this->rvstate) return 1;
	size_t rsize = ((struct EEROM_rvstate *)this->rvstate)->sz << 1;
	if(rsize > this->nvsize) rsize = this->nvsize;
	uint16_t *src = (uint16_t*)this->nvstate;
	for(uint32_t load_addr = 0; rsize; rsize--, load_addr++, src++) {
		this->mem->d_wr(load_addr, *src);
	}
	return 0;
}

int EEROM::HWQ(isiInfo *src, uint32_t *msg, isi_time_t crun)
{
	msg[2] = ((struct EEROM_rvstate *)this->rvstate)->sz;
	return 0;
}

int EEROM::HWI(isiInfo *src, uint32_t *msg, isi_time_t crun)
{
	size_t rsize = ((struct EEROM_rvstate *)this->rvstate)->sz << 1;
	if(rsize > this->nvsize) rsize = this->nvsize;
	uint16_t *nvptr = (uint16_t*)this->nvstate;
	switch(msg[0]) {
	case 0:
		for(uint32_t load_addr = 0; rsize; rsize--, load_addr++, nvptr++) {
			this->mem->d_wr(load_addr, *nvptr);
		}
		break;
	case 1:
		for(uint32_t load_addr = 0; rsize; rsize--, load_addr++, nvptr++) {
			*nvptr = this->mem->d_rd(load_addr);
		}
		break;
	}
	return 0;
}

int EEROM::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	switch(msg[0]) {
	case ISE_RESET: return this->Reset();
	case ISE_QINT: return this->HWQ(src, msg+2, mtime);
	case ISE_XINT: return this->HWI(src, msg+2, mtime);
	default: break;
	}
	return 1;
}

int EEROM::Load()
{
	struct EEROM_rvstate *rvrom = (struct EEROM_rvstate *)this->rvstate;
	if(this->nvsize) {
		uint32_t rqs = this->nvsize;
		if(rqs > 0x20000) rqs = 0x20000;
		rvrom->sz = rqs >> 1;
	}
	return 0;
}

int EEROM::Init(const uint8_t * cfg, size_t lcfg)
{
	uint32_t rqs = 0;
	uint64_t mid = 0;
	uint8_t le = 0;
	char * fname = 0;
	struct EEROM_rvstate *rvrom = (struct EEROM_rvstate *)this->rvstate;
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
		if(!this->nvstate) {
			this->nvstate = isi_calloc(this->nvsize = rqs);
		} else {
			if(rqs > this->nvsize) rqs = this->nvsize;
		}
		loadbinfileto(fname, le, (uint8_t*)(this->nvstate), rqs);
		free(fname);
	}
	return 0;
}

