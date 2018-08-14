/*
 * Copyright (c) 2016 Dongseong Hwang <dongseong.hwang@intel.com>
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

#include "egl_drm_glue.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cassert>
#include <cstring>
#include <iostream>

#include "drm_modesetter.h"

// double buffering
#define NUM_BUFFERS 2

namespace ged {
namespace {

struct EGLGlue {
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;

  // Names are the original gl/egl function names with the prefix chopped off.
  PFNEGLCREATEIMAGEKHRPROC CreateImageKHR;
  PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC EGLImageTargetTexture2DOES;
  PFNEGLCREATESYNCKHRPROC CreateSyncKHR;
  PFNEGLCLIENTWAITSYNCKHRPROC ClientWaitSyncKHR;
  bool egl_sync_supported;
};

const char* EglGetError() {
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

class StreamTextureImpl : public StreamTexture {
 public:
  static std::unique_ptr<StreamTexture> Create(struct gbm_device* gbm,
                                               const EGLGlue& egl,
                                               size_t width,
                                               size_t height) {
    std::unique_ptr<StreamTextureImpl> texture(
        new StreamTextureImpl(egl, width, height));
    if (texture->Initialize(gbm))
      return std::move(texture);
    return nullptr;
  }

  ~StreamTextureImpl() override {
    glDeleteTextures(1, &gl_tex_);
    egl_->DestroyImageKHR(egl_->display, image_);
    close(fd_);
    gbm_bo_destroy(bo_);
  }

  void* Map() final {
    assert(addr_ == nullptr);
    size_t size = dimension_.stride * dimension_.height;
    addr_ = mmap(nullptr, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd_, 0);
    if (addr_ == MAP_FAILED)
      return nullptr;
    return addr_;
  }

  void Unmap() final {
    assert(addr_ != nullptr);
    size_t size = dimension_.stride * dimension_.height;
    munmap(addr_, size);
    addr_ = nullptr;
  }

  GLuint GetTextureID() const final { return gl_tex_; }
  Dimension GetDimension() const final { return dimension_; }

 private:
  StreamTextureImpl(const EGLGlue& egl, size_t width, size_t height)
      : egl_(&egl), dimension_() {
    dimension_.width = width;
    dimension_.height = height;
  }

  bool Initialize(struct gbm_device* gbm) {
    bo_ = gbm_bo_create(gbm, dimension_.width, dimension_.height,
                        GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR);
    if (!bo_) {
      fprintf(stderr, "failed to create a gbm buffer.\n");
      return false;
    }

    fd_ = gbm_bo_get_fd(bo_);
    if (fd_ < 0) {
      fprintf(stderr, "failed to get fb for bo: %d", fd_);
      return false;
    }

    dimension_.stride = gbm_bo_get_stride(bo_);
    EGLint offset = 0;
    const EGLint khr_image_attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                                      fd_,
                                      EGL_WIDTH,
                                      dimension_.width,
                                      EGL_HEIGHT,
                                      dimension_.height,
                                      EGL_LINUX_DRM_FOURCC_EXT,
                                      GBM_FORMAT_ARGB8888,
                                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                      dimension_.stride,
                                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                      offset,
                                      EGL_NONE};

    image_ = egl_->CreateImageKHR(
        egl_->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
        nullptr /* no client buffer */, khr_image_attrs);
    if (image_ == EGL_NO_IMAGE_KHR) {
      fprintf(stderr, "failed to make image from buffer object: %s\n",
              EglGetError());
      return false;
    }

    glGenTextures(1, &gl_tex_);
    glBindTexture(GL_TEXTURE_2D, gl_tex_);
    egl_->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
  }

  const EGLGlue* const egl_;
  struct gbm_bo* bo_ = nullptr;
  int fd_ = -1;
  EGLImageKHR image_ = nullptr;
  GLuint gl_tex_ = 0;
  Dimension dimension_;
  void* addr_ = nullptr;
};

}  // namespace

class EGLDRMGlue::Impl : public DRMModesetter::Client {
 public:
  Impl(std::unique_ptr<DRMModesetter> drm, const SwapBuffersCallback& callback)
      : drm_(std::move(drm)), callback_(callback), egl_({}) {
    drm_->SetClient(this);
  }
  Impl(const Impl&) = delete;
  void operator=(const Impl&) = delete;

  ~Impl() override {
    /* destroy framebuffers */
    for (auto& framebuffer : framebuffers_) {
      glDeleteFramebuffers(1, &framebuffer.gl_fb);
      glDeleteTextures(1, &framebuffer.gl_tex);
      egl_.DestroyImageKHR(egl_.display, framebuffer.image);
      drmModeRmFB(drm_->GetFD(), framebuffer.fb_id);
      close(framebuffer.fd);
      gbm_bo_destroy(framebuffer.bo);
    }

    eglDestroyContext(egl_.display, egl_.context);
    eglTerminate(egl_.display);

    gbm_device_destroy(gbm_);
  }

  bool Initialize() {
    gbm_ = gbm_create_device(drm_->GetFD());
    if (!gbm_) {
      fprintf(stderr, "cannot create gbm device.\n");
      return false;
    }

    if (!InitializeEGL()) {
      fprintf(stderr, "cannot create EGL context.\n");
      return false;
    }

    DRMModesetter::Size display_size = drm_->GetDisplaySize();
    for (auto& framebuffer : framebuffers_) {
      if (!CreateFramebuffer(display_size.width, display_size.height,
                             framebuffer)) {
        fprintf(stderr, "cannot create framebuffer.\n");
        return false;
      }
    }

    // Need to do the first mode setting before page flip.
    if (!drm_->ModeSetCrtc())
      return false;

    return true;
  }

  bool Run() { return drm_->Run(); }

  Size GetDisplaySize() const {
    DRMModesetter::Size display_size = drm_->GetDisplaySize();
    return {display_size.width, display_size.height};
  }

  std::unique_ptr<StreamTexture> CreateStreamTexture(size_t width,
                                                     size_t height) {
    return StreamTextureImpl::Create(gbm_, egl_, width, height);
  }

 private:
  bool InitializeEGL() {
    egl_.CreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    egl_.DestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    egl_.EGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
            "glEGLImageTargetTexture2DOES");
    egl_.CreateSyncKHR =
        (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    egl_.ClientWaitSyncKHR =
        (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
    if (!egl_.CreateImageKHR || !egl_.DestroyImageKHR ||
        !egl_.EGLImageTargetTexture2DOES) {
      fprintf(
          stderr,
          "eglGetProcAddress returned nullptr for a required extension entry "
          "point.\n");
      return false;
    }
    if (egl_.CreateSyncKHR && egl_.ClientWaitSyncKHR) {
      egl_.egl_sync_supported = true;
    } else {
      egl_.egl_sync_supported = false;
    }

    egl_.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint major, minor = 0;
    if (!eglInitialize(egl_.display, &major, &minor)) {
      fprintf(stderr, "failed to initialize\n");
      return false;
    }

    printf("Using display %p with EGL version %d.%d\n", egl_.display, major,
           minor);

    printf("EGL Version \"%s\"\n", eglQueryString(egl_.display, EGL_VERSION));
    printf("EGL Vendor \"%s\"\n", eglQueryString(egl_.display, EGL_VENDOR));

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
      fprintf(stderr, "failed to bind api EGL_OPENGL_ES_API\n");
      return false;
    }

    static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                            EGL_NONE};
    EGLint num_config = 0;
    if (!eglChooseConfig(egl_.display, config_attribs, &egl_.config, 1,
                         &num_config) ||
        num_config != 1) {
      fprintf(stderr, "failed to choose config: %d\n", num_config);
      return false;
    }

    static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                             EGL_NONE};
    egl_.context = eglCreateContext(egl_.display, egl_.config, EGL_NO_CONTEXT,
                                    context_attribs);
    if (egl_.context == nullptr) {
      fprintf(stderr, "failed to create context\n");
      return false;
    }
    /* connect the context to the surface */
    if (!eglMakeCurrent(
            egl_.display, EGL_NO_SURFACE /* no default draw surface */,
            EGL_NO_SURFACE /* no default draw read */, egl_.context)) {
      fprintf(stderr, "failed to make the OpenGL ES Context current: %s\n",
              EglGetError());
      return false;
    }

    const char* egl_extensions = eglQueryString(egl_.display, EGL_EXTENSIONS);
    printf("EGL Extensions \"%s\"\n", egl_extensions);
    if (!ExtensionsContain("EGL_KHR_image_base", egl_extensions)) {
      fprintf(stderr, "EGL_KHR_image_base extension not supported\n");
      return false;
    }
    if (!ExtensionsContain("EGL_EXT_image_dma_buf_import", egl_extensions)) {
      fprintf(stderr, "EGL_EXT_image_dma_buf_import extension not supported\n");
      return false;
    }

    const char* gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!ExtensionsContain("GL_OES_EGL_image", gl_extensions)) {
      fprintf(stderr, "GL_OES_EGL_image extension not supported\n");
      return false;
    }

    return true;
  }

  void EGLSyncFence() {
    if (egl_.egl_sync_supported) {
      EGLSyncKHR sync =
          egl_.CreateSyncKHR(egl_.display, EGL_SYNC_FENCE_KHR, nullptr);
      glFlush();
      egl_.ClientWaitSyncKHR(egl_.display, sync, 0, EGL_FOREVER_KHR);
    } else {
      glFinish();
    }
  }

  bool ExtensionsContain(const char* name, const char* c_extensions) {
    assert(name);
    if (!c_extensions)
      return false;
    std::string extensions(c_extensions);
    extensions += " ";

    std::string delimited_name(name);
    delimited_name += " ";

    return extensions.find(delimited_name) != std::string::npos;
  }

  struct Framebuffer {
    struct gbm_bo* bo = nullptr;
    int fd;
    uint32_t fb_id;
    EGLImageKHR image;
    GLuint gl_tex;
    GLuint gl_fb;
  };

  bool CreateFramebuffer(size_t width,
                         size_t height,
                         Framebuffer& framebuffer) {
    framebuffer.bo = gbm_bo_create(gbm_, width, height, GBM_FORMAT_XRGB8888,
                                   GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!framebuffer.bo) {
      fprintf(stderr, "failed to create a gbm buffer.\n");
      return false;
    }

    framebuffer.fd = gbm_bo_get_fd(framebuffer.bo);
    if (framebuffer.fd < 0) {
      fprintf(stderr, "failed to get fb for bo: %d", framebuffer.fd);
      return false;
    }

    uint32_t handle = gbm_bo_get_handle(framebuffer.bo).u32;
    uint32_t stride = gbm_bo_get_stride(framebuffer.bo);
    uint32_t offset = 0;
    drmModeAddFB2(drm_->GetFD(), width, height, GBM_FORMAT_XRGB8888, &handle,
                  &stride, &offset, &framebuffer.fb_id, 0);
    if (!framebuffer.fb_id) {
      fprintf(stderr, "failed to create framebuffer from buffer object.\n");
      return false;
    }

    const EGLint khr_image_attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                                      framebuffer.fd,
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

    framebuffer.image =
        egl_.CreateImageKHR(egl_.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                            nullptr /* no client buffer */, khr_image_attrs);
    if (framebuffer.image == EGL_NO_IMAGE_KHR) {
      fprintf(stderr, "failed to make image from buffer object: %s\n",
              EglGetError());
      return false;
    }

    glGenTextures(1, &framebuffer.gl_tex);
    glBindTexture(GL_TEXTURE_2D, framebuffer.gl_tex);
    egl_.EGLImageTargetTexture2DOES(GL_TEXTURE_2D, framebuffer.image);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &framebuffer.gl_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.gl_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           framebuffer.gl_tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr,
              "failed framebuffer check for created target buffer: %x\n",
              glCheckFramebufferStatus(GL_FRAMEBUFFER));
      glDeleteFramebuffers(1, &framebuffer.gl_fb);
      glDeleteTextures(1, &framebuffer.gl_tex);
      return false;
    }

    return true;
  }

  // As soon as page flip, notify the client to draw the next frame.
  void DidPageFlip(int front_buffer,
                   unsigned int sec,
                   unsigned int usec) override {
    const Framebuffer& back_fb = framebuffers_[front_buffer ^ 1];

    glBindFramebuffer(GL_FRAMEBUFFER, back_fb.gl_fb);
    callback_(back_fb.gl_fb, sec * 1000000 + usec);
    EGLSyncFence();
  }

  uint32_t GetFrameBuffer(int front_buffer) const override {
    return framebuffers_[front_buffer].fb_id;
  }

  std::unique_ptr<ged::DRMModesetter> drm_;
  SwapBuffersCallback callback_;

  struct gbm_device* gbm_ = nullptr;

  EGLGlue egl_;
  Framebuffer framebuffers_[NUM_BUFFERS];
};

// static
std::unique_ptr<EGLDRMGlue> EGLDRMGlue::Create(
    std::unique_ptr<DRMModesetter> drm,
    const SwapBuffersCallback& callback) {
  std::unique_ptr<EGLDRMGlue> egl(new EGLDRMGlue());
  if (egl->Initialize(std::move(drm), callback))
    return egl;
  return nullptr;
}

EGLDRMGlue::EGLDRMGlue() {}

EGLDRMGlue::~EGLDRMGlue() {}

bool EGLDRMGlue::Initialize(std::unique_ptr<DRMModesetter> drm,
                            const SwapBuffersCallback& callback) {
  impl_.reset(new Impl(std::move(drm), callback));
  return impl_->Initialize();
}

EGLDRMGlue::Size EGLDRMGlue::GetDisplaySize() const {
  return impl_->GetDisplaySize();
}

std::unique_ptr<StreamTexture> EGLDRMGlue::CreateStreamTexture(size_t width,
                                                               size_t height) {
  return impl_->CreateStreamTexture(width, height);
}

bool EGLDRMGlue::Run() {
  return impl_->Run();
}

}  // namespace ged
