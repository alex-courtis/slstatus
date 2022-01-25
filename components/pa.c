/* See LICENSE file for copyright and license details. */
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../util.h"

#include <pulse/pulseaudio.h>

#define PA_SUBSCRIPTIONS PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SERVER

#define PAD3 bp != buf ? "   " : ""

struct state {
	int src_perc;
	int sink_perc;

	bool other_unmuted_srcs;
	bool other_unmuted_sinks;

	char *sink_def;
	char *src_def;

	bool available;
};

int calc_perc(pa_cvolume volume, bool mute);
void source_info_cb(pa_context *c, const pa_source_info *i, int eol, void *userdata);
void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata);
void server_info_cb(pa_context *c, const pa_server_info *i, void *userdata);
void subscribe_cb(pa_context *c, pa_subscription_event_type_t type, uint32_t index, void *userdata);

int calc_perc(pa_cvolume volume, bool mute) {
	if (mute) {
		return -1;
	}

	pa_volume_t vol_avg = pa_cvolume_avg(&volume);
	if (vol_avg) {
		return round((float)vol_avg * 100 / 65535);
	} else {
		return 0;
	}
}

void source_info_cb(pa_context *c, const pa_source_info *i, int eol, void *userdata) {
	(void)(c);
	(void)(eol);
	struct state *s = userdata;
	if (!i) {
		return;
	}
	if (s->src_def && strcmp(i->name, s->src_def) == 0) {
		s->src_perc = calc_perc(i->volume, i->mute);
	} else if (!i->mute) {
		s->other_unmuted_srcs = true;
	}
}

void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
	(void)(c);
	(void)(eol);
	struct state *s = userdata;
	if (!i) {
		return;
	}
	if (s->sink_def && strcmp(i->name, s->sink_def) == 0) {
		s->sink_perc = calc_perc(i->volume, i->mute);
	} else if (!i->mute) {
		s->other_unmuted_sinks = true;
	}
}

void server_info_cb(pa_context *c, const pa_server_info *i, void *userdata) {
	struct state *s = userdata;
	if (!i) {
		return;
	}

	if (s->sink_def) {
		free(s->sink_def);
	}
	if (s->src_def) {
		free(s->src_def);
	}
	s->sink_def = strdup(i->default_sink_name);
	s->src_def = strdup(i->default_source_name);

	// (re)discover volumes
	subscribe_cb(c, PA_SUBSCRIPTION_EVENT_SINK, 0, s);
	subscribe_cb(c, PA_SUBSCRIPTION_EVENT_SOURCE, 0, s);
}

void subscribe_cb(pa_context *c, pa_subscription_event_type_t type, uint32_t index, void *userdata) {
	(void)(index);
	struct state *s = userdata;
	switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK:
			s->other_unmuted_sinks = false;
			pa_context_get_sink_info_list(c, sink_info_cb, userdata);
			break;
		case PA_SUBSCRIPTION_EVENT_SOURCE:
			s->other_unmuted_srcs = false;
			pa_context_get_source_info_list(c, source_info_cb, userdata);
			break;
		case PA_SUBSCRIPTION_EVENT_SERVER:
			pa_context_get_server_info(c, server_info_cb, userdata);
			break;
		default:
			break;
	}
}

void *pa_loop(void *data) {
	struct state *s = data;

	for (;;) {
		pa_mainloop *mainloop = pa_mainloop_new();
		pa_mainloop_api *mainloop_api = pa_mainloop_get_api(mainloop);

		// context will block inner loop until PA is available
		pa_context *context = pa_context_new(mainloop_api, "slstatus");
		pa_context_connect(context, NULL, PA_CONTEXT_NOFAIL, NULL);
		pa_context_set_subscribe_callback(context, subscribe_cb, s);

		s->available = false;

		bool first_run = true;
		bool fail_or_terminate = false;
		for (;;) {
			switch (pa_context_get_state(context)) {
				case PA_CONTEXT_UNCONNECTED:
				case PA_CONTEXT_CONNECTING:
				case PA_CONTEXT_AUTHORIZING:
				case PA_CONTEXT_SETTING_NAME:
				default:
					pa_mainloop_iterate(mainloop, 1, NULL);
					continue;
					break;
				case PA_CONTEXT_FAILED:
				case PA_CONTEXT_TERMINATED:
					fail_or_terminate = true;
					break;
				case PA_CONTEXT_READY:
					s->available = true;
					break;
			}
			if (fail_or_terminate)
				break;

			if (first_run) {
				first_run = false;
				pa_context_subscribe(context, PA_SUBSCRIPTIONS, NULL, s);

				// discover defaults; will start a volume query
				pa_context_get_server_info(context, server_info_cb, s);
			}

			pa_mainloop_iterate(mainloop, 1, NULL);
		}

		// pa is very leaky; best effort here
		if (context) {
			pa_context_disconnect(context);
			pa_context_unref(context);
		}
		if (mainloop) {
			pa_mainloop_free(mainloop);
		}
		if (s->sink_def) {
			free(s->sink_def);
			s->sink_def = NULL;
		}
		if (s->src_def) {
			free(s->src_def);
			s->sink_def = NULL;
		}
	}

	return NULL;
}

const char *pa() {
	static struct state s = { 0 };
	static char buf[2048];
	static pthread_t pa_thread = 0;

	if (!pa_thread) {
		pthread_create(&pa_thread, NULL, &pa_loop, &s);
		return "";
	}

	char *bp = buf;
	if (s.available) {
		if (s.other_unmuted_srcs)
			bp += sprintf(bp, "Warning: Non-default Microphones Are Active");

		if (s.src_perc != -1)
			bp += sprintf(bp, "%sMic %d%%", PAD3, s.src_perc);

		if (s.sink_perc == -1)
			bp += sprintf(bp, "%sMute", PAD3);
		else if (s.sink_perc != 100)
			bp += sprintf(bp, "%sVol %d%%", PAD3, s.sink_perc);

	} else {
		bp += sprintf(bp, "%sPulse Audio Unavailable", PAD3);
	}

	bp += sprintf(bp, "%s", PAD3);

	return bprintf("%s", buf);
}

