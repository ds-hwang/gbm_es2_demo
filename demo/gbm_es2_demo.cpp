/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
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

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <cmath>
#include <memory>

#include "drm_modesetter.h"
#include "gbm_es2_demo.h"
#include "matrix.h"

namespace demo {

ES2CubeImpl::~ES2CubeImpl() {
  glDeleteBuffers(1, &vbo_);
  glDeleteProgram(program_);
}

bool ES2CubeImpl::Initialize(std::string card) {
  std::unique_ptr<ged::DRMModesetter> drm = ged::DRMModesetter::Create(card);
  if (!drm) {
    fprintf(stderr, "failed to create DRMModesetter.\n");
    return false;
  }

  egl_ = ged::EGLDRMGlue::Create(
      std::move(drm), std::bind(&ES2CubeImpl::DidSwapBuffer, this,
                                std::placeholders::_1, std::placeholders::_2));
  if (!egl_) {
    fprintf(stderr, "failed to create EGLDRMGlue.\n");
    return false;
  }

  display_size_ = egl_->GetDisplaySize();

  // Need to do the first mode setting before page flip.
  if (!InitializeGL())
    return false;
  return true;
}

bool ES2CubeImpl::Run() {
  return egl_->Run();
}

bool ES2CubeImpl::InitializeGL() {
  static const GLfloat vVertices[] = {
      // front
      -1.0f, -1.0f, +1.0f,  // point blue
      +1.0f, -1.0f, +1.0f,  // point magenta
      -1.0f, +1.0f, +1.0f,  // point cyan
      +1.0f, +1.0f, +1.0f,  // point white
      // back
      +1.0f, -1.0f, -1.0f,  // point red
      -1.0f, -1.0f, -1.0f,  // point black
      +1.0f, +1.0f, -1.0f,  // point yellow
      -1.0f, +1.0f, -1.0f,  // point green
      // right
      +1.0f, -1.0f, +1.0f,  // point magenta
      +1.0f, -1.0f, -1.0f,  // point red
      +1.0f, +1.0f, +1.0f,  // point white
      +1.0f, +1.0f, -1.0f,  // point yellow
      // left
      -1.0f, -1.0f, -1.0f,  // point black
      -1.0f, -1.0f, +1.0f,  // point blue
      -1.0f, +1.0f, -1.0f,  // point green
      -1.0f, +1.0f, +1.0f,  // point cyan
      // top
      -1.0f, +1.0f, +1.0f,  // point cyan
      +1.0f, +1.0f, +1.0f,  // point white
      -1.0f, +1.0f, -1.0f,  // point green
      +1.0f, +1.0f, -1.0f,  // point yellow
      // bottom
      -1.0f, -1.0f, -1.0f,  // point black
      +1.0f, -1.0f, -1.0f,  // point red
      -1.0f, -1.0f, +1.0f,  // point blue
      +1.0f, -1.0f, +1.0f   // point magenta
  };

  static const GLfloat vColors[] = {
      // front
      0.0f, 0.0f, 1.0f,  // blue
      1.0f, 0.0f, 1.0f,  // magenta
      0.0f, 1.0f, 1.0f,  // cyan
      1.0f, 1.0f, 1.0f,  // white
      // back
      1.0f, 0.0f, 0.0f,  // red
      0.0f, 0.0f, 0.0f,  // black
      1.0f, 1.0f, 0.0f,  // yellow
      0.0f, 1.0f, 0.0f,  // green
      // right
      1.0f, 0.0f, 1.0f,  // magenta
      1.0f, 0.0f, 0.0f,  // red
      1.0f, 1.0f, 1.0f,  // white
      1.0f, 1.0f, 0.0f,  // yellow
      // left
      0.0f, 0.0f, 0.0f,  // black
      0.0f, 0.0f, 1.0f,  // blue
      0.0f, 1.0f, 0.0f,  // green
      0.0f, 1.0f, 1.0f,  // cyan
      // top
      0.0f, 1.0f, 1.0f,  // cyan
      1.0f, 1.0f, 1.0f,  // white
      0.0f, 1.0f, 0.0f,  // green
      1.0f, 1.0f, 0.0f,  // yellow
      // bottom
      0.0f, 0.0f, 0.0f,  // black
      1.0f, 0.0f, 0.0f,  // red
      0.0f, 0.0f, 1.0f,  // blue
      1.0f, 0.0f, 1.0f   // magenta
  };

  static const GLfloat vNormals[] = {
      // front
      +0.0f, +0.0f, +1.0f,  // forward
      +0.0f, +0.0f, +1.0f,  // forward
      +0.0f, +0.0f, +1.0f,  // forward
      +0.0f, +0.0f, +1.0f,  // forward
      // back
      +0.0f, +0.0f, -1.0f,  // backbard
      +0.0f, +0.0f, -1.0f,  // backbard
      +0.0f, +0.0f, -1.0f,  // backbard
      +0.0f, +0.0f, -1.0f,  // backbard
      // right
      +1.0f, +0.0f, +0.0f,  // right
      +1.0f, +0.0f, +0.0f,  // right
      +1.0f, +0.0f, +0.0f,  // right
      +1.0f, +0.0f, +0.0f,  // right
      // left
      -1.0f, +0.0f, +0.0f,  // left
      -1.0f, +0.0f, +0.0f,  // left
      -1.0f, +0.0f, +0.0f,  // left
      -1.0f, +0.0f, +0.0f,  // left
      // top
      +0.0f, +1.0f, +0.0f,  // up
      +0.0f, +1.0f, +0.0f,  // up
      +0.0f, +1.0f, +0.0f,  // up
      +0.0f, +1.0f, +0.0f,  // up
      // bottom
      +0.0f, -1.0f, +0.0f,  // down
      +0.0f, -1.0f, +0.0f,  // down
      +0.0f, -1.0f, +0.0f,  // down
      +0.0f, -1.0f, +0.0f   // down
  };

  if (!InitializeGLProgram())
    return false;

  modelviewmatrix_ = glGetUniformLocation(program_, "modelviewMatrix");
  modelviewprojectionmatrix_ =
      glGetUniformLocation(program_, "modelviewprojectionMatrix");
  normalmatrix_ = glGetUniformLocation(program_, "normalMatrix");

  glViewport(0, 0, display_size_.width, display_size_.height);
  glEnable(GL_CULL_FACE);

  GLintptr positionsoffset = 0;
  GLintptr colorsoffset = sizeof(vVertices);
  GLintptr normalsoffset = sizeof(vVertices) + sizeof(vColors);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices),
                  &vVertices[0]);
  glBufferSubData(GL_ARRAY_BUFFER, colorsoffset, sizeof(vColors), &vColors[0]);
  glBufferSubData(GL_ARRAY_BUFFER, normalsoffset, sizeof(vNormals),
                  &vNormals[0]);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                        reinterpret_cast<const void*>(positionsoffset));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0,
                        reinterpret_cast<const void*>(normalsoffset));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0,
                        reinterpret_cast<const void*>(colorsoffset));
  glEnableVertexAttribArray(2);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  return true;
}

bool ES2CubeImpl::InitializeGLProgram() {
  static const char* vertex_shader_source =
      "uniform mat4 modelviewMatrix;      \n"
      "uniform mat4 modelviewprojectionMatrix;\n"
      "uniform mat3 normalMatrix;         \n"
      "                                   \n"
      "attribute vec4 in_position;        \n"
      "attribute vec3 in_normal;          \n"
      "attribute vec4 in_color;           \n"
      "\n"
      "vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    gl_Position = modelviewprojectionMatrix * in_position;\n"
      "    vec3 vEyeNormal = normalMatrix * in_normal;\n"
      "    vec4 vPosition4 = modelviewMatrix * in_position;\n"
      "    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
      "    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
      "    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
      "    vVaryingColor = vec4(diff * in_color.rgb, 1.0);\n"
      "}                                  \n";

  static const char* fragment_shader_source =
      "precision mediump float;           \n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    gl_FragColor = vVaryingColor;  \n"
      "}                                  \n";

  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  GLint ret = 0;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
  if (!ret) {
    printf("vertex shader compilation failed!:\n");
    glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &ret);
    if (ret > 1) {
      char* log = static_cast<char*>(malloc(ret));
      glGetShaderInfoLog(vertex_shader, ret, NULL, log);
      printf("%s\n", log);
      free(log);
    }
    return false;
  }

  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
  if (!ret) {
    printf("fragment shader compilation failed!:\n");
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &ret);
    if (ret > 1) {
      char* log = static_cast<char*>(malloc(ret));
      glGetShaderInfoLog(fragment_shader, ret, NULL, log);
      printf("%s\n", log);
      free(log);
    }
    return false;
  }

  program_ = glCreateProgram();

  glAttachShader(program_, vertex_shader);
  glAttachShader(program_, fragment_shader);

  glBindAttribLocation(program_, 0, "in_position");
  glBindAttribLocation(program_, 1, "in_normal");
  glBindAttribLocation(program_, 2, "in_color");

  glLinkProgram(program_);

  glGetProgramiv(program_, GL_LINK_STATUS, &ret);
  if (!ret) {
    printf("program linking failed!:\n");
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &ret);
    if (ret > 1) {
      char* log = static_cast<char*>(malloc(ret));
      glGetProgramInfoLog(program_, ret, NULL, log);
      printf("%s\n", log);
      free(log);
    }
    return false;
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glUseProgram(program_);
  return true;
}

void ES2CubeImpl::DidSwapBuffer(GLuint gl_framebuffer, unsigned long usec) {
  Draw(usec);

  static int num_frames = 0;
  static unsigned long lasttime = 0;
  static const size_t one_sec = 1000000;
  num_frames++;
  unsigned long elapsed = usec - lasttime;
  if (elapsed > one_sec) {
    printf("FPS: %4f \n", num_frames / ((double)elapsed / one_sec));
    num_frames = 0;
    lasttime = usec;
  }
}

void ES2CubeImpl::Draw(unsigned long usec) {
  // 100% every 10 sec
  static const int interval = 10000000.f;
  float progress = 1.f * (usec % interval) / interval;
  float red = pow(cos(M_PI * 2 * progress), 2) / 3;
  float green = pow(cos(M_PI * 2 * (progress + 0.33)), 2) / 3;
  float blue = pow(cos(M_PI * 2 * (progress + 0.66)), 2) / 3;
  glClearColor(red, green, blue, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // convert to 200ms precision, which covers 60FPS very enough.
  int i = usec / 5000;
  ged::Matrix modelview;
  modelview.Translate(0.0f, 0.0f, -8.0f);
  modelview.Rotate(45.0f + (0.25f * i), 1.0f, 0.0f, 0.0f);
  modelview.Rotate(45.0f - (0.5f * i), 0.0f, 1.0f, 0.0f);
  modelview.Rotate(10.0f + (0.15f * i), 0.0f, 0.0f, 1.0f);

  GLfloat aspect =
      (GLfloat)(display_size_.width) / (GLfloat)(display_size_.height);

  ged::Matrix projection;
  float field_of_view = 35.f;
  projection.Perspective(field_of_view, aspect, 6.f, 10.f);

  ged::Matrix modelviewprojection = modelview;
  modelviewprojection.MatrixMultiply(projection);

  glUniformMatrix4fv(modelviewmatrix_, 1, GL_FALSE, modelview.Data());
  glUniformMatrix4fv(modelviewprojectionmatrix_, 1, GL_FALSE,
                     modelviewprojection.Data());
  float normal[9] = {};
  modelview.Get3x3(&normal[0]);
  glUniformMatrix3fv(normalmatrix_, 1, GL_FALSE, normal);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
}

}  // namespace demo
