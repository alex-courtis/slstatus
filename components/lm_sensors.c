/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sensors/sensors.h>

#include "../util.h"
#include "../slstatus.h"

#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define ASUS_WMI_CPU_FAN_THRESHOLD 1000
#define ASUS_WMI_CHAS_FAN_THRESHOLD 1500
#define ASUS_WMI_WATER_PUMP_THRESHOLD 3000

#define THINKPAD_FAN_THRESHOLD 3500
#define DELL_FAN_THRESHOLD 3500

// disabmiguate integrated
#define EMP_AMDGPU_NAME "amdgpu-pci-0300"

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
	int k10tempTccd2;
	int thinkpadFan;
	int dellFan;
	int coreTemp;
	int asusWmiFanCpu;
	int asusWmiFanCpuOpt;
	int asusWmiFanChassis1;
	int asusWmiFanChassis2;
	int asusWmiWaterPump;
	bool blinkOn;
} Sts;

Sts sts = { 0 };

void zero_sts() {
	sts.amdgpuPowerAverage = 0;
	sts.amdgpuTempEdge = 0;
	sts.k10tempTdie = 0;
	sts.k10tempTccd2 = 0;
	sts.thinkpadFan = 0;
	sts.dellFan = 0;
	sts.coreTemp = 0;
}

/* discover and collect interesting sensor stats */
void collect() {
	static char chip_name_print[256];

	const sensors_chip_name *chip_name;
	int chip_nr;
	const sensors_feature *feature;
	int feature_nr;
	const sensors_subfeature *subfeature;
	int subfeature_nr;
	char *label = NULL;
	double value;
	enum Chip chip;

	zero_sts();

	/* init; clean up is done at end */
	sensors_init(NULL);

	/* iterate chips */
	chip_nr = 0;
	while ((chip_name = sensors_get_detected_chips(NULL, &chip_nr))) {

		sensors_snprintf_chip_name(chip_name_print, sizeof(chip_name_print), chip_name);
		dbg("%s %s\n", chip_name->prefix, chip_name_print);

		/* only interested in known chips */
		if ((strcmp(chip_name->prefix, "amdgpu") == 0) && (strcmp(chip_name_print, EMP_AMDGPU_NAME) == 0))
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

				if (DBG) {
					sensors_get_value(chip_name, subfeature->number, &value);
					dbg("  %s = %g\n", subfeature->name, value);
				}

				switch(chip) {
					case thinkpad:
						switch (subfeature->type) {
							case SENSORS_SUBFEATURE_FAN_INPUT:
								sensors_get_value(chip_name, subfeature->number, &value);
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
									sensors_get_value(chip_name, subfeature->number, &value);
									sts.amdgpuTempEdge = MAX(sts.amdgpuTempEdge, (int)(value + 0.5));
								}
								break;
							case SENSORS_SUBFEATURE_POWER_AVERAGE:
								sensors_get_value(chip_name, subfeature->number, &value);
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
									sensors_get_value(chip_name, subfeature->number, &value);
									sts.k10tempTdie = MAX(sts.k10tempTdie, (int)(value + 0.5));
								// shown by the motherboard 7 segment
								} else if (strcmp(label, "Tccd2") == 0) {
									sensors_get_value(chip_name, subfeature->number, &value);
									sts.k10tempTccd2 = MAX(sts.k10tempTccd2, (int)(value + 0.5));
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
							sts.coreTemp = MAX(sts.coreTemp, (int)(value + 0.5));
							break;
						default:
							break;
						}
						break;
					case dell:
						switch (subfeature->type) {
						case SENSORS_SUBFEATURE_FAN_INPUT:
							sensors_get_value(chip_name, subfeature->number, &value);
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
								sensors_get_value(chip_name, subfeature->number, &value);
								sts.asusWmiFanCpu = (int)(value + 0.5);
							} else if (strcmp(label, "CPU OPT") == 0) {
								// this one is too slow
								// sensors_get_value(chip_name, subfeature->number, &value);
								// sts.asusWmiFanCpuOpt = (int)(value + 0.5);
							} else if (strcmp(label, "Chassis Fan 1") == 0) {
								sensors_get_value(chip_name, subfeature->number, &value);
								sts.asusWmiFanChassis1 = (int)(value + 0.5);
							} else if (strcmp(label, "Chassis Fan 2") == 0) {
								sensors_get_value(chip_name, subfeature->number, &value);
								sts.asusWmiFanChassis2 = (int)(value + 0.5);
							} else if (strcmp(label, "Water Pump 1") == 0) {
								sensors_get_value(chip_name, subfeature->number, &value);
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
}

/* render max stats as a string with a trailing newline */
/* static buffer is returned, do not free */
const char *render(const bool amdgpu) {
	static char buf[1024];

	char *pbuf = buf;

	if (amdgpu) {
		if (sts.amdgpuTempEdge)
			pbuf += sprintf(pbuf, "│ %i°C ", sts.amdgpuTempEdge);

		if (sts.amdgpuPowerAverage)
			pbuf += sprintf(pbuf, "%iW ", sts.amdgpuPowerAverage);
	}

	sts.blinkOn = !sts.blinkOn;

	if (sts.asusWmiFanCpu > ASUS_WMI_CPU_FAN_THRESHOLD) {
		pbuf += sprintf(pbuf, "│ %s %iR ", sts.blinkOn ? "CPU" : "   ", sts.asusWmiFanCpu);
	}

	if (sts.asusWmiFanCpuOpt > ASUS_WMI_CPU_FAN_THRESHOLD) {
		pbuf += sprintf(pbuf, "│ %s %iR ", sts.blinkOn ? "OPT" : "   ", sts.asusWmiFanCpuOpt);
	}

	if (sts.asusWmiFanChassis1 > ASUS_WMI_CHAS_FAN_THRESHOLD) {
		pbuf += sprintf(pbuf, "│ %s %iR ", sts.blinkOn ? "Chas1" : "     ", sts.asusWmiFanChassis1);
	}

	if (sts.asusWmiFanChassis2 > ASUS_WMI_CHAS_FAN_THRESHOLD) {
		pbuf += sprintf(pbuf, "│ %s %iR ", sts.blinkOn ? "Chas2" : "     ", sts.asusWmiFanChassis2);
	}

	if (sts.asusWmiWaterPump > ASUS_WMI_WATER_PUMP_THRESHOLD) {
		pbuf += sprintf(pbuf, "│ %s %iR ", sts.blinkOn ? "Pump" : "    ", sts.asusWmiWaterPump);
	}

	if (sts.thinkpadFan > THINKPAD_FAN_THRESHOLD) {
		if (sts.blinkOn)
			pbuf += sprintf(pbuf, "│ %iR ", sts.thinkpadFan);
		else
			pbuf += sprintf(pbuf, "│       ");
	}

	if (sts.dellFan > DELL_FAN_THRESHOLD) {
		if (sts.blinkOn)
			pbuf += sprintf(pbuf, "│ %iR ", sts.dellFan);
		else
			pbuf += sprintf(pbuf, "│       ");
	}

	if (sts.coreTemp)
		pbuf += sprintf(pbuf, "│ %i°C ", sts.coreTemp);

	if (sts.k10tempTdie)
		pbuf += sprintf(pbuf, "│ %i°C ", sts.k10tempTdie);

	if (sts.k10tempTccd2)
		pbuf += sprintf(pbuf, "│ %i°C ", sts.k10tempTccd2);

	return buf;
}

const char *
lm_sensors(const char *opts)
{
	static int invocation = 0;
	static const char *output;

	if (invocation == 0) {
		collect();
	}

	output = render(strstr(opts, "amdgpu"));

	if (++invocation >= 3)
		invocation = 0;

	return output;
}
