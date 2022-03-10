/* See LICENSE file for copyright and license details. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../util.h"

#define DOWN ""
#define VPN_UP "VPN   "

const char *
vpn_state(const char *interface)
{
	char *p;
	char path[PATH_MAX];
	FILE *fp;
	char status[5];

	if (esnprintf(path, sizeof(path), "/sys/class/net/%s/operstate", interface) < 0) {
		return DOWN;
	}
	if (!(fp = fopen(path, "r"))) {
		warn("fopen '%s':", path);
		return DOWN;
	}
	p = fgets(status, 5, fp);
	fclose(fp);
	if (!p || strcmp(status, "up\n") != 0) {
		return DOWN;
	}

	return VPN_UP;
}

