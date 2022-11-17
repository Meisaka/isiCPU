
#include "../dcpuhw.h"

#define ERR_M35_NONE 0
#define ERR_M35_BUSY 1
#define ERR_M35_NO_MEDIA 2
#define ERR_M35_PROTECT 3
#define ERR_M35_EJECT 4
#define ERR_M35_SECTOR 5
#define ERR_M35_BROKEN 0xffff

#define ST_M35_NO_MEDIA 0
#define ST_M35_IDLE 1
#define ST_M35_IDLEP 2
#define ST_M35_BUSY 3
#define ST_M35_BUSYREAD 3
#define ST_M35_BUSYWRITE 4

/* state while this device exists/active (runtime volatile) */
struct Disk_M35FD_rvstate {
	uint16_t track;
	uint16_t sector;
	uint16_t seektrack;
	uint16_t seeksector;
	uint16_t rwaddr;
	uint16_t iword;
	uint16_t oper;
	uint16_t errcode;
};
ISIREFLECT(struct Disk_M35FD_rvstate,
	ISIR(Disk_M35FD_rvstate, uint16_t, track)
	ISIR(Disk_M35FD_rvstate, uint16_t, sector)
	ISIR(Disk_M35FD_rvstate, uint16_t, seektrack)
	ISIR(Disk_M35FD_rvstate, uint16_t, seeksector)
	ISIR(Disk_M35FD_rvstate, uint16_t, rwaddr)
	ISIR(Disk_M35FD_rvstate, uint16_t, iword)
	ISIR(Disk_M35FD_rvstate, uint16_t, oper)
	ISIR(Disk_M35FD_rvstate, uint16_t, errcode)
)

/* state while running on this server (server volatile) */
struct Disk_M35FD_svstate {
	uint16_t svbuf[512];
};
ISIREFLECT(struct Disk_M35FD_svstate,
	ISIR(Disk_M35FD_svstate, uint16_t, svbuf)
)

class Disk_M35FD : public isiInfo {
public:
	//virtual int Init(const uint8_t *, size_t);
	//virtual int Load();
	//virtual int Unload();
	virtual int Run(isi_time_t crun);
	virtual int MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime);
	virtual int QueryAttach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	//virtual int Attach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	//virtual int Deattach(int32_t topoint, isiInfo *dev, int32_t frompoint);
	virtual int Reset();
protected:
	void statechange(isi_time_t mtime);
	int HWI(isiInfo *src, uint32_t *msg, int len, isi_time_t crun);
};

static struct isidcpudev Disk_M35FD_Meta = {0x000b,0x4fd524c5,MF_MACK};
static isiClass<Disk_M35FD> Disk_M35FD_Con(
	ISIT_HARDWARE, "txc_mack_35fd", "Mackapar M35FD",
	&ISIREFNAME(struct Disk_M35FD_rvstate),
	&ISIREFNAME(struct Disk_M35FD_svstate),
	NULL,
	&Disk_M35FD_Meta);

void Disk_M35FD_Register()
{
	isi_register(&Disk_M35FD_Con);
}

int Disk_M35FD::QueryAttach(int32_t point, isiInfo *dev, int32_t devpoint)
{
	if(!dev || (point != ISIAT_UP && dev->otype != ISIT_DISK)) return -1;
	return 0;
}

int Disk_M35FD::Reset()
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)this->rvstate;
	dev->errcode = 0;
	dev->iword = 0;
	dev->oper = 0;
	dev->seektrack = 0;
	dev->seeksector = 0;
	dev->track = 400;
	dev->sector = 0;
	return 0;
}

void Disk_M35FD::statechange(isi_time_t mtime)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)this->rvstate;
	uint32_t iom[3];
	if(dev->iword) {
		iom[0] = 2;
		iom[1] = 0;
		iom[2] = dev->iword;
		isi_message_dev(this, ISIAT_UP, iom, 3, mtime);
	}
}

int Disk_M35FD::HWI(isiInfo *src, uint32_t *msg, int len, isi_time_t crun)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)this->rvstate;
	struct Disk_M35FD_svstate *dss = (struct Disk_M35FD_svstate*)this->svstate;
	uint16_t lerr;
	uint16_t mc;
	int i;
	int r = 0;
	isiInfo *media = 0;
	this->get_dev(0, &media);
	switch(msg[0]) {
	case 0:
		if(media) {
			if(dev->oper >= ST_M35_BUSY) {
				msg[1] = ST_M35_BUSY;
			} else {
				msg[1] = dev->oper;
			}
		} else {
			msg[1] = 0;
		}
		msg[2] = dev->errcode;
		if(dev->errcode) this->statechange(crun);
		dev->errcode = ERR_M35_NONE;
		break;
	case 1:
		dev->iword = msg[3];
		break;
	case 2:
		msg[1] = 0;
		lerr = dev->errcode;
		if(dev->oper >= ST_M35_BUSY) dev->errcode = ERR_M35_BUSY;
		else if(!media) dev->errcode = ERR_M35_NO_MEDIA;
		else if(msg[3] >= 1440) dev->errcode = ERR_M35_SECTOR;
		else {
			dev->seeksector = msg[3];
			dev->seektrack = dev->seeksector / 18;
			struct isiDiskSeekMsg dseek;
			dseek.mcode = 0x0020;
			dseek.block = dev->seeksector >> 2;
			isi_message_dev(this, 0, (uint32_t*)&dseek, 3, crun);
			dev->rwaddr = msg[4];
			dev->oper = ST_M35_BUSYREAD;
			msg[1] = 1;
			this->statechange(crun);
		}
		if(lerr != dev->errcode) this->statechange(crun);
		break;
	case 3:
		msg[1] = 0;
		lerr = dev->errcode;
		if(dev->oper >= ST_M35_BUSY) dev->errcode = ERR_M35_BUSY;
		else if(!media) dev->errcode = ERR_M35_NO_MEDIA;
		else if(msg[3] >= 1440) dev->errcode = ERR_M35_SECTOR;
		else if(isi_disk_isreadonly(media)) dev->errcode = ERR_M35_PROTECT; /* TODO write_protect */
		else {
			dev->seeksector = msg[3];
			dev->seektrack = dev->seeksector / 18;
			struct isiDiskSeekMsg dseek;
			dseek.mcode = 0x0020;
			dseek.block = dev->seeksector >> 2;
			isi_message_dev(this, 0, (uint32_t*)&dseek, 3, crun);
			mc = dev->rwaddr = msg[4];
			for(i = 0; i < 512; i++) {
				dss->svbuf[i] = (uint16_t)this->mem->d_rd(mc);
				mc++;
			}
			dev->oper = ST_M35_BUSYWRITE;
			r = 512;
			msg[1] = 1;
			this->statechange(crun);
		}
		if(lerr != dev->errcode) this->statechange(crun);
		break;
	}
	return r;
}

int Disk_M35FD::Run(isi_time_t crun)
{
	struct Disk_M35FD_rvstate *dev = (struct Disk_M35FD_rvstate*)this->rvstate;
	struct Disk_M35FD_svstate *dss = (struct Disk_M35FD_svstate*)this->svstate;
	isiInfo *media = 0;
	if(!this->nrun) {
		this->nrun = crun;
	}
	while(isi_time_lt(&this->nrun, &crun)) {
		if(dev->oper >= ST_M35_BUSY) {
			if(this->get_dev(0, &media)) {
				dev->errcode = ERR_M35_EJECT;
				dev->oper = ST_M35_NO_MEDIA;
				this->statechange(this->nrun);
				continue;
			}
			if(dev->seektrack != dev->track) {
				isi_add_time(&this->nrun, 2400000);
				dev->track += (dev->seektrack < dev->track) ?-1:1;
				dev->sector += 73;
				dev->sector %= 18;
			} else {
				dev->sector++;
				dev->sector %= 18;
				isi_add_time(&this->nrun, 32573);
				uint32_t seekblock;
				isi_disk_getindex(media, &seekblock);
				if((dev->seeksector + 1) % 18 == dev->sector && seekblock == (uint32_t)(dev->seeksector >> 2)) {
					/* read success */
					uint16_t *dr;
					isi_disk_getblock(media, (void**)&dr);
					dr += (512 * (dev->seeksector & 3));
					if(dev->oper == ST_M35_BUSYREAD) {
						uint16_t mc;
						mc = dev->rwaddr;
						for(int i = 0; i < 512; i++) {
							this->mem->d_wr(mc, dr[i]);
							mc++;
						}
					} else if(dev->oper == ST_M35_BUSYWRITE) {
						for(int i = 0; i < 512; i++) {
							dr[i] = dss->svbuf[i];
						}
					}
					dev->oper = isi_disk_isreadonly(media)? ST_M35_IDLEP:ST_M35_IDLE;
					this->statechange(this->nrun);
				}
			}
		} else {
			if(this->get_dev(0, &media)) {
				if(dev->oper != ST_M35_NO_MEDIA) {
					dev->oper = ST_M35_NO_MEDIA;
					this->statechange(this->nrun);
				}
			} else {
				if(dev->oper == ST_M35_NO_MEDIA) {
					dev->oper = isi_disk_isreadonly(media)? ST_M35_IDLEP:ST_M35_IDLE;
					this->statechange(this->nrun);
				}
			}
			isi_add_time(&this->nrun, 50000000);
			dev->sector += 1535;
			dev->sector %= 18;
		}
	}
	return 0;
}

int Disk_M35FD::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case 0: return 0; /* CPU finished reset */
	case 1: return 0; /* HWQ executed */
	case 2:
		if(len < 10) return -1;
		return this->HWI(src, msg+2, len - 2, mtime); /* HWI executed */
	default: break;
	}
	return 1;
}

