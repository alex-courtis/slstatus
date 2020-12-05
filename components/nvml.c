#include <stddef.h>
#include <stdio.h>
#include "nvml.h"

#include "../util.h"

/**
 * Took used bits out of example.c from http://developer.download.nvidia.com/compute/cuda/7.5/Prod/gdk/gdk_linux_amd64_352_79_release.run
 */

const char *nvml(void) {
    static char b[1024];
    nvmlReturn_t result;
    unsigned int device_count = 0;
    unsigned int temperatureInC = 0;
    unsigned int powerInMilliWatts = 0;

    result = nvmlInit();
    if (NVML_SUCCESS != result)
    { 
        warn("nvmlInit fail: %s", nvmlErrorString(result));
        return NULL;
    }

    result = nvmlDeviceGetCount(&device_count);
    if (NVML_SUCCESS != result)
    { 
        warn("nvmlDeviceGetCount fail: %s", nvmlErrorString(result));
        return NULL;
    }

    // once card is enough for now
    if (device_count != 1)
    {
        return NULL;
    }

    nvmlDevice_t device;
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (NVML_SUCCESS != result)
    {
        warn("nvmlDeviceGetHandleByIndex fail: %s", nvmlErrorString(result));
        return NULL;
    }

    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperatureInC);
    if (NVML_SUCCESS != result)
    {
        warn("nvmlDeviceGetTemperature fail: %s", nvmlErrorString(result));
        return NULL;
    }

    result = nvmlDeviceGetPowerUsage(device, &powerInMilliWatts);
    if (NVML_SUCCESS != result && NVML_ERROR_NOT_SUPPORTED != result)
    {
        warn("nvmlDeviceGetPowerUsage fail: %s", nvmlErrorString(result));
        return NULL;
    }

    result = nvmlShutdown();
    if (NVML_SUCCESS != result)
    {
        warn("nvmlShutdown fail: %s", nvmlErrorString(result));
    }

    char *pb = b;

    if (powerInMilliWatts)
        pb += sprintf(pb, "%uW", (powerInMilliWatts + 500) / 1000);

    if (pb != b)
        pb += sprintf(pb, " ");

    if (temperatureInC)
        pb += sprintf(pb, "%u°C", temperatureInC);

    return b;
}
