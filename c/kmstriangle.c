/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright (c) 2016 Dongseong Hwang <dongseong.hwang@intel.com>
 * Copyright (c) 2018 Joone Hur <joone.hur@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include "esUtil.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
// double buffering
#define NUM_BUFFERS 2

static struct {
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;

  // Names are the original gl/egl function names with the prefix chopped off.
  PFNEGLCREATEIMAGEKHRPROC CreateImageKHR;
  PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC EGLImageTargetTexture2DOES;
  PFNEGLCREATESYNCKHRPROC CreateSyncKHR;
  PFNEGLCLIENTWAITSYNCKHRPROC ClientWaitSyncKHR;
  int egl_sync_supported;
} egl;

static struct {
  GLuint program;
  GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
  GLuint vbo;
  GLuint positionsoffset, colorsoffset, normalsoffset;
} gl;

static struct {
  struct gbm_device *dev;
  uint32_t front_buffer;
} gbm;

struct framebuffer {
  struct gbm_bo *bo;
  int fd;
  uint32_t fb_id;
  EGLImageKHR image;
  GLuint gl_tex;
  GLuint gl_fb;
};
static struct framebuffer fbs[NUM_BUFFERS];

static struct {
  int fd;
  drmModeModeInfo *mode;
  uint32_t crtc_id;
  uint32_t connector_id;
} drm;

struct drm_fb {
  struct gbm_bo *bo;
  uint32_t fb_id;
};

static const char *get_egl_error() {
  switch (eglGetError()) {
  case EGL_SUCCESS:
    return "EGL_SUCCESS";
  case EGL_NOT_INITIALIZED:
    return "EGL_NOT_INITIALIZED";
  case EGL_BAD_ACCESS:
    return "EGL_BAD_ACCESS";
  case EGL_BAD_ALLOC:
    return "EGL_BAD_ALLOC";
  case EGL_BAD_ATTRIBUTE:
    return "EGL_BAD_ATTRIBUTE";
  case EGL_BAD_CONTEXT:
    return "EGL_BAD_CONTEXT";
  case EGL_BAD_CONFIG:
    return "EGL_BAD_CONFIG";
  case EGL_BAD_CURRENT_SURFACE:
    return "EGL_BAD_CURRENT_SURFACE";
  case EGL_BAD_DISPLAY:
    return "EGL_BAD_DISPLAY";
  case EGL_BAD_SURFACE:
    return "EGL_BAD_SURFACE";
  case EGL_BAD_MATCH:
    return "EGL_BAD_MATCH";
  case EGL_BAD_PARAMETER:
    return "EGL_BAD_PARAMETER";
  case EGL_BAD_NATIVE_PIXMAP:
    return "EGL_BAD_NATIVE_PIXMAP";
  case EGL_BAD_NATIVE_WINDOW:
    return "EGL_BAD_NATIVE_WINDOW";
  case EGL_CONTEXT_LOST:
    return "EGL_CONTEXT_LOST";
  default:
    return "EGL_???";
  }
}

static const char *get_gl_framebuffer_error() {
  switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
  case GL_FRAMEBUFFER_COMPLETE:
    return "GL_FRAMEBUFFER_COMPLETE";
  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
    return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
    return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
  case GL_FRAMEBUFFER_UNSUPPORTED:
    return "GL_FRAMEBUFFER_UNSUPPORTED";
  case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
    return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
  default:
    return "GL_FRAMEBUFFER_???";
  }
}

static int has_extension(const char *extension, const char *extensions) {
  const char *start, *where, *terminator;
  start = extensions;
  for (;;) {
    where = (char *)strstr((const char *)start, extension);
    if (!where)
      break;
    terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ')
      if (*terminator == ' ' || *terminator == '\0')
        return 0;
    start = terminator;
  }
  return -1;
}

static int init_drm(const char *node) {
  drmModeRes *resources;
  drmModeConnector *connector = NULL;
  drmModeEncoder *encoder = NULL;
  int i, area;

  drm.fd = open(node, O_RDWR | O_CLOEXEC);
  if (drm.fd < 0) {
    printf("could not open drm device\n");
    return -1;
  }

  // Retrieve current display configuration information.
  resources = drmModeGetResources(drm.fd);
  if (!resources) {
    printf("drmModeGetResources failed: %s\n", strerror(errno));
    return -1;
  }

  /* find a connected connector: */
  for (i = 0; i < resources->count_connectors; i++) {
    connector = drmModeGetConnector(drm.fd, resources->connectors[i]);
    if (connector->connection == DRM_MODE_CONNECTED) {
      /* it's connected, let's use this! */
      break;
    }
    drmModeFreeConnector(connector);
    connector = NULL;
  }

  if (!connector) {
    /* we could be fancy and listen for hotplug events and wait for
     * a connector..
     */
    printf("no connected connector!\n");
    return -1;
  }

  /* find highest resolution mode: */
  for (i = 0, area = 0; i < connector->count_modes; i++) {
    drmModeModeInfo *current_mode = &connector->modes[i];
    int current_area = current_mode->hdisplay * current_mode->vdisplay;
    if (current_area > area) {
      drm.mode = current_mode;
      area = current_area;
    }
  }

  if (!drm.mode) {
    printf("could not find mode!\n");
    return -1;
  }

  /* find encoder: */
  for (i = 0; i < resources->count_encoders; i++) {
    encoder = drmModeGetEncoder(drm.fd, resources->encoders[i]);
    if (encoder->encoder_id == connector->encoder_id)
      break;
    drmModeFreeEncoder(encoder);
    encoder = NULL;
  }

  if (!encoder) {
    printf("no encoder!\n");
    return -1;
  }

  drm.crtc_id = encoder->crtc_id;
  drm.connector_id = connector->connector_id;

  return 0;
}

static int init_gbm(void) {
  gbm.dev = gbm_create_device(drm.fd);
  return 0;
}

static int init_egl(void) {
  EGLint major, minor, n;
  GLint ret;

  egl.CreateImageKHR =
      (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  egl.DestroyImageKHR =
      (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  egl.EGLImageTargetTexture2DOES =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
          "glEGLImageTargetTexture2DOES");
  egl.CreateSyncKHR =
      (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
  egl.ClientWaitSyncKHR =
      (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
  if (!egl.CreateImageKHR || !egl.DestroyImageKHR ||
      !egl.EGLImageTargetTexture2DOES) {
    printf("eglGetProcAddress returned NULL for a required extension entry "
           "point.\n");
    return -1;
  }
  if (egl.CreateSyncKHR && egl.ClientWaitSyncKHR) {
    egl.egl_sync_supported = 1;
  } else {
    egl.egl_sync_supported = 0;
  }

  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                           EGL_NONE};

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                          EGL_NONE};

  egl.display = eglGetDisplay(gbm.dev);

  if (!eglInitialize(egl.display, &major, &minor)) {
    printf("failed to initialize\n");
    return -1;
  }

  printf("Using display %p with EGL version %d.%d\n", egl.display, major,
         minor);

  printf("EGL Version \"%s\"\n", eglQueryString(egl.display, EGL_VERSION));
  printf("EGL Vendor \"%s\"\n", eglQueryString(egl.display, EGL_VENDOR));

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    printf("failed to bind api EGL_OPENGL_ES_API\n");
    return -1;
  }

  if (!eglChooseConfig(egl.display, config_attribs, &egl.config, 1, &n) ||
      n != 1) {
    printf("failed to choose config: %d\n", n);
    return -1;
  }

  egl.context = eglCreateContext(egl.display, egl.config, EGL_NO_CONTEXT,
                                 context_attribs);
  if (egl.context == NULL) {
    printf("failed to create context\n");
    return -1;
  }
  /* connect the context to the surface */
  if (!eglMakeCurrent(egl.display, EGL_NO_SURFACE /* no default draw surface */,
                      EGL_NO_SURFACE /* no default draw read */, egl.context)) {
    printf("failed to make the OpenGL ES Context current: %s\n",
           get_egl_error());
    return -1;
  }

  const char *egl_extensions = eglQueryString(egl.display, EGL_EXTENSIONS);
  printf("EGL Extensions \"%s\"\n", egl_extensions);
  if (has_extension("EGL_KHR_image_base", egl_extensions)) {
    printf("EGL_KHR_image_base extension not supported\n");
    return -1;
  }
  if (has_extension("EGL_EXT_image_dma_buf_import", egl_extensions)) {
    printf("EGL_EXT_image_dma_buf_import extension not supported\n");
    return -1;
  }

  const char *gl_extensions = (const char *)glGetString(GL_EXTENSIONS);
  if (has_extension("GL_OES_EGL_image", gl_extensions)) {
    printf("GL_OES_EGL_image extension not supported\n");
    return -1;
  }

  return 0;
}

static int create_framebuffers(void) {
  uint32_t width = drm.mode->hdisplay;
  uint32_t height = drm.mode->vdisplay;
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    fbs[i].bo = gbm_bo_create(gbm.dev, width, height, GBM_FORMAT_XRGB8888,
                              GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!fbs[i].bo) {
      printf("failed to create a gbm buffer.\n");
      return -1;
    }

    fbs[i].fd = gbm_bo_get_fd(fbs[i].bo);
    if (fbs[i].fd < 0) {
      printf("failed to get fb for bo: %d", fbs[i].fd);
      return -1;
    }

    uint32_t handle = gbm_bo_get_handle(fbs[i].bo).u32;
    uint32_t stride = gbm_bo_get_stride(fbs[i].bo);
    uint32_t offset = 0;
    drmModeAddFB2(drm.fd, width, height, GBM_FORMAT_XRGB8888, &handle, &stride,
                  &offset, &fbs[i].fb_id, 0);
    if (!fbs[i].fb_id) {
      printf("failed to create framebuffer from buffer object.\n");
      return -1;
    }

    const EGLint khr_image_attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                                      fbs[i].fd,
                                      EGL_WIDTH,
                                      width,
                                      EGL_HEIGHT,
                                      height,
                                      EGL_LINUX_DRM_FOURCC_EXT,
                                      GBM_FORMAT_XRGB8888,
                                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                      stride,
                                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                      offset,
                                      EGL_NONE};

    fbs[i].image =
        egl.CreateImageKHR(egl.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                           NULL /* no client buffer */, khr_image_attrs);
    if (fbs[i].image == EGL_NO_IMAGE_KHR) {
      printf("failed to make image from buffer object: %s\n", get_egl_error());
      return -1;
    }
  }
  return 0;
}

static int init_gl(void) {
  GLuint vertex_shader, fragment_shader;
  GLint ret;

  static const char *vertex_shader_source =
    "attribute vec4 pos;\n"
    "void main() {\n"
    "  gl_Position = pos;\n"
    "}\n";

  static const char *fragment_shader_source =
    "precision mediump float;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "}\n";

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
  if (!ret) {
    char *log;

    printf("vertex shader compilation failed!:\n");
    glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &ret);
    if (ret > 1) {
      log = malloc(ret);
      glGetShaderInfoLog(vertex_shader, ret, NULL, log);
      printf("%s", log);
    }

    return -1;
  }

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
  if (!ret) {
    char *log;

    printf("fragment shader compilation failed!:\n");
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &ret);

    if (ret > 1) {
      log = malloc(ret);
      glGetShaderInfoLog(fragment_shader, ret, NULL, log);
      printf("%s", log);
    }

    return -1;
  }

  gl.program = glCreateProgram();

  glAttachShader(gl.program, vertex_shader);
  glAttachShader(gl.program, fragment_shader);
  glLinkProgram(gl.program);

  glGetProgramiv(gl.program, GL_LINK_STATUS, &ret);
  if (!ret) {
    char *log;

    printf("program linking failed!:\n");
    glGetProgramiv(gl.program, GL_INFO_LOG_LENGTH, &ret);

    if (ret > 1) {
      log = malloc(ret);
      glGetProgramInfoLog(gl.program, ret, NULL, log);
      printf("%s", log);
    }

    return -1;
  }

  glUseProgram(gl.program);
  glViewport(0, 0, drm.mode->hdisplay, drm.mode->vdisplay);

  return 0;
}

static int create_gl_framebuffers(void) {
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    glGenTextures(1, &fbs[i].gl_tex);
    glBindTexture(GL_TEXTURE_2D, fbs[i].gl_tex);
    egl.EGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)fbs[i].image);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fbs[i].gl_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fbs[i].gl_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fbs[i].gl_tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      printf("failed framebuffer check for created target buffer: %s\n",
             get_gl_framebuffer_error());
      glDeleteFramebuffers(1, &fbs[i].gl_fb);
      glDeleteTextures(1, &fbs[i].gl_tex);
      return -1;
    }
  }
  return 0;
}

static void draw(uint32_t i) {
  static const GLfloat verts[] = {0.0f, 0.5f, 0.0f,  -0.5, -0.5f,
                                  0.0f, 0.5f, -0.5f, 0.0f};

  /* clear the color buffer */
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, verts);
  glEnableVertexAttribArray(0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void egl_sync_fence() {
  if (egl.egl_sync_supported) {
    EGLSyncKHR sync = egl.CreateSyncKHR(egl.display, EGL_SYNC_FENCE_KHR, NULL);
    glFlush();
    egl.ClientWaitSyncKHR(egl.display, sync, 0, EGL_FOREVER_KHR);
  } else {
    glFinish();
  }
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data) {
  struct drm_fb *fb = data;
  struct gbm_device *gbm = gbm_bo_get_device(bo);

  if (fb->fb_id)
    drmModeRmFB(drm.fd, fb->fb_id);

  free(fb);
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec,
                              unsigned int usec, void *data) {
  int *waiting_for_flip = data;
  *waiting_for_flip = 0;
}

int main(int argc, char *argv[]) {
  const char *card;
  fd_set fds;
  drmEventContext evctx = {
      .version = DRM_EVENT_CONTEXT_VERSION,
      .page_flip_handler = page_flip_handler,
  };
  struct gbm_bo *bo;
  struct drm_fb *fb;
  uint32_t i = 0;
  int ret;

  /* check which DRM device to open */
  if (argc > 1)
    card = argv[1];
  else
    card = "/dev/dri/card0";

  ret = init_drm(card);
  if (ret) {
    printf("failed to initialize DRM\n");
    return ret;
  }

  FD_ZERO(&fds);
  FD_SET(0, &fds);
  FD_SET(drm.fd, &fds);

  ret = init_egl();
  if (ret) {
    printf("failed to initialize EGL\n");
    return ret;
  }

  ret = init_gbm();
  if (ret) {
    printf("failed to initialize GBM\n");
    return ret;
  }

  ret = create_framebuffers();
  if (ret) {
    printf("failed to create framebuffers\n");
    return ret;
  }

  ret = init_gl();
  if (ret) {
    printf("failed to initialize EGL\n");
    return ret;
  }

  ret = create_gl_framebuffers();
  if (ret) {
    printf("failed to initialize EGL\n");
    return ret;
  }

  /* clear the color buffer */
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  egl_sync_fence();

  gbm.front_buffer = 0;
  const struct framebuffer *back_fb = &fbs[gbm.front_buffer ^ 1];

  /* set mode: */
  ret = drmModeSetCrtc(drm.fd, drm.crtc_id, back_fb->fb_id, 0, 0,
                       &drm.connector_id, 1, drm.mode);
  if (ret) {
    printf("failed to set mode: %s\n", strerror(errno));
    return ret;
  }

  gbm.front_buffer ^= 1;
  int got_user_input = 0;
  struct timeval lasttime;
  gettimeofday(&lasttime, NULL);
  int num_frames = 0;
  const int one_sec = 1000000;
  while (1) {
    int waiting_for_flip = 1;

    const struct framebuffer *back_fb = &fbs[gbm.front_buffer ^ 1];
    glBindFramebuffer(GL_FRAMEBUFFER, back_fb->gl_fb);
    draw(i++);
    egl_sync_fence();

    /*
     * Here you could also update drm plane layers if you want
     * hw composition
     */

    ret = drmModePageFlip(drm.fd, drm.crtc_id, back_fb->fb_id,
                          DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
    if (ret) {
      printf("failed to queue page flip: %s\n", strerror(errno));
      return -1;
    }

    while (waiting_for_flip) {
      ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
      if (ret < 0) {
        printf("select err: %s\n", strerror(errno));
        return ret;
      } else if (ret == 0) {
        printf("select timeout!\n");
        return -1;
      } else if (FD_ISSET(0, &fds)) {
        printf("exit due to user-input\n");
        got_user_input = 1;
        break;
      } else if (FD_ISSET(drm.fd, &fds)) {
        drmHandleEvent(drm.fd, &evctx);
      }
    }

    if (got_user_input) {
      break;
    }

    gbm.front_buffer ^= 1;
    num_frames++;
    struct timeval currenttime;
    gettimeofday(&currenttime, NULL);
    long elapsed = (currenttime.tv_sec - lasttime.tv_sec) * one_sec +
                   currenttime.tv_usec - lasttime.tv_usec;
    if (elapsed > one_sec) {
      printf("FPS: %4f \n", num_frames / ((double)elapsed / one_sec));
      num_frames = 0;
      lasttime = currenttime;
    }
  }

  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    glDeleteFramebuffers(1, &fbs[i].gl_fb);
    glDeleteTextures(1, &fbs[i].gl_tex);
    egl.DestroyImageKHR(egl.display, fbs[i].image);
    drmModeRmFB(drm.fd, fbs[i].fb_id);
    close(fbs[i].fd);
    gbm_bo_destroy(fbs[i].bo);
  }

  eglDestroyContext(egl.display, egl.context);
  eglTerminate(egl.display);

  return ret;
}
