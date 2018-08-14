/*
 * Written 2012 by David Herrmann <dh.herrmann@googlemail.com>
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

// Based on modeset - DRM Double-Buffered VSync'ed Modesetting Example

#include "drm_modesetter.h"

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <list>

namespace ged {

class DRMModesetter::Impl {
 public:
  Impl() {}
  Impl(const Impl&) = delete;
  void operator=(const Impl&) = delete;

  ~Impl() {
    assert(!page_flip_pending_);
    for (auto& dev : modeset_dev_list_) {
      /* restore saved CRTC configuration */
      drmModeSetCrtc(fd_, dev->saved_crtc->crtc_id, dev->saved_crtc->buffer_id,
                     dev->saved_crtc->x, dev->saved_crtc->y, &dev->conn, 1,
                     &dev->saved_crtc->mode);
      drmModeFreeCrtc(dev->saved_crtc);
    }

    close(fd_);
  }

  void SetClient(DRMModesetter::Client* client) { client_ = client; }
  int GetFD() const { return fd_; }

  Size GetDisplaySize() const {
    return {modeset_dev_->mode.hdisplay, modeset_dev_->mode.vdisplay};
  }

  bool ModeSetCrtc() {
    assert(modeset_dev_);
    uint32_t fb_id = client_->GetFrameBuffer(front_buffer_);

    /* perform actual modesetting on each found connector+CRTC */
    modeset_dev_->saved_crtc = drmModeGetCrtc(fd_, modeset_dev_->crtc);
    int ret = drmModeSetCrtc(fd_, modeset_dev_->crtc, fb_id, 0, 0,
                             &modeset_dev_->conn, 1, &modeset_dev_->mode);
    if (ret) {
      fprintf(stderr, "cannot set CRTC for connector %u (%d): %m\n",
              modeset_dev_->conn, errno);
      return false;
    }
    return true;
  }

  bool PageFlip(uint32_t fb_id, void* user_data) {
    int ret = drmModePageFlip(fd_, modeset_dev_->crtc, fb_id,
                              DRM_MODE_PAGE_FLIP_EVENT, user_data);
    if (ret) {
      std::cout << "failed to queue page flip: " << std::strerror(errno)
                << '\n';
      return false;
    }
    return true;
  }

  /*
   * As a next step we need to find our available display devices. libdrm
   * provides
   * a drmModeRes structure that contains all the needed information. We can
   * retrieve it via drmModeGetResources(fd) and free it via
   * drmModeFreeResources(res) again.
   *
   * A physical connector on your graphics card is called a "connector". You can
   * plug a monitor into it and control what is displayed. We are definitely
   * interested in what connectors are currently used, so we simply iterate
   * through the list of connectors and try to display a test-picture on each
   * available monitor.
   * However, this isn't as easy as it sounds. First, we need to check whether
   * the connector is actually used (a monitor is plugged in and turned on).
   * Then we need to find a CRTC that can control this connector. CRTCs are
   * described later on. After that we create a framebuffer object. If we have
   * all this, we can mmap() the framebuffer and draw a test-picture into it.
   * Then we can tell the DRM device to show the framebuffer on the given CRTC
   * with the selected connector.
   *
   * As we want to draw moving pictures on the framebuffer, we actually have to
   * remember all these settings. Therefore, we create one "struct modeset_dev"
   * object for each connector+crtc+framebuffer pair that we successfully
   * initialized and push it into the global device-list.
   */
  bool Initialize(const std::string& card) {
    fprintf(stdout, "using card: '%s': %m\n", card.data());

    /* open the DRM device */
    if (!DeviceOpen(card))
      return false;

    /* prepare all connectors and CRTCs */
    if (!GetConnector())
      return false;

    return true;
  }

  bool Run() {
    fd_set fds;
    drmEventContext evctx = {};
    evctx.version = DRM_EVENT_CONTEXT_VERSION;
    evctx.page_flip_handler = OnModesetPageFlipEvent;
    bool is_running = true;

    while (is_running) {
      front_buffer_ ^= 1;
      if (!PageFlip(client_->GetFrameBuffer(front_buffer_), this)) {
        std::cout << "failed page flip.\n";
        return false;
      }

      page_flip_pending_ = true;
      while (page_flip_pending_) {
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(GetFD(), &fds);
        int ret = select(GetFD() + 1, &fds, nullptr, nullptr, nullptr);
        if (ret < 0) {
          std::cout << "select err: " << std::strerror(errno) << '\n';
          return false;
        } else if (ret == 0) {
          fprintf(stderr, "select timeout!\n");
          return false;
        }

        if (FD_ISSET(0, &fds)) {
          is_running = false;
        }
        if (FD_ISSET(GetFD(), &fds)) {
          drmHandleEvent(GetFD(), &evctx);
        }
      }
      if (FD_ISSET(0, &fds)) {
        printf("exit due to user-input\n");
      }
    }
    return true;
  }

 private:
  /*
   * When the linux kernel detects a graphics-card on your machine, it loads the
   * correct device driver (located in kernel-tree at ./drivers/gpu/drm/<xy>)
   * and provides two character-devices to control it. Udev (or whatever
   * hotplugging application you use) will create them as:
   *     /dev/dri/card0
   *     /dev/dri/controlID64
   * We only need the first one. You can hard-code this path into your
   * application
   * like we do here, but it is recommended to use libudev with real hotplugging
   * and multi-seat support. However, this is beyond the scope of this document.
   * Also note that if you have multiple graphics-cards, there may also be
   * /dev/dri/card1, /dev/dri/card2, ...
   *
   * We simply use /dev/dri/card0 here but the user can specify another path on
   * the command line.
   *
   * modeset_open(out, node): This small helper function opens the DRM device
   * which is given as @node. The new fd is stored in @out on success. On
   * failure, a negative error code is returned.
   * After opening the file, we also check for the DRM_CAP_DUMB_BUFFER
   * capability.
   * If the driver supports this capability, we can create simple memory-mapped
   * buffers without any driver-dependent code. As we want to avoid any radeon,
   * nvidia, intel, etc. specific code, we depend on DUMB_BUFFERs here.
   */
  bool DeviceOpen(const std::string& card) {
    fd_ = open(card.data(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
      fprintf(stderr, "cannot open '%s': %m\n", card.data());
      return false;
    }
    return true;
  }

  /*
   * So as next step we need to actually prepare all connectors that we find. We
   * do this in this little helper function:
   *
   * modeset_prepare(fd): This helper function takes the DRM fd as argument and
   * then simply retrieves the resource-info from the device. It then iterates
   * through all connectors and calls other helper functions to initialize this
   * connector (described later on).
   * If the initialization was successful, we simply add this object as new
   * device into the global modeset device list.
   *
   * The resource-structure contains a list of all connector-IDs. We use the
   * helper function drmModeGetConnector() to retrieve more information on each
   * connector. After we are done with it, we free it again with
   * drmModeFreeConnector().
   * Our helper modeset_setup_dev() returns -ENOENT if the connector is
   * currently
   * unused and no monitor is plugged in. So we can ignore this connector.
   */
  bool GetConnector() {
    /* retrieve resources */
    drmModeRes* res = drmModeGetResources(fd_);
    if (!res) {
      fprintf(stderr, "cannot retrieve DRM resources (%d): %m\n", errno);
      return false;
    }

    /* iterate all connectors */
    for (int i = 0; i < res->count_connectors; ++i) {
      /* get information for each connector */
      drmModeConnector* conn = drmModeGetConnector(fd_, res->connectors[i]);
      if (!conn) {
        fprintf(stderr, "cannot retrieve DRM connector %u:%u (%d): %m\n", i,
                res->connectors[i], errno);
        continue;
      }

      /* check if a monitor is connected */
      if (conn->connection != DRM_MODE_CONNECTED) {
        drmModeFreeConnector(conn);
        fprintf(stderr, "ignoring unused connector %u\n", conn->connector_id);
        continue;
      }

      /* check if there is at least one valid mode */
      if (!conn->count_modes) {
        drmModeFreeConnector(conn);
        fprintf(stderr, "no valid mode for connector %u\n", conn->connector_id);
        continue;
      }

      /* create a device structure */
      std::unique_ptr<ModesetDev> dev(new ModesetDev());
      dev->conn = conn->connector_id;
      dev->mode = conn->modes[0];

      /* find a crtc for this connector */
      if (!FindCrtc(fd_, res, conn, &dev->crtc)) {
        fprintf(stderr, "cannot setup device for connector %u:%u (%d): %m\n", i,
                res->connectors[i], errno);
        drmModeFreeConnector(conn);
        continue;
      }

      /* free connector data and link device into global list */
      drmModeFreeConnector(conn);
      modeset_dev_ = dev.get();
      modeset_dev_list_.push_back(std::move(dev));

      // FIXME(dshwang): GetConnector() can support multiple connector, but
      // it makes page flip logic is so complicated. So use only one connector.
      // Most embedded devices have only one monitor.
      break;
    }

    /* free resources again */
    drmModeFreeResources(res);
    return true;
  }

  /*
   * modeset_find_crtc(fd, res, conn, dev): This small helper tries to find a
   * suitable CRTC for the given connector. We have actually have to introduce
   * one more DRM object to make this more clear: Encoders.
   * Encoders help the CRTC to convert data from a framebuffer into the right
   * format that can be used for the chosen connector. We do not have to
   * understand any more of these conversions to make use of it. However, you
   * must know that each connector has a limited list of encoders that it can
   * use. And each encoder can only work with a limited list of CRTCs. So what
   * we do is trying each encoder that is available and looking for a CRTC that
   * this encoder can work with. If we find the first working combination, we
   * are happy and write it into the @dev structure.
   * But before iterating all available encoders, we first try the currently
   * active encoder+crtc on a connector to avoid a full modeset.
   *
   * However, before we can use a CRTC we must make sure that no other device,
   * that we setup previously, is already using this CRTC. Remember, we can only
   * drive one connector per CRTC! So we simply iterate through the
   * "modeset_list" of previously setup devices and check that this CRTC wasn't
   * used before. Otherwise, we continue with the next CRTC/Encoder combination.
   */
  bool FindCrtc(int fd,
                drmModeRes* res,
                drmModeConnector* conn,
                uint32_t* crtc_out) {
    /* first try the currently conected encoder+crtc */
    if (conn->encoder_id) {
      drmModeEncoder* enc = drmModeGetEncoder(fd, conn->encoder_id);
      if (enc && enc->crtc_id) {
        uint32_t crtc = enc->crtc_id;
        for (auto& dev : modeset_dev_list_) {
          if (dev->crtc == crtc) {
            break;
          }
        }

        if (crtc >= 0) {
          drmModeFreeEncoder(enc);
          *crtc_out = crtc;
          return true;
        }
      }
      drmModeFreeEncoder(enc);
    }

    /* If the connector is not currently bound to an encoder or if the
     * encoder+crtc is already used by another connector (actually unlikely
     * but lets be safe), iterate all other available encoders to find a
     * matching CRTC. */
    for (int i = 0; i < conn->count_encoders; ++i) {
      drmModeEncoder* enc = drmModeGetEncoder(fd, conn->encoders[i]);
      if (!enc) {
        fprintf(stderr, "cannot retrieve encoder %u:%u (%d): %m\n", i,
                conn->encoders[i], errno);
        continue;
      }

      /* iterate all global CRTCs */
      for (int j = 0; j < res->count_crtcs; ++j) {
        /* check whether this CRTC works with the encoder */
        if (!(enc->possible_crtcs & (1 << j)))
          continue;

        /* check that no other device already uses this CRTC */
        uint32_t crtc = res->crtcs[j];
        for (auto& dev : modeset_dev_list_) {
          if (dev->crtc == crtc) {
            break;
          }
        }

        /* we have found a CRTC, so save it and return */
        if (crtc >= 0) {
          drmModeFreeEncoder(enc);
          *crtc_out = crtc;
          return true;
        }
      }

      drmModeFreeEncoder(enc);
    }

    fprintf(stderr, "cannot find suitable CRTC for connector %u\n",
            conn->connector_id);
    return false;
  }

  // As soon as page flip, notify the client to draw the next frame.
  void DidPageFlip(unsigned int sec, unsigned int usec) {
    page_flip_pending_ = false;
    client_->DidPageFlip(front_buffer_, sec, usec);
  }

  static void OnModesetPageFlipEvent(int fd,
                                     unsigned int frame,
                                     unsigned int sec,
                                     unsigned int usec,
                                     void* data) {
    DRMModesetter::Impl* self = static_cast<DRMModesetter::Impl*>(data);
    self->DidPageFlip(sec, usec);
  }

  struct ModesetDev {
    // the display mode that we want to use
    drmModeModeInfo mode;
    // the connector ID that we want to use with this buffer
    uint32_t conn;
    // the crtc ID that we want to use with this connector
    uint32_t crtc;
    // the configuration of the crtc before we changed it. We use it so we can
    // restore the same mode when we exit.
    drmModeCrtc* saved_crtc = nullptr;
  };

  int fd_ = -1;
  unsigned int front_buffer_ = 0;
  DRMModesetter::Client* client_ = nullptr;
  std::list<std::unique_ptr<ModesetDev>> modeset_dev_list_;
  // Use the first modeset device.
  ModesetDev* modeset_dev_ = nullptr;

  // return true when a page-flip is currently pending, that is, the kernel will
  // flip buffers on the next vertical blank.
  bool page_flip_pending_ = false;
};

// static
std::unique_ptr<DRMModesetter> DRMModesetter::Create(const std::string& card,
                                                     bool atomic) {
  std::unique_ptr<DRMModesetter> drm(new DRMModesetter());
  if (drm->Initialize(card))
    return drm;
  return nullptr;
}

DRMModesetter::DRMModesetter() {}

DRMModesetter::~DRMModesetter() {}

bool DRMModesetter::Initialize(const std::string& card) {
  impl_.reset(new Impl());
  return impl_->Initialize(card);
}

void DRMModesetter::SetClient(DRMModesetter::Client* client) {
  impl_->SetClient(client);
}

int DRMModesetter::GetFD() const {
  return impl_->GetFD();
}

DRMModesetter::Size DRMModesetter::GetDisplaySize() const {
  return impl_->GetDisplaySize();
}

bool DRMModesetter::ModeSetCrtc() {
  return impl_->ModeSetCrtc();
}

bool DRMModesetter::PageFlip(uint32_t fb_id, void* user_data) {
  return impl_->PageFlip(fb_id, user_data);
}

bool DRMModesetter::Run() {
  return impl_->Run();
}

}  // namespace ged
