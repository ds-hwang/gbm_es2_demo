/*
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

#ifndef GED_MATRIX_H
#define GED_MATRIX_H

namespace ged {

class Matrix {
 public:
  Matrix();
  ~Matrix();
  Matrix(const Matrix&) = default;
  void operator=(const Matrix&);

  const float* Data() const;
  void Get3x3(float* m3x3) const;

  void MatrixMultiply(const Matrix& op);
  void Scale(float sx, float sy, float sz);
  void Translate(float tx, float ty, float tz);
  void Rotate(float angle, float x, float y, float z);

  // \brief multiply matrix specified by result with a perspective matrix and
  // return new matrix in result
  /// \param result Specifies the input matrix.  new matrix is returned in
  /// result.
  /// \param left, right Coordinates for the left and right vertical clipping
  /// planes
  /// \param bottom, top Coordinates for the bottom and top horizontal clipping
  /// planes
  /// \param nearZ, farZ Distances to the near and far depth clipping planes.
  /// Both distances must be positive.
  void Frustum(float left,
               float right,
               float bottom,
               float top,
               float nearZ,
               float farZ);

  /// \brief multiply matrix specified by result with a perspective matrix and
  /// return new matrix in result
  /// \param result Specifies the input matrix.  new matrix is returned in
  /// result.
  /// \param fovy Field of view y angle in degrees
  /// \param aspect Aspect ratio of screen
  /// \param nearZ Near plane distance
  /// \param farZ Far plane distance
  void Perspective(float fovy, float aspect, float nearZ, float farZ);

 private:
  void InitIdentity();
  float m_[4][4];
};

}  // namespace ged

#endif  // GED_MATRIX_H
