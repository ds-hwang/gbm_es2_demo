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

#ifndef GED_EGL_DRM_GLUE_H_
#define GED_EGL_DRM_GLUE_H_

#include <memory>
#include <functional>

namespace ged {

class DRMModesetter;
typedef unsigned int GLuint;
typedef std::function<void(GLuint /* gl_framebuffer */,
                           unsigned long /* usec */)> SwapBuffersCallback;

class StreamTexture {
 public:
  virtual ~StreamTexture() = default;
  virtual void* Map() = 0;
  virtual void Unmap() = 0;
  virtual GLuint GetTextureID() const = 0;
  struct Dimension {
    size_t width = 0;
    size_t height = 0;
    size_t stride = 0;
  };
  virtual Dimension GetDimension() const = 0;
};

/*
 * EGLDRMGlue provides API to handle page-flips along with VBlank interval.
 */
class EGLDRMGlue {
 public:
  static std::unique_ptr<EGLDRMGlue> Create(
      std::unique_ptr<DRMModesetter> drm,
      const SwapBuffersCallback& callback);

  ~EGLDRMGlue();
  EGLDRMGlue(const EGLDRMGlue&) = delete;
  void operator=(const EGLDRMGlue&) = delete;

  struct Size {
    size_t width;
    size_t height;
  };
  Size GetDisplaySize() const;

  std::unique_ptr<StreamTexture> CreateStreamTexture(size_t width,
                                                     size_t height);

  bool Run();

  // It must be called inside Run() loop.
  void Destroy();

 private:
  EGLDRMGlue();

  bool Initialize(std::unique_ptr<DRMModesetter> drm,
                  const SwapBuffersCallback& callback);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ged

#endif  // GED_EGL_DRM_GLUE_H_
