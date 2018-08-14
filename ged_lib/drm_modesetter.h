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

#ifndef GED_DRM_MODESETTER_H_
#define GED_DRM_MODESETTER_H_

#include <memory>
#include <string>

namespace ged {

/*
 * DRMModesetter abstracts the DRM modesetting API. It plays a role of
 * initializing DRM connection, crtc, and encoder. It provides API to handle
 * page-flips along with VBlank interval.
 */
class DRMModesetter {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    virtual void DidPageFlip(int front_buffer,
                             unsigned int sec,
                             unsigned int usec) = 0;
    virtual uint32_t GetFrameBuffer(int front_buffer) const = 0;
  };

  static std::unique_ptr<DRMModesetter> Create(const std::string& card,
                                               bool atomic);

  ~DRMModesetter();
  DRMModesetter(const DRMModesetter&) = delete;
  void operator=(const DRMModesetter&) = delete;

  void SetClient(Client* client);
  int GetFD() const;

  struct Size {
    int width;
    int height;
  };
  Size GetDisplaySize() const;

  bool ModeSetCrtc();
  bool PageFlip(uint32_t fb_id, void* user_data);
  bool Run();

 private:
  DRMModesetter();

  bool Initialize(const std::string& card);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ged

#endif  // GED_DRM_MODESETTER_H_
