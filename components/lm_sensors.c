/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sensors/sensors.h>

#include "../util.h"
#include "../slstatus.h"

#define MAX(A, B) ((A) > (B) ? (A) : (B))

void dbg(const char *restrict __format, ...) {
	if (!DBG)
		return;

	va_list args;
	va_start(args, __format);
	vfprintf(stderr, __format, args);
	va_end(args);
}

enum Chip { k10Temp, amdgpu, thinkpad, coretemp, dell, asus_wmi };

typedef struct {
	int amdgpuTempEdge;
	int amdgpuPowerAverage;
	int k10tempTdie;
	int thinkpadFan;
	int dellFan;
	int coreTemp;
	int asusWmiFanCpu;
	int asusWmiFanCpuOpt;
	int asusWmiFanChassis1;
	int asusWmiFanChassis2;
	int asusWmiWaterPump;
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
			.amdgpuPowerAverage = 0,
			.amdgpuTempEdge = 0,
			.k10tempTdie = 0,
			.thinkpadFan = 0,
			.dellFan = 0,
			.coreTemp = 0,
	};

	/* init; clean up is done at end */
	sensors_init(NULL);

	/* iterate chips */
	chip_nr = 0;
	while ((chip_name = sensors_get_detected_chips(NULL, &chip_nr))) {

		dbg("%s\n", chip_name->prefix);

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
		else if (strcmp(chip_name->prefix, "asus_wmi_sensors") == 0)
			chip = asus_wmi;
		else
			continue;

		/* iterate features */
		feature_nr = 0;
		while ((feature = sensors_get_features(chip_name, &feature_nr))) {
			if ((label = sensors_get_label(chip_name, feature)) == NULL)
				continue;
			dbg(" %s %s\n", label, feature->name);

			/* iterate readable sub-features */
			subfeature_nr = 0;
			while ((subfeature = sensors_get_all_subfeatures(chip_name, feature, &subfeature_nr))) {
				if (!(subfeature->flags & SENSORS_MODE_R))
					continue;

				sensors_get_value(chip_name, subfeature->number, &value);
				dbg("  %s %g\n", subfeature->name, value);

				switch(chip) {
					case thinkpad:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_FAN_INPUT:
								if ((short)value != -1) {
									sts.thinkpadFan = MAX(sts.thinkpadFan, (int) (value + 0.5));
								}
								break;
							default:
								break;
						}
						break;
					case amdgpu:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_TEMP_INPUT:
								// edge, junction, mem; mangohud uses edge
								if (strcmp(label, "edge") == 0) {
									sts.amdgpuTempEdge = MAX(sts.amdgpuTempEdge, (int)(value + 0.5));
								}
								break;
							case SENSORS_SUBFEATURE_POWER_AVERAGE:
								sts.amdgpuPowerAverage = MAX(sts.amdgpuPowerAverage, (int)(value + 0.5));
								break;
							default:
								break;
						}
						break;
					case k10Temp:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_TEMP_INPUT:
								// Tctl is offset +27 degrees, Tdie is derived from junction
								if (strcmp(label, "Tdie") == 0) {
									sts.k10tempTdie = MAX(sts.k10tempTdie, (int)(value + 0.5));
								}
								break;
							default:
								break;
						}
						break;
					case coretemp:
						switch (subfeature->type) {
						case SENSORS_SUBFEATURE_TEMP_INPUT:
							sts.coreTemp = MAX(sts.coreTemp, (int)(value + 0.5));
							break;
						default:
							break;
						}
						break;
					case dell:
						switch (subfeature->type) {
						case SENSORS_SUBFEATURE_FAN_INPUT:
							sts.dellFan = MAX(sts.dellFan, (int)(value + 0.5));
							break;
						default:
							break;
						}
						break;
					case asus_wmi:
						switch (subfeature->type) {
						case SENSORS_SUBFEATURE_FAN_INPUT:
							if (strcmp(label, "CPU Fan") == 0) {
								sts.asusWmiFanCpu = (int)(value + 0.5);
							} else if (strcmp(label, "CPU OPT") == 0) {
								sts.asusWmiFanCpuOpt = (int)(value + 0.5);
							} else if (strcmp(label, "Chassis Fan 1") == 0) {
								sts.asusWmiFanChassis1 = (int)(value + 0.5);
							} else if (strcmp(label, "Chassis Fan 2") == 0) {
								sts.asusWmiFanChassis2 = (int)(value + 0.5);
							} else if (strcmp(label, "Water Pump 1") == 0) {
								sts.asusWmiWaterPump = (int)(value + 0.5);
							}
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
const char *render(const Sts sts, const bool amdgpu) {
	static char buf[128];

	char *pbuf = buf;

	if (amdgpu) {
		if (sts.amdgpuPowerAverage)
			pbuf += sprintf(pbuf, "│ %iW ", sts.amdgpuPowerAverage);

		if (sts.amdgpuTempEdge)
			pbuf += sprintf(pbuf, "%i°C ", sts.amdgpuTempEdge);
	}

	if (sts.coreTemp)
		pbuf += sprintf(pbuf, "│ %i°C ", sts.coreTemp);

	if (sts.k10tempTdie)
		pbuf += sprintf(pbuf, "│ %i°C ", sts.k10tempTdie);

	if (sts.asusWmiFanCpu)
		pbuf += sprintf(pbuf, "│ CPU %iR ", sts.asusWmiFanCpu);

	if (sts.asusWmiFanCpuOpt)
		pbuf += sprintf(pbuf, "│ OPT %iR ", sts.asusWmiFanCpuOpt);

	if (sts.asusWmiFanChassis1)
		pbuf += sprintf(pbuf, "│ Chas1 %iR ", sts.asusWmiFanChassis1);

	if (sts.asusWmiFanChassis2)
		pbuf += sprintf(pbuf, "│ Chas2 %iR ", sts.asusWmiFanChassis2);

	if (sts.asusWmiWaterPump)
		pbuf += sprintf(pbuf, "│ Pump %iR ", sts.asusWmiWaterPump);

	if (sts.thinkpadFan)
		pbuf += sprintf(pbuf, "│ %iR ", sts.thinkpadFan);

	if (sts.dellFan)
		pbuf += sprintf(pbuf, "│ %iR ", sts.dellFan);

	return buf;
}

const char *
lm_sensors(const char *opts)
{
	static int invocation = 0;
	static const char *output;

	if (invocation == 0)
		output = render(collect(), strstr(opts, "amdgpu"));

	if (++invocation >= 3)
		invocation = 0;

	return output;
}
