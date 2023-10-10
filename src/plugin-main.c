/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <plugin-support.h>
#include <stdio.h>
#include <string.h>
#include "img-utils.h"
#include "ocr.h"
#include "game-detect/smash-ultimate.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define SETTINGS_OUT_PATH "out_path"
#define DETECT_INTERVAL 0.05f
#define CAPTURE_INTERVAL 10.0f

struct autovod_ctx {
	pthread_mutex_t mutex;
	pthread_cond_t cv;
	pthread_t thread;
	bool should_run;
	bool running;
	struct obs_source *source;
	gs_texrender_t *texrender;
	gs_stagesurf_t *staging_surface;
	char *out_path;
	uint32_t width;
	uint32_t height;
	float seconds_since_last_detect;
	float seconds_since_last_capture;
	struct frame_data *capture_frame;
};

static void *autovod_thread(void *data)
{
	struct autovod_ctx *autovod = data;

	pthread_mutex_lock(&autovod->mutex);
	autovod->running = true;

	while (1) {
		while (autovod->should_run && !autovod->capture_frame) {
			pthread_cond_wait(&autovod->cv, &autovod->mutex);
		}

		if (!autovod->should_run) {
			break;
		}

		pthread_mutex_unlock(&autovod->mutex);
		ssbu_detect(autovod->capture_frame);
		pthread_mutex_lock(&autovod->mutex);

		frame_data_destroy(autovod->capture_frame);
		bfree(autovod->capture_frame);
		autovod->capture_frame = NULL;
	}

	autovod->running = false;
	if (autovod->capture_frame) {
		frame_data_destroy(autovod->capture_frame);
		bfree(autovod->capture_frame);
		autovod->capture_frame = NULL;
	}
	pthread_mutex_unlock(&autovod->mutex);

	pthread_exit(NULL);
	return NULL;
}

static const char *autovod_plugin_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Autovod Filter";
}

static obs_properties_t *autovod_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, SETTINGS_OUT_PATH, "Destination", OBS_PATH_DIRECTORY, "*.*",
				NULL);

	return props;
}

static void autovod_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SETTINGS_OUT_PATH, "/Users/Tom/Downloads");
}

static void autovod_on_update(void *data, obs_data_t *settings)
{
	struct autovod_ctx *autovod = data;

	const char *out_path = obs_data_get_string(settings, SETTINGS_OUT_PATH);

	//TODO: check how the memory management works here (out_path is a string)
	pthread_mutex_lock(&autovod->mutex);
	autovod->out_path = (char *)out_path;
	pthread_mutex_unlock(&autovod->mutex);

	obs_log(LOG_INFO, "settings updated: out_path='%s'", autovod->out_path);
}

static void autovod_on_destroy(void *data)
{
	struct autovod_ctx *autovod = data;

	if (autovod->thread) {
		pthread_mutex_lock(&autovod->mutex);
		autovod->should_run = false;
		pthread_cond_broadcast(&autovod->cv);
		pthread_mutex_unlock(&autovod->mutex);

		(void)pthread_join(autovod->thread, NULL);
		autovod->thread = 0;
	}

	if (autovod->texrender) {
		obs_enter_graphics();
		gs_texrender_destroy(autovod->texrender);
		obs_leave_graphics();
	}

	pthread_mutex_destroy(&autovod->mutex);
	pthread_cond_destroy(&autovod->cv);
	bfree(autovod);

	obs_log(LOG_INFO, "plugin destroyed successfully");
}

static void source_removed_callback(void *param, calldata_t *unused_data)
{
	struct autovod_ctx *autovod = param;
	(void)unused_data;

	obs_source_remove(autovod->source);
	obs_source_release(autovod->source);
}

static void *autovod_on_create(obs_data_t *settings, obs_source_t *context)
{
	int ret;
	struct autovod_ctx *autovod = bzalloc(sizeof(struct autovod_ctx));

	autovod->source = context;
	autovod->should_run = true;
	autovod->running = false;
	pthread_mutex_init(&autovod->mutex, NULL);
	pthread_cond_init(&autovod->cv, NULL);

	obs_enter_graphics();
	autovod->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();

	obs_source_t *target = obs_filter_get_target(autovod->source);
	signal_handler_t *sh = obs_source_get_signal_handler(target);
	signal_handler_connect(sh, "remove", source_removed_callback, autovod);

	ret = pthread_create(&autovod->thread, NULL, autovod_thread, autovod);
	if (ret != 0) {
		obs_log(LOG_ERROR, "failed to create thread");
		goto error;
	}

	obs_source_update(context, settings);

	obs_log(LOG_INFO, "plugin created successfully");
	return autovod;

error:
	autovod_on_destroy(autovod);
	return NULL;
}

static void autovod_on_tick(void *data, float seconds)
{
	struct autovod_ctx *autovod = data;

	obs_source_t *target = obs_filter_get_target(autovod->source);

	if (!target) {
		autovod->width = 0;
		autovod->height = 0;

		if (autovod->staging_surface) {
			obs_enter_graphics();
			gs_stagesurface_destroy(autovod->staging_surface);
			obs_leave_graphics();
			autovod->staging_surface = NULL;
			autovod->seconds_since_last_detect = 0;
			autovod->seconds_since_last_capture = 0;
		}

		return;
	}

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);

	pthread_mutex_lock(&autovod->mutex);

	if (width != autovod->width || height != autovod->height) {
		autovod->width = width;
		autovod->height = height;

		obs_enter_graphics();
		if (autovod->staging_surface) {
			gs_stagesurface_destroy(autovod->staging_surface);
		}
		autovod->staging_surface =
			gs_stagesurface_create(autovod->width, autovod->height, GS_RGBA);
		obs_leave_graphics();
	}

	autovod->seconds_since_last_detect += seconds;
	autovod->seconds_since_last_capture += seconds;

	pthread_mutex_unlock(&autovod->mutex);
}

static void autovod_on_render(void *data, gs_effect_t *unused_effect)
{
	struct autovod_ctx *autovod = data;
	UNUSED_PARAMETER(unused_effect);

	bool detect_cooldown = autovod->seconds_since_last_detect >= DETECT_INTERVAL;
	bool capture_cooldown = autovod->seconds_since_last_capture >= CAPTURE_INTERVAL;
	obs_source_t *target = obs_filter_get_target(autovod->source);
	obs_source_t *parent = obs_filter_get_parent(autovod->source);

	if (!parent || !autovod->width || !autovod->height || !detect_cooldown ||
	    !capture_cooldown) {
		obs_source_skip_video_filter(autovod->source);
		return;
	}

	gs_texrender_reset(autovod->texrender);

	if (gs_texrender_begin(autovod->texrender, autovod->width, autovod->height)) {
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)autovod->width, 0.0f, (float)autovod->height, -100.0f,
			 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		if (target) {
			obs_source_inc_showing(target);
			obs_source_video_render(target);
			obs_source_dec_showing(target);
		} else {
			obs_render_main_texture();
		}

		gs_blend_state_pop();
		gs_texrender_end(autovod->texrender);
	}

	gs_texture_t *tex = gs_texrender_get_texture(autovod->texrender);
	if (tex) {
		gs_stage_texture(autovod->staging_surface, tex);

		uint8_t *data;
		uint32_t linesize;

		pthread_mutex_lock(&autovod->mutex);
		if (gs_stagesurface_map(autovod->staging_surface, &data, &linesize)) {
			//TODO: handle case where linesize != width * 4
			//		handle case where image isnt processed before next frame
			//		goto error handling if allocating fails

			struct frame_data tmp_frame = {
				.rgba_data = data,
				.width = autovod->width,
				.height = autovod->height,
			};

			if (ssbu_detect_loadin_screen(&tmp_frame)) {
				struct frame_data *frame = bzalloc(sizeof(struct frame_data));
				frame_data_init(frame, autovod->width, autovod->height);

				memcpy(frame->rgba_data, data, linesize * autovod->height);
				autovod->capture_frame = frame;
				autovod->seconds_since_last_capture = 0;
				pthread_cond_broadcast(&autovod->cv);
			}
			autovod->seconds_since_last_detect = 0;

			gs_stagesurface_unmap(autovod->staging_surface);
		}
		pthread_mutex_unlock(&autovod->mutex);

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, tex);

		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(tex, 0, autovod->width, autovod->height);
	}
}

struct obs_source_info autovod_def = {
	.id = "autovod_filter",

	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,

	.get_name = autovod_plugin_get_name,

	.get_properties = autovod_get_properties,
	.get_defaults = autovod_get_defaults,
	.update = autovod_on_update,

	.create = autovod_on_create,
	.destroy = autovod_on_destroy,

	.video_tick = autovod_on_tick,
	.video_render = autovod_on_render,
};

bool obs_module_load(void)
{
	ocr_init();
	obs_register_source(&autovod_def);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	ocr_destroy();
	obs_log(LOG_INFO, "plugin unloaded");
}
