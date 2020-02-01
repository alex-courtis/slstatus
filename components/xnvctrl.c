#include <X11/Xlib.h>
#include <NVCtrl/NVCtrl.h>
#include <NVCtrl/NVCtrlLib.h>

#include "../util.h"

// see git@github.com:NVIDIA/nvidia-settings.git nv-control-dvc.c
const char *nvctrl(void) {
    Display *dpy;
    int retval;

    if (!(dpy = XOpenDisplay(NULL))) {
        warn("XOpenDisplay: Failed to open display");
        return NULL;
    }

    // hard coded to screen 0; enumerating the nvidia screens is cumbersome
    if (!XNVCTRLQueryAttribute(dpy, 0, 0, NV_CTRL_GPU_CORE_TEMPERATURE, &retval)) {
        warn("XNVCTRLQueryAttribute: Failed to query NV_CTRL_GPU_CORE_TEMPERATURE "
             "on display device DPY-%d of screen %d of '%s'.",
             0, 0, XDisplayName(NULL));
        XCloseDisplay(dpy);
        return NULL;
    }

    XCloseDisplay(dpy);

    return bprintf("%i°C", retval);
}
