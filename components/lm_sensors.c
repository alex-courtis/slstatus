/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sensors/sensors.h>
#include <sensors/error.h>

#include "../util.h"

#define PREFIX_K10_TEMP "k10temp"
#define PREFIX_AMDGPU "amdgpu"

typedef struct {
	double tempInput;
	double powerAverage;
} Amdgpu;

#define LABEL_TDIE "Tdie"
typedef struct {
	double tdie; // only Tdie is correct; Tctl is offset by +27 and exists only for legacy purposes
} K10temp;

#define MAX_AMDGPUS 4
#define MAX_K10_TEMPS 4
typedef struct {
	Amdgpu amdgpus[MAX_AMDGPUS];
	int numAmdgpus;
	K10temp k10temps[MAX_K10_TEMPS];
	int numk10temps;
} Stats;

// discover and collect interesting sensor stats
// pointer to static is returned; do not free
const Stats* collect() {
	static Stats stats;

	const sensors_chip_name *chip_name;
	int chip_nr;
	const sensors_feature *feature;
	int feature_nr;
	const sensors_subfeature *subfeature;
	int subfeature_nr;
	const char *label;
	Amdgpu *amdgpu;
	K10temp *k10temp;

	stats.numAmdgpus = 0;
	stats.numk10temps = 0;

	// init; clean up is done at end
	sensors_init(NULL);

	// iterate chips
	chip_nr = 0;
	while ((chip_name = sensors_get_detected_chips(NULL, &chip_nr))) {
		amdgpu = NULL;
		k10temp = NULL;

		// only interested in known chips
		if (strcmp(chip_name->prefix, PREFIX_AMDGPU) == 0) {
			if (stats.numAmdgpus >= MAX_AMDGPUS) {
				continue;
			}
			amdgpu = &(stats.amdgpus[stats.numAmdgpus++]);
		} else if (strcmp(chip_name->prefix, PREFIX_K10_TEMP) == 0) {
			if (stats.numk10temps >= MAX_K10_TEMPS) {
				continue;
			}
			k10temp = &(stats.k10temps[stats.numk10temps++]);
		} else {
			continue;
		}

		// iterate features
		feature_nr = 0;
		while ((feature = sensors_get_features(chip_name, &feature_nr))) {

			if ((label = sensors_get_label(chip_name, feature)) == NULL)
				continue;

			// iterate readable sub-features
			subfeature_nr = 0;
			while ((subfeature = sensors_get_all_subfeatures(chip_name, feature, &subfeature_nr))) {
				if (!(subfeature->flags & SENSORS_MODE_R)) {
					continue;
				}

				if (amdgpu) {
					switch (subfeature->type) {
						case SENSORS_SUBFEATURE_TEMP_INPUT:
							sensors_get_value(chip_name, subfeature->number, &amdgpu->tempInput);
							break;
						case SENSORS_SUBFEATURE_POWER_AVERAGE:
							sensors_get_value(chip_name, subfeature->number, &amdgpu->powerAverage);
							break;
						default:
							break;
					}
				} else if (k10temp) {
					switch (subfeature->type) {
						case SENSORS_SUBFEATURE_TEMP_INPUT:
							if (strcmp(label, LABEL_TDIE) == 0)
								sensors_get_value(chip_name, subfeature->number, &k10temp->tdie);
							break;
						default:
							break;
					}
				}
			}
		}
	}

	// promises not to error
	sensors_cleanup();

	return &stats;
}

// render average stats as a string with a trailing newline
// static buffer is returned, do not free
const char *render(const Stats *stats) {
	static char buf[128]; // ensure that this is large enough for all the sprintfs with maxints

	if (stats) {
		char *bufPtr = buf;

		if (stats->numAmdgpus > 0) {
			double maxTempInput = 0;
			double maxPowerAverage = 0;
			for (int i = 0; i < stats->numAmdgpus; i++) {
				if (stats->amdgpus[i].tempInput > maxTempInput) {
					maxTempInput = stats->amdgpus[i].tempInput;
				}
				if (stats->amdgpus[i].powerAverage > maxPowerAverage) {
					maxPowerAverage = stats->amdgpus[i].powerAverage;
				}
			}
			bufPtr += sprintf(bufPtr, "amdgpu %i°C %iW", (int)(maxTempInput + 0.5), (int)(maxPowerAverage + 0.5));
		}

		if (stats->numk10temps > 0) {
			double maxTdie = 0;
			for (int i = 0; i < stats->numk10temps; i++) {
				if (stats->k10temps[i].tdie > maxTdie) {
					maxTdie = stats->k10temps[i].tdie;
				}
			}
			sprintf(bufPtr, "%s%s %i°C", bufPtr == buf ? "" : "   ", LABEL_TDIE, (int)(maxTdie + 0.5));
		}
	}

	return buf;
}

const char *
lm_sensors(void)
{
	// collect
	const Stats *stats = collect();

	// render and return
	return render(stats);
}
