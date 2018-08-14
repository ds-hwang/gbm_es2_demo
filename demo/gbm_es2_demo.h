/*
 * Copyright (c) 2018 Dongseong Hwang <dongseong.hwang@intel.com>
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

#ifndef GBM_ES2_DEMO_H_

#include <GLES2/gl2.h>
#include <string>

#include "egl_drm_glue.h"

namespace demo {

class ES2Cube {
 public:
  ES2Cube() = default;
  virtual ~ES2Cube() = default;
  ES2Cube(const ES2Cube&) = delete;
  void operator=(const ES2Cube&) = delete;

  virtual bool Initialize(std::string card) = 0;
  virtual bool Run() = 0;
};

class ES2CubeImpl : public ES2Cube {
 public:
  ES2CubeImpl() = default;
  ~ES2CubeImpl() override;
  bool Initialize(std::string card) override;
  bool Run() override;

 private:
  bool InitializeGL();
  bool InitializeGLProgram();
  void DidSwapBuffer(GLuint gl_framebuffer, unsigned long usec);
  void Draw(unsigned long usec);

  std::unique_ptr<ged::EGLDRMGlue> egl_;
  ged::EGLDRMGlue::Size display_size_ = {};
  GLuint program_ = 0;
  GLint modelviewmatrix_ = 0;
  GLint modelviewprojectionmatrix_ = 0;
  GLint normalmatrix_ = 0;
  GLuint vbo_ = 0;
};

class ES2CubeMapImpl : public ES2Cube {
 public:
  ES2CubeMapImpl() = default;
  ~ES2CubeMapImpl() override;

  bool Initialize(std::string card) override;
  bool Run() override;

 private:
  bool InitializeGL();
  bool InitializeGLProgram();
  void DidSwapBuffer(GLuint gl_framebuffer, unsigned long usec);
  void Draw(unsigned long usec);
  void UpdateStreamTexture(unsigned long usec);

  std::unique_ptr<ged::EGLDRMGlue> egl_;
  ged::EGLDRMGlue::Size display_size_ = {};
  GLuint program_ = 0;
  GLint modelviewmatrix_ = 0;
  GLint modelviewprojectionmatrix_ = 0;
  GLint normalmatrix_ = 0;
  GLuint vbo_ = 0;
  static const size_t s_length = 512;
  std::unique_ptr<ged::StreamTexture> stream_texture_;

  // For check pattern.
  float last_progress_ = 0.f;
  bool even_turn_ = true;
};

}  // namespace demo

#endif
