
#include "cputypes.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

int isi_scan_dir()
{
	struct dirent *dbuf;
	struct dirent *de;
	DIR *d;
	char *dot;

	d = opendir(".");
	if(!d) { perror("opendir"); return -1; }
	dbuf = (struct dirent*)malloc(offsetof(struct dirent, d_name) + 256);
	if(!dbuf) { perror("malloc"); return -1; }

	while(!readdir_r(d, dbuf, &de) && de) {
		dot = strchr(de->d_name, '.');
		if(dot) {
			dot++;
			if(dot[0] == 'b') {
				fprintf(stderr, "ENT-%s\n", de->d_name);
			}
		}
	}
	closedir(d);
	free(dbuf);
	return 0;
}

