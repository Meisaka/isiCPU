
#include "cputypes.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int isi_read_disk_file(struct isiDisk *disk);
int isi_write_disk_file(struct isiDisk *disk);

int isi_text_enc(char *text, int limit, void const *vv, int len)
{
	char const * const cenc =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		"ghijklmnopqrstuvwxyz0123456789-_";
	unsigned char const *tid = (unsigned char const *)vv;
	uint32_t ce;
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
int isi_text_dec(const char *text, int len, int limit, void *vv, int olen)
{
	int cs;
	int32_t ce;
	int i;
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
			} else if(cs == '-') {
				ce = (ce<<6) | 62;
			} else {
				return -1;
			}
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

int isi_scan_dir()
{
	struct dirent *dbuf;
	struct dirent *de;
	DIR *d;
	char *dot;
	uint64_t dsk;
	char dskname[32];

	d = opendir(".");
	if(!d) { perror("opendir"); return -1; }
	dbuf = (struct dirent*)malloc(offsetof(struct dirent, d_name) + 256);
	if(!dbuf) { perror("malloc"); closedir(d); return -1; }

	while(!readdir_r(d, dbuf, &de) && de) {
		dot = strchr(de->d_name, '.');
		if(dot) {
			if(!strncmp(dot+1, "idi", 3)) {
				fprintf(stderr, "ENT-%s ", de->d_name);
				dsk = 0;
				if(isi_text_dec(de->d_name, dot - de->d_name, 11, &dsk, 8)) {
					fprintf(stderr, "Bad name ");
				}
				isi_text_enc(dskname, 11, &dsk, 8);
				int f = 13 - (dot - de->d_name);
				for(int i = 0; i < f; i++) putc(' ', stderr);
				fprintf(stderr, "ID:%016lx N:%s\n", dsk, dskname);
			}
		}
	}
	closedir(d);
	free(dbuf);
	return 0;
}

int isi_test_disk(struct isiDisk *disk)
{
	uint32_t *a, *b;
	struct isiInfo *idisk = (struct isiInfo *)disk;
	struct disk_svstate *ds = (struct disk_svstate*)idisk->svstate;
	a = (uint32_t*)ds->block;
	b = (uint32_t*)ds->dblock;
	for(int i = sizeof(ds->block) / sizeof(uint32_t); i--;) {
		if(*a != *b) return 1;
		a++; b++;
	}
	return 0;
}

int isi_find_disk(uint64_t diskid, char **nameout)
{
	struct dirent *dbuf;
	struct dirent *de;
	DIR *d;
	char *dot;
	uint64_t dsk;
	int found = 0;

	d = opendir(".");
	if(!d) { perror("opendir"); return -1; }
	dbuf = (struct dirent*)malloc(offsetof(struct dirent, d_name) + 256);
	if(!dbuf) { perror("malloc"); closedir(d); return -1; }

	while(!readdir_r(d, dbuf, &de) && de && !found) {
		dot = strchr(de->d_name, '.');
		if(dot) {
			if(!strncmp(dot+1, "idi", 3)) {
				dsk = 0;
				if(isi_text_dec(de->d_name, dot - de->d_name, 11, &dsk, 8)) {
					continue;
				}
				if(dsk == diskid) {
					found = 1;
					if(nameout) {
						*nameout = strdup(de->d_name);
					}
				}
			}
		}
	}
	closedir(d);
	free(dbuf);
	return !found;
}

static int isi_disk_msgin(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	if(info->id.objtype != ISIT_DISK) return -1;
	struct isiDisk *disk = (struct isiDisk *)info;
	struct isiDiskSeekMsg *dsm = (struct isiDiskSeekMsg *)msg;
	switch(msg[0]) {
	case 0x0020:
		if(dsm->block != disk->block) {
			if(isi_test_disk(disk)) isi_write_disk_file(disk);
			disk->block = dsm->block;
			isi_read_disk_file(disk);
		}
		return 0;
	default:
		break;
	}
	return -1;
}

static int isi_delete_disk(struct isiInfo *info)
{
	if(info->id.objtype != ISIT_DISK) return -1;
	struct isiDisk *disk = (struct isiDisk*)info;
	if(isi_test_disk(disk)) isi_write_disk_file(disk);
	close(disk->fd);
	return 0;
}

int isi_disk_getblock(struct isiInfo *disk, void **blockaddr)
{
	if(!disk || !blockaddr) return -1;
	if(disk->id.objtype != ISIT_DISK) {
		*blockaddr = 0;
		return -1;
	}
	*blockaddr = disk->svstate;
	return 0;
}

int isi_create_disk(uint64_t diskid, struct isiInfo **ndisk)
{
	char dskname[32];
	char *ldisk;
	int oflags = 0;
	int fd;
	if(!ndisk || !diskid) return -1;
	isi_text_enc(dskname, 11, &diskid, 8);
	if(isi_find_disk(diskid, &ldisk)) {
		asprintf(&ldisk, "%s.idi", dskname);
		oflags = O_CREAT;
	}
	if(!strcmp(dskname, ldisk)) {
	}
	fprintf(stderr, "Openning Disk\n");
	fd = open(ldisk, O_RDWR | oflags);
	if(fd == -1) {
		perror("open");
		free(ldisk);
		return -1;
	}
	struct isiDisk *mdisk;
	struct isiInfo *idisk;
	isi_create_object(ISIT_DISK, (struct objtype**)&idisk);
	mdisk = (struct isiDisk *)idisk;
	mdisk->fd = fd;
	mdisk->diskid = diskid;
	mdisk->block = 0;
	idisk->Delete = isi_delete_disk;
	idisk->MsgIn = isi_disk_msgin;
	idisk->svstate = malloc(sizeof(struct disk_svstate));
	isi_read_disk_file(mdisk);
	*ndisk = idisk;
	free(ldisk);
	return 0;
}

int isi_read_disk_file(struct isiDisk *disk)
{
	int i;
	size_t f = 0;
	struct isiInfo *idisk = (struct isiInfo *)disk;
	struct disk_svstate *ds = (struct disk_svstate*)idisk->svstate;
	if(!ds) return -1;
	memset(ds->block, 0, sizeof(ds->block));
	lseek(disk->fd, sizeof(ds->block) * disk->block, SEEK_SET);
	if(((i = read(disk->fd, ds->block, 4096)) > 0)) {
		f += i;
	}
	if( i < 0 ) {
		perror("read");
		return -1;
	}
	memcpy(ds->dblock, ds->block, sizeof(ds->block));
	return 0;
}

int isi_write_disk_file(struct isiDisk *disk)
{
	int i;
	size_t f = 0;
	struct isiInfo *idisk = (struct isiInfo *)disk;
	struct disk_svstate *ds = (struct disk_svstate*)idisk->svstate;
	lseek(disk->fd, sizeof(ds->block) * disk->block, SEEK_SET);
	if(((i = write(disk->fd, ds->block, 4096)) > 0)) {
		f += i;
	}
	if( i < 0 ) {
		perror("write");
		return -1;
	}
	return 0;
}

int loadbinfile(const char* path, int endian, unsigned char **nmem, uint32_t *nsize)
{
	int fd, i;
	struct stat fdi;
	size_t f, o;
	uint16_t eswp;
	uint8_t *mem;
	fd = open(path, O_RDONLY);
	if(fd < 0) { perror("open"); return -5; }
	if(fstat(fd, &fdi)) { perror("fstat"); close(fd); return -3; }
	f = fdi.st_size & (~1);
	mem = (uint8_t*)malloc(f);
	if(!mem) { close(fd); return -5; }
	o = 0;
	while(((i = read(fd, mem+o, f - o)) > 0) && o < f) {
		o += i;
	}
	if( i < 0 ) {
		perror("read");
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
	fprintf(stderr, "loaded %lu bytes\n", f);
	if(nsize) *nsize = f;
	return 0;
}

int savebinfile(const char* path, int endian, unsigned char *nmem, uint32_t nsize)
{
	int fd, i;
	struct stat fdi;
	size_t o;
	uint16_t eswp;
	uint8_t *mem;
	fd = open(path, O_RDWR);
	if(fd < 0) { perror("open"); return -5; }
	if(fstat(fd, &fdi)) { perror("fstat"); close(fd); return -3; }
	if(endian) {
		mem = (uint8_t*)malloc(nsize);
		if(!mem) { close(fd); return -5; }
		while(o < nsize) {
			eswp = *(uint16_t*)(mem+o);
			*(uint16_t*)(mem+o) = (eswp >> 8) | (eswp << 8);
			o += 2;
		}
	} else {
		mem = nmem;
	}
	o = 0;
	while(o < nsize && ((i = write(fd, nmem+o, nsize-o)) > 0)) {
		o += i;
	}
	if(endian) free(mem);
	if( i < 0 ) {
		perror("write");
		close(fd);
		return -1;
	}
	close(fd);
	fprintf(stderr, "wrote %lu bytes\n", o);
	return 0;
}

