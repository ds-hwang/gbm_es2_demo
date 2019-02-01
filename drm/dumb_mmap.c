/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bs_drm.h"

void *bs_dumb_mmap(int fd, uint32_t handle, size_t size)
{
	assert(fd >= 0);

	struct drm_mode_map_dumb mmap_arg = { 0 };

	mmap_arg.handle = handle;

	int ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mmap_arg);
	if (ret != 0) {
		bs_debug_error("failed DRM_IOCTL_MODE_MAP_DUMB: %d", ret);
		return NULL;
	}

	if (mmap_arg.offset == 0) {
		bs_debug_error("DRM_IOCTL_MODE_MAP_DUMB returned 0 offset");
		return NULL;
	}

	void *ptr = mmap(NULL, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, mmap_arg.offset);

	if (ptr == MAP_FAILED) {
		bs_debug_error("mmap returned MAP_FAILED: %d", errno);
		return NULL;
	}

	return ptr;
}
