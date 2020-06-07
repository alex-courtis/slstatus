/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>

#include "../util.h"

const char *
load_avg(void)
{
	double avgs[1];

	if (getloadavg(avgs, 1) < 1) {
		warn("getloadavg: Failed to obtain load average");
		return NULL;
	}

	return bprintf("%.2f", avgs[0]);
}
