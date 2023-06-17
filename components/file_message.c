/* See LICENSE file for copyright and license details. */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include "../util.h"

const char *
file_message(const char *path)
{
	static FILE *fp;
	static char b[128];

	buf[0] = '\0';

	if ((fp = fopen(path, "r"))) {

		if (fgets(b, sizeof(b), fp)) {
			*(strchrnul(b, '\n')) = '\0';
			bprintf("%s ", b);
		}

		fclose(fp);
	}

	return buf;
}

