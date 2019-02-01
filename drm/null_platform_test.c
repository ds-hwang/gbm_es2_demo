/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bs_drm.h"

#include <getopt.h>

#define NUM_BUFFERS 2

static GLuint solid_shader_create()
{
	const GLchar *vert =
	    "attribute vec4 vPosition;\n"
	    "attribute vec4 vColor;\n"
	    "varying vec4 vFillColor;\n"
	    "void main() {\n"
	    "  gl_Position = vPosition;\n"
	    "  vFillColor = vColor;\n"
	    "}\n";

	const GLchar *frag =
	    "precision mediump float;\n"
	    "varying vec4 vFillColor;\n"
	    "void main() {\n"
	    "  gl_FragColor = vFillColor;\n"
	    "}\n";

	struct bs_gl_program_create_binding bindings[] = {
		{ 0, "vPosition" }, { 1, "vColor" }, { 0, NULL },
	};

	return bs_gl_program_create_vert_frag_bind(vert, frag, bindings);
}

static float f(int i)
{
	int a = i % 40;
	int b = (i / 40) % 6;
	switch (b) {
		case 0:
		case 1:
			return 0.0f;
		case 3:
		case 4:
			return 1.0f;
		case 2:
			return (a / 40.0f);
		case 5:
			return 1.0f - (a / 40.0f);
		default:
			return 0.0f;
	}
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec,
			      void *data)
{
	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

static const struct option longopts[] = {
	{ "test-page-flip-format-change", no_argument, NULL, 'f' },
	{ "help", no_argument, NULL, 'h' },
	{ 0, 0, 0, 0 },
};

static void print_help(const char *argv0)
{
	// clang-format off
	printf("usage: %s [OPTIONS] [drm_device_path]\n", argv0);
	printf("  -f, --test-page-flip-format-change		    test page flips alternating RGB and BGR fbs\n");
	printf("  -h, --help		    show help\n");
	printf("\n");
	printf("\n");
	// clang-format on
}

int main(int argc, char **argv)
{
	int fd = -1;
	bool test_page_flip_format_change = false;
	bool help_flag = false;

	int c = -1;
	while ((c = getopt_long(argc, argv, "fh", longopts, NULL)) != -1) {
		switch (c) {
			case 'f':
				test_page_flip_format_change = true;
				break;
			case 'h':
				help_flag = true;
				break;
		}
	}

	if (help_flag) {
		print_help(*argv);
	}

	if (optind < argc) {
		fd = open(argv[optind], O_RDWR);
		if (fd < 0) {
			bs_debug_error("failed to open card %s", argv[optind]);
			return 1;
		}
	} else {
		fd = bs_drm_open_main_display();
		if (fd < 0) {
			bs_debug_error("failed to open card for display");
			return 1;
		}
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
	assert(connector);
	drmModeModeInfo *mode = &connector->modes[0];

	struct bs_egl *egl = bs_egl_new();
	if (!bs_egl_setup(egl)) {
		bs_debug_error("failed to setup egl context");
		return 1;
	}

	struct gbm_bo *bos[NUM_BUFFERS];
	uint32_t ids[NUM_BUFFERS];
	struct bs_egl_fb *egl_fbs[NUM_BUFFERS];
	EGLImageKHR egl_images[NUM_BUFFERS];
	for (size_t fb_index = 0; fb_index < NUM_BUFFERS; fb_index++) {
		uint32_t format = GBM_FORMAT_XRGB8888;
		if (test_page_flip_format_change && fb_index) {
			format = GBM_FORMAT_XBGR8888;
		}

		bos[fb_index] = gbm_bo_create(gbm, mode->hdisplay, mode->vdisplay, format,
					      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
		if (bos[fb_index] == NULL) {
			bs_debug_error("failed to allocate framebuffer");
			return 1;
		}

		ids[fb_index] = bs_drm_fb_create_gbm(bos[fb_index]);
		if (ids[fb_index] == 0) {
			bs_debug_error("failed to create framebuffer id");
			return 1;
		}

		EGLImageKHR egl_image = bs_egl_image_create_gbm(egl, bos[fb_index]);
		if (egl_image == EGL_NO_IMAGE_KHR) {
			bs_debug_error("failed to create EGLImageKHR from framebuffer");
			return 1;
		}

		egl_fbs[fb_index] = bs_egl_fb_new(egl, egl_image);
		if (!egl_fbs[fb_index]) {
			bs_debug_error("failed to create framebuffer from EGLImageKHR");
			return 1;
		}
		egl_images[fb_index] = egl_image;
	}

	int ret = drmModeSetCrtc(fd, pipe.crtc_id, ids[0], 0 /* x */, 0 /* y */, &pipe.connector_id,
				 1 /* connector count */, mode);
	if (ret) {
		bs_debug_error("failed to set CRTC");
		return 1;
	}

	GLuint program = solid_shader_create();
	if (!program) {
		bs_debug_error("failed to create solid shader");
		return 1;
	}

	int fb_idx = 1;
	for (int i = 0; i <= 500; i++) {
		int waiting_for_flip = 1;
		// clang-format off
		GLfloat verts[] = {
			0.0f, -0.5f, 0.0f,
			-0.5f, 0.5f, 0.0f,
			0.5f, 0.5f, 0.0f
		};
		GLfloat colors[] = {
			1.0f, 0.0f, 0.0f, 1.0f,
			0.0f, 1.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 1.0f
		};
		// clang-format on

		glBindFramebuffer(GL_FRAMEBUFFER, bs_egl_fb_name(egl_fbs[fb_idx]));
		glViewport(0, 0, (GLint)mode->hdisplay, (GLint)mode->vdisplay);

		glClearColor(f(i), f(i + 80), f(i + 160), 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(program);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, colors);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		usleep(1e6 / 120); /* 120 Hz */
		glFinish();

		if (!bs_egl_image_flush_external(egl, egl_images[fb_idx])) {
			bs_debug_error("failed to call image_flush_external");
			return 1;
		}

		ret = drmModePageFlip(fd, pipe.crtc_id, ids[fb_idx], DRM_MODE_PAGE_FLIP_EVENT,
				      &waiting_for_flip);
		if (ret) {
			bs_debug_error("failed page flip: %d", ret);
			return 1;
		}

		while (waiting_for_flip) {
			drmEventContext evctx = {
				.version = DRM_EVENT_CONTEXT_VERSION,
				.page_flip_handler = page_flip_handler,
			};

			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			ret = select(fd + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				bs_debug_error("select err: %s", strerror(errno));
				return 1;
			} else if (ret == 0) {
				bs_debug_error("select timeout");
				return 1;
			}
			ret = drmHandleEvent(fd, &evctx);
			if (ret) {
				bs_debug_error("failed to wait for page flip: %d", ret);
				return 1;
			}
		}
		fb_idx = fb_idx ^ 1;
	}

	for (size_t fb_index = 0; fb_index < NUM_BUFFERS; fb_index++) {
		bs_egl_fb_destroy(&egl_fbs[fb_index]);
		bs_egl_image_destroy(egl, &egl_images[fb_index]);
		drmModeRmFB(fd, ids[fb_idx]);
		gbm_bo_destroy(bos[fb_index]);
	}

	bs_egl_destroy(&egl);
	gbm_device_destroy(gbm);
	close(fd);
	return 0;
}
