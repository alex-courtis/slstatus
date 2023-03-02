/* See LICENSE file for copyright and license details. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/statvfs.h>

#include "../util.h"

const char *
disk_free(const char *path)
{
	struct statvfs fs;

	if (statvfs(path, &fs) < 0) {
		warn("statvfs '%s':", path);
		return NULL;
	}

	return fmt_human(fs.f_frsize * fs.f_bavail, 1024);
}

const char *
disk_perc(const char *path)
{
	struct statvfs fs;

	if (statvfs(path, &fs) < 0) {
		warn("statvfs '%s':", path);
		return NULL;
	}

	return bprintf("%d", (int)(100 *
	               (1.0f - ((float)fs.f_bavail / (float)fs.f_blocks))));
}

const char *
tmp_perc_gt(const char *perc)
{
	static const int n = 2;
	static const char paths[2][PATH_MAX] = {
		"/tmp",
		"/run",
	};
	static char buf[256];

	struct statvfs fs;
	char *pbuf = buf;

	int minimum = atoi(perc);

	for (int i = 0; i < n; i++) {
		if (statvfs(paths[i], &fs) < 0) {
			warn("statvfs '%s':", paths[i]);
			return NULL;
		}
		int actual = (int)(100 * (1.0f - ((float)fs.f_bavail / (float)fs.f_blocks)));

		if (actual >= minimum)
			pbuf += sprintf(pbuf, "%s %d%%   ", paths[i], actual);
	}

	return buf;
}

const char *
disk_total(const char *path)
{
	struct statvfs fs;

	if (statvfs(path, &fs) < 0) {
		warn("statvfs '%s':", path);
		return NULL;
	}

	return fmt_human(fs.f_frsize * fs.f_blocks, 1024);
}

const char *
disk_used(const char *path)
{
	struct statvfs fs;

	if (statvfs(path, &fs) < 0) {
		warn("statvfs '%s':", path);
		return NULL;
	}

	return fmt_human(fs.f_frsize * (fs.f_blocks - fs.f_bfree), 1024);
}
