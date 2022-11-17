
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "isitypes.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string>
#include "platform.h"

extern int usefs;

struct disk_rvstate {
	uint32_t size;
	uint8_t wrprotect;
};
struct disk_svstate {
	uint32_t index;
	char block[4096];
	char dblock[4096];
};
ISIREFLECT(struct disk_rvstate,
	ISIR(disk_rvstate, uint32_t, size)
	ISIR(disk_rvstate, uint8_t, wrprotect)
)

ISIREFLECT(struct disk_svstate,
	ISIR(disk_svstate, uint32_t, index)
	ISIR(disk_svstate, char, block)
	ISIR(disk_svstate, char, dblock)
)

int isi_read_disk_file(isiDisk *disk, uint32_t blockseek);
int isi_write_disk_file(isiDisk *disk);

int isi_text_enc(char *text, int limit, void const *vv, int len)
{
	char const * const cenc =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		"ghijklmnopqrstuvwxyz0123456789-_";
	unsigned char const *tid = (unsigned char const *)vv;
	uint32_t ce = 0;
	int i, s;
	s = 0;
	for(i = 0; i < limit; i++) {
		if(!(i & 3)) {
			ce = 0;
			if(s < len) ce |= tid[s++] << 16;
			if(s < len) ce |= tid[s++] << 8;
			if(s < len) ce |= tid[s++];
		}
		text[i] = cenc[((ce>>(18-6*(i&3))) & 0x3f)];
	}
	text[i] = 0;
	return 0;
}
int isi_text_dec(const char *text, size_t len, size_t limit, void *vv, int olen)
{
	int cs;
	int32_t ce;
	size_t i;
	unsigned char *tid = (unsigned char *)vv;
	int s;
	s = 0;
	ce = 0;
	if(len > limit) len = limit;
	for(i = 0; i < len; i++) {
		cs = text[i];
		if(cs >= '0') {
			if(cs <= '9') {
				ce = (ce<<6) | (52+cs-'0');
			} else if(cs >= 'A') {
				if(cs <= 'Z') {
					ce = (ce<<6) | (cs - 'A');
				} else if(cs >= 'a' && cs <= 'z') {
					ce = (ce<<6) | (26+cs-'a');
				} else if(cs == '_') {
					ce = (ce<<6) | 63;
				} else return -1;
			} else {
				return -1;
			}
		} else if(cs == '-') {
			ce = (ce<<6) | 62;
		} else {
			return -1;
		}
		if(i+1 == len) {
			while((i&3) != 3) {
				ce <<= 6; i++;
			}
		}
		if((i & 3) == 3) {
			if(s < olen) tid[s] = ((ce >> 16) & 0xff);
			if(s+1 < olen) tid[s+1] = ((ce >> 8) & 0xff);
			if(s+2 < olen) tid[s+2] = ((ce) & 0xff);
			s += 3;
			ce = 0;
		}
	}
	return 0;
}

int isi_fname_id(const char *fname, uint64_t *id)
{
	const char *dot;
	size_t nlen;
	uint64_t dsk;
	dsk = 0;
	dot = strchr(fname, '.');
	if(dot) {
		nlen = dot - fname;
	} else {
		nlen = strlen(fname);
	}
	if(isi_text_dec(fname, nlen, 11, &dsk, 8)) {
		isilog(L_WARN, "media name to id: Bad name\n");
		return -1;
	}
	if(id) *id = dsk;
	return 0;
}

int isi_scan_dir() {
	struct dirent *de;
	DIR *d;
	char *dot;
	uint64_t dsk;
	char dskname[32];

	d = opendir(".");
	if(!d) { isilogerr("opendir"); return -1; }

	while( (de = readdir(d)) ) {
		dot = strrchr(de->d_name, '.');
		if(dot) {
			if(!strncmp(dot+1, "idi", 3) || !strncmp(dot+1, "bin", 3)) {
				isilog(L_DEBUG, "ENT-%s ", de->d_name);
				dsk = 0;
				if(isi_text_dec(de->d_name, dot - de->d_name, 11, &dsk, 8)) {
					isilog(L_DEBUG, "Bad name ");
				}
				isi_text_enc(dskname, 11, &dsk, 8);
				int f = 13 - (dot - de->d_name);
				for(int i = 0; i < f; i++) putc(' ', stderr);
				isilog(L_DEBUG, "ID:%016lx N:%s\n", dsk, dskname);
			}
		}
	}
	closedir(d);
	return 0;
}

int isi_test_disk(isiDisk *disk)
{
	uint32_t *a, *b;
	struct disk_svstate *ds = (struct disk_svstate*)disk->svstate;
	a = (uint32_t*)ds->block;
	b = (uint32_t*)ds->dblock;
	for(int i = sizeof(ds->block) / sizeof(uint32_t); i--;) {
		if(*a != *b) return 1;
		a++; b++;
	}
	return 0;
}

int isi_find_media(uint64_t diskid, std::string &nameout, const char *ext)
{
	struct dirent *de;
	DIR *d;
	char *dot;
	uint64_t dsk;
	int found = 0;

	d = opendir(".");
	if(!d) { isilogerr("opendir"); return -1; }

	while((de = readdir(d)) && !found) {
		dot = strrchr(de->d_name, '.');
		if(!dot) continue;
		if(strncmp(dot + 1, ext, 3) != 0) continue;
		dsk = 0;
		if(isi_text_dec(de->d_name, dot - de->d_name, 11, &dsk, 8)) {
			continue;
		}
		if(dsk == diskid) {
			found = 1;
			nameout.assign(de->d_name);
		}
	}
	closedir(d);
	return !found;
}

int isi_find_disk(uint64_t diskid, std::string &nameout)
{
	return isi_find_media(diskid, nameout, "idi");
}

int isi_find_bin(uint64_t diskid, char **nameout) {
	std::string strout;
	int r = isi_find_media(diskid, strout, "bin");
	if(!r && nameout) {
		*nameout = (char*)isi_calloc(strout.size() + 1);
		memcpy(*nameout, strout.c_str(), strout.size() + 1);
	}
	return r;
}

static int isi_read_disk(isiInfo *info, uint32_t blkseek)
{
	isiDisk *disk = (isiDisk *)info;
	if(info->uuid) {
		persist_disk(info, blkseek, 0, 1);
	} else if(disk->fd != fdesc_empty) {
		return isi_read_disk_file(disk, blkseek);
	}
	return -1;
}

static int isi_writeread_disk(isiInfo *info, uint32_t blkseek)
{
	isiDisk *disk = (isiDisk *)info;
	if(info->uuid) {
		struct disk_svstate *sv = (struct disk_svstate *)info->svstate;
		persist_disk(info, blkseek, sv->index, 3);
	} else if(disk->fd != fdesc_empty) {
		isi_write_disk_file(disk);
		return isi_read_disk_file(disk, blkseek);
	}
	return -1;
}

static int isi_write_disk(isiInfo *info)
{
	isiDisk *disk = (isiDisk *)info;
	if(info->uuid) {
		struct disk_svstate *sv = (struct disk_svstate *)info->svstate;
		persist_disk(info, 0, sv->index, 2);
	} else if(disk->fd != fdesc_empty) {
		return isi_write_disk_file(disk);
	}
	return -1;
}

int isiDisk::MsgIn(isiInfo *src, int32_t lsindex, uint32_t *msg, int len, isi_time_t mtime)
{
	struct disk_rvstate *rv = (struct disk_rvstate *)this->rvstate;
	struct disk_svstate *sv = (struct disk_svstate *)this->svstate;
	struct isiDiskSeekMsg *dsm = (struct isiDiskSeekMsg *)msg;
	switch(msg[0]) {
	case ISE_DISKSEEK:
		if(dsm->block != sv->index) {
			if(!rv->wrprotect && isi_test_disk(this)) {
				isi_writeread_disk(this, dsm->block);
			} else {
				isi_read_disk(this, dsm->block);
			}
		}
		return 0;
	case ISE_DISKWPRST:
		if(!rv->wrprotect && isi_test_disk(this)) isi_write_disk(this);
		break;
	case ISE_DISKRESET:
		sv->index = (msg[2] << 16u) | msg[1];
		memcpy(sv->dblock, sv->block, sizeof(sv->block));
		break;
	default:
		break;
	}
	return -1;
}

int isiDisk::Unload()
{
	struct disk_rvstate *rv = (struct disk_rvstate *)this->rvstate;
	if(!rv->wrprotect && isi_test_disk(this)) isi_write_disk(this);
	close(this->fd);
	return 0;
}

int isi_disk_isreadonly(isiInfo *disk)
{
	if(!disk) return -1;
	if(disk->otype != ISIT_DISK) {
		return -1;
	}
	struct disk_rvstate *rv = (struct disk_rvstate *)disk->rvstate;
	return (rv->wrprotect != 0);
}

int isi_disk_getblock(isiInfo *disk, void **blockaddr)
{
	if(!disk || !blockaddr) return -1;
	if(disk->otype != ISIT_DISK) {
		*blockaddr = 0;
		return -1;
	}
	struct disk_svstate *sv = (struct disk_svstate *)disk->svstate;
	*blockaddr = sv->block;
	return 0;
}

int isi_disk_getindex(isiInfo *disk, uint32_t *blockindex)
{
	if(!disk || !blockindex) return -1;
	if(disk->otype != ISIT_DISK) {
		*blockindex = 0;
		return -1;
	}
	struct disk_svstate *sv = (struct disk_svstate *)disk->svstate;
	*blockindex = sv->index;
	return 0;
}

isiDisk::isiDisk()
{
	this->fd = fdesc_empty;
}
int isiDisk::Load()
{
	if(this->uuid) isi_read_disk(this, 0);
	return 0;
}

int isiDisk::Init(const uint8_t *cfg, size_t lcfg)
{
	char dskname[32];
	std::string ldisk;
	int oflags = 0;
	fdesc_t fd = fdesc_empty;
	uint64_t diskid = 0;
	if(isi_fetch_parameter(cfg, lcfg, 1, &diskid, sizeof(uint64_t))) {
	}
	if(usefs) {
		if(diskid) {
			isi_text_enc(dskname, 11, &diskid, 8);
			if(isi_find_disk(diskid, ldisk)) {
				ldisk.assign(dskname);
				ldisk.append(".idi");
				oflags = O_CREAT;
			}
			isilog(L_DEBUG, "Openning Disk\n");
			fd = open(ldisk.c_str(), O_RDWR | oflags, 0644);
			if(fd == fdesc_empty) {
				isilogerr("open");
				return ISIERR_FILE;
			}
		}
	} else if(diskid) {
		return ISIERR_NOTALLOWED;
	}
	this->fd = fd;
	if(usefs) isi_read_disk(this, 0);
	return 0;
}
static char Disk_Meta[] = {0,0,0,0,0,0,0,0,0,0,0,0};
static isiClass<isiDisk> Disk_Con(ISIT_DISK, "disk", "Disk media", 
		&ISIREFNAME(struct disk_rvstate),
		&ISIREFNAME(struct disk_svstate),
		NULL, &Disk_Meta);
void Disk_Register()
{
	isi_register(&Disk_Con);
}

int isi_read_disk_file(isiDisk *disk, uint32_t blockseek)
{
	int i;
	size_t f = 0;
	isiInfo *idisk = (isiInfo *)disk;
	struct disk_svstate *ds = (struct disk_svstate*)idisk->svstate;
	if(!ds) return -1;
	memset(ds->block, 0, sizeof(ds->block));
	lseek(disk->fd, sizeof(ds->block) * blockseek, SEEK_SET);
	if(((i = read(disk->fd, ds->block, 4096)) > 0)) {
		f += i;
	}
	if( i < 0 ) {
		isilogerr("read");
		return ISIERR_FILE;
	}
	ds->index = blockseek;
	memcpy(ds->dblock, ds->block, sizeof(ds->block));
	return 0;
}

int isi_write_disk_file(isiDisk *disk)
{
	int i;
	size_t f = 0;
	isiInfo *idisk = (isiInfo *)disk;
	struct disk_svstate *ds = (struct disk_svstate*)idisk->svstate;
	lseek(disk->fd, sizeof(ds->block) * ds->index, SEEK_SET);
	if(((i = write(disk->fd, ds->block, 4096)) > 0)) {
		f += i;
	}
	if( i < 0 ) {
		isilogerr("write");
		return ISIERR_FILE;
	}
	return 0;
}

size_t isi_fsize(const char *path)
{
	struct stat fdi;
	if(stat(path, &fdi)) { isilogerr("stat"); return 0; }
	return fdi.st_size;
}

int loadbinfileto(const char* path, int endian, unsigned char *nmem, uint32_t nsize)
{
	fdesc_t fd;
	int i;
	struct stat fdi;
	size_t f, o;
	uint16_t eswp;
	if(!nmem) { return -5; }
	fd = open(path, O_RDONLY);
	if(fd == fdesc_empty) { isilogerr("open"); return -5; }
	if(fstat(fd, &fdi)) { isilogerr("fstat"); close(fd); return -3; }
	f = fdi.st_size & (~1);
	if(f > nsize) f = nsize;
	o = 0;
	while(((i = read(fd, (char*)(nmem+o), f - o)) > 0) && o < f) {
		o += i;
	}
	if( i < 0 ) {
		isilogerr("read");
		close(fd);
		return -1;
	}
	close(fd);
	o = 0;
	if(endian) {
		while(o < f) {
			eswp = *(uint16_t*)(nmem+o);
			*(uint16_t*)(nmem+o) = (eswp >> 8) | (eswp << 8);
			o += 2;
		}
	}
	isilog(L_DEBUG, "loaded %lu bytes\n", f);
	return 0;
}

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize)
{
	fdesc_t fd;
	int i;
	struct stat fdi;
	size_t f, o;
	uint16_t eswp;
	uint8_t *mem;
	fd = open(path, O_RDONLY);
	if(fd == fdesc_empty) { isilogerr("open"); return -5; }
	if(fstat(fd, &fdi)) { isilogerr("fstat"); close(fd); return -3; }
	f = fdi.st_size & (~1);
	mem = (uint8_t*)isi_calloc(f);
	if(!mem) { close(fd); return -5; }
	o = 0;
	while(((i = read(fd, (char*)(mem+o), f - o)) > 0) && o < f) {
		o += i;
	}
	if( i < 0 ) {
		isilogerr("read");
		close(fd);
		return -1;
	}
	close(fd);

	o = 0;
	if(endian) {
		while(o < f) {
			eswp = *(uint16_t*)(mem+o);
			*(uint16_t*)(mem+o) = (eswp >> 8) | (eswp << 8);
			o += 2;
		}
	}
	*nmem = mem;
	isilog(L_DEBUG, "loaded %lu bytes\n", f);
	if(nsize) *nsize = f;
	return 0;
}

int savebinfile(const char* path, int endian, unsigned char *nmem, uint32_t nsize)
{
	fdesc_t fd;
	int i;
	struct stat fdi;
	size_t o;
	uint16_t eswp;
	uint8_t *mem;
	fd = open(path, O_RDWR);
	if(fd == fdesc_empty) { isilogerr("open"); return -5; }
	if(fstat(fd, &fdi)) { isilogerr("fstat"); close(fd); return -3; }
	if(endian) {
		mem = (uint8_t*)isi_calloc(nsize);
		if(!mem) { close(fd); return -5; }
		o = 0;
		while(o < nsize) {
			eswp = *(uint16_t*)(mem+o);
			*(uint16_t*)(mem+o) = (eswp >> 8) | (eswp << 8);
			o += 2;
		}
	} else {
		mem = nmem;
	}
	o = 0;
	i = 0;
	while(o < nsize && ((i = write(fd, (const char*)(nmem+o), nsize-o)) > 0)) {
		o += i;
	}
	if(endian) free(mem);
	if( i < 0 ) {
		isilogerr("write");
		close(fd);
		return -1;
	}
	close(fd);
	isilog(L_DEBUG, "wrote %lu bytes\n", o);
	return 0;
}

