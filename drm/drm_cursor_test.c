/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bs_drm.h"

int main(int argc, char **argv)
{
	uint32_t cursor_size = 64;
	if (argc >= 2) {
		char *end_str;
		unsigned long new_cursor_size = strtoul(argv[1], &end_str, 0);
		if (end_str == argv[1] || new_cursor_size == 0 || new_cursor_size > UINT32_MAX) {
			printf(
			    "usage:\n  drm_cursor_test [cursor size]\n\nCursor size defaults to "
			    "%u\n",
			    cursor_size);
			return 1;
		}
		cursor_size = (uint32_t)new_cursor_size;
	}

	int fd = bs_drm_open_main_display();
	if (fd < 0) {
		bs_debug_error("failed to open card for display");
		return 1;
	}

	struct gbm_device *gbm = gbm_create_device(fd);
	if (!gbm) {
		bs_debug_error("failed to create gbm");
		return 1;
	}

	struct bs_drm_pipe pipe = { 0 };
	if (!bs_drm_pipe_make(fd, &pipe)) {
		bs_debug_error("failed to make pipe");
		return 1;
	}

	drmModeConnector *connector = drmModeGetConnector(fd, pipe.connector_id);
	drmModeModeInfo *mode = &connector->modes[0];
	uint32_t crtc_id = pipe.crtc_id;

	// Restart the cursor position before binding the crtc so that the old cursor position isn't
	// displayed briefly when the display is turned activated.
	int ret = drmModeMoveCursor(fd, crtc_id, 0, 0);
	if (ret) {
		bs_debug_error("failed to move cursor: %d", ret);
		return 1;
	}

	struct gbm_bo *fb_bo =
	    gbm_bo_create(gbm, mode->hdisplay, mode->vdisplay, GBM_FORMAT_XRGB8888,
			  GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
	if (!fb_bo) {
		bs_debug_error("failed to create buffer object for frame buffer");
		return 1;
	}

	struct bs_mapper *mapper = bs_mapper_gem_new();
	if (mapper == NULL) {
		bs_debug_error("failed to create mapper object");
		return 1;
	}

	void *map_data;
	uint8_t *fb_ptr = bs_mapper_map(mapper, fb_bo, 0, &map_data);
	if (fb_ptr == MAP_FAILED) {
		bs_debug_error("failed to mmap frame buffer object");
		return 1;
	}
	uint32_t stride = gbm_bo_get_stride(fb_bo);
	for (size_t y = 0; y < mode->vdisplay; y++) {
		for (size_t x = 0; x < mode->hdisplay; x++) {
			// Solid blue fill
			fb_ptr[y * stride + x * 4 + 0] = 0xff;
			fb_ptr[y * stride + x * 4 + 1] = 0;
			fb_ptr[y * stride + x * 4 + 2] = 0;
			fb_ptr[y * stride + x * 4 + 3] = 0;
		}
	}
	bs_mapper_unmap(mapper, fb_bo, map_data);

	uint32_t fb_id = bs_drm_fb_create_gbm(fb_bo);
	if (!fb_id) {
		bs_debug_error("failed to create frame buffer from buffer object");
		return 1;
	}

	ret = drmModeSetCrtc(fd, crtc_id, fb_id, 0 /* x */, 0 /* y */, &pipe.connector_id,
			     1 /* connector count */, mode);
	if (ret) {
		bs_debug_error("failed to set crtc: %d", ret);
		return 1;
	}

	struct gbm_bo *cursor_bo =
	    gbm_bo_create(gbm, cursor_size, cursor_size, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR);
	if (!cursor_bo) {
		bs_debug_error("failed to create cursor buffer object");
		return 1;
	}

	uint8_t *cursor_ptr = bs_mapper_map(mapper, cursor_bo, 0, &map_data);
	if (cursor_ptr == MAP_FAILED) {
		bs_debug_error("failed to mmap cursor buffer object");
		return 1;
	}
	for (size_t y = 0; y < cursor_size; y++) {
		for (size_t x = 0; x < cursor_size; x++) {
			// A white triangle pointing right
			bool color_white = y > x / 2 && y < (cursor_size - x / 2);
			memset(&cursor_ptr[y * cursor_size * 4 + x * 4], color_white ? 0xFF : 0x00,
			       4);
		}
	}
	bs_mapper_unmap(mapper, cursor_bo, map_data);

	ret = drmModeSetCursor(fd, crtc_id, gbm_bo_get_handle(cursor_bo).u32, cursor_size,
			       cursor_size);
	if (ret) {
		bs_debug_error("failed to set cursor: %d", ret);
		return 1;
	}

	// Divisor chosen so that the test would last about 10 seconds @ 60fps.
	const uint32_t divisor = 25;
	const uint32_t xinc = mode->hdisplay / divisor;
	const uint32_t yinc = mode->vdisplay / divisor;
	for (uint32_t x = 0; x < divisor; x++) {
		for (uint32_t y = 0; y < divisor; y++) {
			ret = drmModeMoveCursor(fd, crtc_id, x * xinc + y, y * yinc + x);
			if (ret) {
				bs_debug_error("failed to move cursor: %d", ret);
				return 1;
			}
			usleep(16667);
		}
	}

	bs_mapper_destroy(mapper);
	drmModeRmFB(fd, fb_id);
	gbm_bo_destroy(fb_bo);
	gbm_bo_destroy(cursor_bo);
	gbm_device_destroy(gbm);
	close(fd);

	return 0;
}
