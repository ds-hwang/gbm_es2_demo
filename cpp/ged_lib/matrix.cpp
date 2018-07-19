/*
 * (c) 2009 Aaftab Munshi, Dan Ginsburg, Dave Shreiner
 * Copyright (c) 2016 Dongseong Hwang <dongseong.hwang@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//
// ESUtil.c
//
//    A utility library for OpenGL ES.  This library provides a
//    basic common framework for the example applications in the
//    OpenGL ES 2.0 Programming Guide.
//

#include "matrix.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ged {

Matrix::Matrix() {
  InitIdentity();
}

Matrix::~Matrix() {}

void Matrix::operator=(const Matrix& other) {
  std::copy(&other.m_[0][0], &other.m_[3][3] + 1, &m_[0][0]);
}

const float* Matrix::Data() const {
  return &m_[0][0];
}

void Matrix::Get3x3(float* m3x3) const {
  m3x3[0] = m_[0][0];
  m3x3[1] = m_[0][1];
  m3x3[2] = m_[0][2];
  m3x3[3] = m_[1][0];
  m3x3[4] = m_[1][1];
  m3x3[5] = m_[1][2];
  m3x3[6] = m_[2][0];
  m3x3[7] = m_[2][1];
  m3x3[8] = m_[2][2];
}

void Matrix::InitIdentity() {
  std::memset(m_, 0, sizeof m_);
  m_[0][0] = 1.0f;
  m_[1][1] = 1.0f;
  m_[2][2] = 1.0f;
  m_[3][3] = 1.0f;
}

void Matrix::MatrixMultiply(const Matrix& op) {
  Matrix tmp;
  for (int i = 0; i < 4; i++) {
    tmp.m_[i][0] = (m_[i][0] * op.m_[0][0]) + (m_[i][1] * op.m_[1][0]) +
                   (m_[i][2] * op.m_[2][0]) + (m_[i][3] * op.m_[3][0]);

    tmp.m_[i][1] = (m_[i][0] * op.m_[0][1]) + (m_[i][1] * op.m_[1][1]) +
                   (m_[i][2] * op.m_[2][1]) + (m_[i][3] * op.m_[3][1]);

    tmp.m_[i][2] = (m_[i][0] * op.m_[0][2]) + (m_[i][1] * op.m_[1][2]) +
                   (m_[i][2] * op.m_[2][2]) + (m_[i][3] * op.m_[3][2]);

    tmp.m_[i][3] = (m_[i][0] * op.m_[0][3]) + (m_[i][1] * op.m_[1][3]) +
                   (m_[i][2] * op.m_[2][3]) + (m_[i][3] * op.m_[3][3]);
  }
  std::copy(&tmp.m_[0][0], &tmp.m_[3][3] + 1, &m_[0][0]);
}

void Matrix::Scale(float sx, float sy, float sz) {
  m_[0][0] *= sx;
  m_[0][1] *= sx;
  m_[0][2] *= sx;
  m_[0][3] *= sx;

  m_[1][0] *= sy;
  m_[1][1] *= sy;
  m_[1][2] *= sy;
  m_[1][3] *= sy;

  m_[2][0] *= sz;
  m_[2][1] *= sz;
  m_[2][2] *= sz;
  m_[2][3] *= sz;
}

void Matrix::Translate(float tx, float ty, float tz) {
  m_[3][0] += (m_[0][0] * tx + m_[1][0] * ty + m_[2][0] * tz);
  m_[3][1] += (m_[0][1] * tx + m_[1][1] * ty + m_[2][1] * tz);
  m_[3][2] += (m_[0][2] * tx + m_[1][2] * ty + m_[2][2] * tz);
  m_[3][3] += (m_[0][3] * tx + m_[1][3] * ty + m_[2][3] * tz);
}

void Matrix::Rotate(float angle, float x, float y, float z) {
  float mag = sqrt(x * x + y * y + z * z);
  if (mag > 0.0f) {
    float xx, yy, zz, xy, yz, zx, xs, ys, zs;
    Matrix rot_mat;

    x /= mag;
    y /= mag;
    z /= mag;

    xx = x * x;
    yy = y * y;
    zz = z * z;
    xy = x * y;
    yz = y * z;
    zx = z * x;
    float sin_angle = sin(angle * M_PI / 180.0f);
    float cos_angle = cos(angle * M_PI / 180.0f);
    xs = x * sin_angle;
    ys = y * sin_angle;
    zs = z * sin_angle;
    float one_cos = 1.0f - cos_angle;

    rot_mat.m_[0][0] = (one_cos * xx) + cos_angle;
    rot_mat.m_[0][1] = (one_cos * xy) - zs;
    rot_mat.m_[0][2] = (one_cos * zx) + ys;
    rot_mat.m_[0][3] = 0.0F;

    rot_mat.m_[1][0] = (one_cos * xy) + zs;
    rot_mat.m_[1][1] = (one_cos * yy) + cos_angle;
    rot_mat.m_[1][2] = (one_cos * yz) - xs;
    rot_mat.m_[1][3] = 0.0F;

    rot_mat.m_[2][0] = (one_cos * zx) - ys;
    rot_mat.m_[2][1] = (one_cos * yz) + xs;
    rot_mat.m_[2][2] = (one_cos * zz) + cos_angle;
    rot_mat.m_[2][3] = 0.0F;

    rot_mat.m_[3][0] = 0.0F;
    rot_mat.m_[3][1] = 0.0F;
    rot_mat.m_[3][2] = 0.0F;
    rot_mat.m_[3][3] = 1.0F;

    rot_mat.MatrixMultiply(*this);
    *this = rot_mat;
  }
}

void Matrix::Frustum(float left,
                     float right,
                     float bottom,
                     float top,
                     float nearZ,
                     float farZ) {
  float deltaX = right - left;
  float deltaY = top - bottom;
  float deltaZ = farZ - nearZ;
  if ((nearZ <= 0.0f) || (farZ <= 0.0f) || (deltaX <= 0.0f) ||
      (deltaY <= 0.0f) || (deltaZ <= 0.0f))
    return;

  Matrix frust;
  frust.m_[0][0] = 2.0f * nearZ / deltaX;
  frust.m_[0][1] = frust.m_[0][2] = frust.m_[0][3] = 0.0f;

  frust.m_[1][1] = 2.0f * nearZ / deltaY;
  frust.m_[1][0] = frust.m_[1][2] = frust.m_[1][3] = 0.0f;

  frust.m_[2][0] = (right + left) / deltaX;
  frust.m_[2][1] = (top + bottom) / deltaY;
  frust.m_[2][2] = -(nearZ + farZ) / deltaZ;
  frust.m_[2][3] = -1.0f;

  frust.m_[3][2] = -2.0f * nearZ * farZ / deltaZ;
  frust.m_[3][0] = frust.m_[3][1] = frust.m_[3][3] = 0.0f;

  frust.MatrixMultiply(*this);
  *this = frust;
}

void Matrix::Perspective(float fovy, float aspect, float nearZ, float farZ) {
  float frustumH = tanf(fovy / 360.0f * M_PI) * nearZ;
  float frustumW = frustumH * aspect;
  Frustum(-frustumW, frustumW, -frustumH, frustumH, nearZ, farZ);
}

}  // namespace ged
