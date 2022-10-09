/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sensors/sensors.h>

#include "../util.h"

#define MAX(A, B) ((A) > (B) ? (A) : (B))

enum Chip { k10Temp, amdgpu, thinkpad, coretemp, dell };

typedef struct {
	int amdgpuTempMax;
	int amdgpuPowerTotal;
	int k10tempTdieMax;
	int thinkpadFanMax;
	int dellFanMax;
	int coreTempMax;
} Sts;

/* discover and collect interesting sensor stats */
Sts collect() {

	const sensors_chip_name *chip_name;
	int chip_nr;
	const sensors_feature *feature;
	int feature_nr;
	const sensors_subfeature *subfeature;
	int subfeature_nr;
	char *label = NULL;
	double value;
	enum Chip chip;
	Sts sts = {
			.amdgpuPowerTotal = 0,
			.amdgpuTempMax = 0,
			.k10tempTdieMax = 0,
			.thinkpadFanMax = 0,
			.dellFanMax = 0,
			.coreTempMax = 0,
	};

	/* init; clean up is done at end */
	sensors_init(NULL);

	/* iterate chips */
	chip_nr = 0;
	while ((chip_name = sensors_get_detected_chips(NULL, &chip_nr))) {

		/* only interested in known chips */
		if (strcmp(chip_name->prefix, "amdgpu") == 0)
			chip = amdgpu;
		else if (strcmp(chip_name->prefix, "k10temp") == 0)
			chip = k10Temp;
		else if (strcmp(chip_name->prefix, "thinkpad") == 0)
			chip = thinkpad;
		else if (strcmp(chip_name->prefix, "coretemp") == 0)
			chip = coretemp;
		else if (strcmp(chip_name->prefix, "dell_smm") == 0)
			chip = dell;
		else
			continue;

		/* iterate features */
		feature_nr = 0;
		while ((feature = sensors_get_features(chip_name, &feature_nr))) {
			if ((label = sensors_get_label(chip_name, feature)) == NULL)
				continue;

			/* iterate readable sub-features */
			subfeature_nr = 0;
			while ((subfeature = sensors_get_all_subfeatures(chip_name, feature, &subfeature_nr))) {
				if (!(subfeature->flags & SENSORS_MODE_R))
					continue;

				switch(chip) {
					case thinkpad:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_FAN_INPUT:
								sensors_get_value(chip_name, subfeature->number, &value);
								if ((short)value != -1) {
									sts.thinkpadFanMax = MAX(sts.thinkpadFanMax, (int) (value + 0.5));
								}
								break;
							default:
								break;
						}
						break;
					case amdgpu:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_TEMP_INPUT:
								sensors_get_value(chip_name, subfeature->number, &value);
								sts.amdgpuTempMax = MAX(sts.amdgpuTempMax, (int) (value + 0.5));
								break;
							case SENSORS_SUBFEATURE_POWER_AVERAGE:
								sensors_get_value(chip_name, subfeature->number, &value);
								sts.amdgpuPowerTotal += (int) (value + 0.5);
								break;
							default:
								break;
						}
						break;
					case k10Temp:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_TEMP_INPUT:
								if (strcmp(label, "Tdie") == 0) {
									sensors_get_value(chip_name, subfeature->number, &value);
									sts.k10tempTdieMax = MAX(sts.k10tempTdieMax, (int)(value + 0.5));
								}
								break;
							default:
								break;
						}
						break;
					case coretemp:
						switch (subfeature->type) {
						case SENSORS_SUBFEATURE_TEMP_INPUT:
							sensors_get_value(chip_name, subfeature->number, &value);
							sts.coreTempMax = MAX(sts.coreTempMax, (int)(value + 0.5));
							break;
						default:
							break;
						}
						break;
					case dell:
						switch (subfeature->type) {
						case SENSORS_SUBFEATURE_FAN_INPUT:
							sensors_get_value(chip_name, subfeature->number, &value);
							sts.dellFanMax = MAX(sts.dellFanMax, (int)(value + 0.5));
							break;
						default:
							break;
						}
						break;
				}
			}
			if (label) {
				free(label);
			}
		}
	}

	/* promises not to error */
	sensors_cleanup();

	return sts;
}

/* render max stats as a string with a trailing newline */
/* static buffer is returned, do not free */
const char *render(const Sts sts, const bool gpu) {
	static char buf[128];

	char *pbuf = buf;

	if (gpu) {
		if (sts.amdgpuPowerTotal)
			pbuf += sprintf(pbuf, "%s%iW", pbuf == buf ? "" : " ", sts.amdgpuPowerTotal);

		if (sts.amdgpuTempMax)
			pbuf += sprintf(pbuf, "%s%i°C", pbuf == buf ? "" : " ", sts.amdgpuTempMax);
	}

	if (sts.coreTempMax)
		pbuf += sprintf(pbuf, "%s%i°C", pbuf == buf ? "" : "    ", sts.coreTempMax);

	if (sts.k10tempTdieMax)
		pbuf += sprintf(pbuf, "%s%i°C", pbuf == buf ? "" : "    ", sts.k10tempTdieMax);

	if (sts.thinkpadFanMax)
		pbuf += sprintf(pbuf, "%s%irpm", pbuf == buf ? "" : " ", sts.thinkpadFanMax);

	if (sts.dellFanMax)
		pbuf += sprintf(pbuf, "%s%irpm", pbuf == buf ? "" : " ", sts.dellFanMax);

	return buf;
}

const char *
lm_sensors(const char *opts)
{
	static int invocation = 0;
	static const char *output;

	if (invocation == 0)
		output = render(collect(), strstr(opts, "gpu"));

	if (++invocation >= 3)
		invocation = 0;

	return output;
}
