/*
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
 *
 * This example was implemented with reference to the following examples:
 * https://github.com/perceptia/perceptia/blob/master/perceptia/skylane_simple_framework/examples/simple_egl.rs
 * https://github.com/Smithay/drm-rs/blob/master/examples/legacy_modeset.rs
 * https://github.com/joone/gbm_es2_demo/blob/c_version/kmstriangle.c
 */

extern crate drm;
extern crate gbm;
extern crate egl;
extern crate gl;
extern crate cognitive_graphics;

use drm::Device as BasicDevice;
use drm::control::Device as ControlDevice;

use drm::control::ResourceInfo;
use drm::control::ResourceHandle;
use drm::control::{connector, crtc, framebuffer};
use gbm::{AsRaw, Device, Format, BufferObjectFlags};

use std::fs::File;
use std::fs::OpenOptions;
use std::{thread, time};

use std::os::unix::io::RawFd;
use std::os::unix::io::AsRawFd;
use std::os::unix::io::IntoRawFd;

use cognitive_graphics::{egl_tools, gl_tools};

// Vertex shader source code.
const VERTEX_SHADER_CODE: &'static str = "
attribute vec4 position;
void main() {
    gl_Position = position;
}";

// Fragment shader source code.
const FRAGMENT_SHADER_CODE: &'static str = "
precision mediump float;
void main() {
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}";

#[derive(Debug)]
pub struct DrmDevice(File);

pub struct DrmPlatform {}

impl AsRawFd for DrmDevice {
    fn as_raw_fd(&self) -> RawFd {
        self.0.as_raw_fd()
    }
}

impl IntoRawFd for DrmDevice {
    fn into_raw_fd(self) -> RawFd {
        self.0.into_raw_fd()
    }
}

impl BasicDevice for DrmDevice {}
impl ControlDevice for DrmDevice {}

pub struct EglWindow {
    pub display: egl::EGLDisplay,
    pub context: egl::EGLContext,

    pub image: Option<*const std::os::raw::c_void>,
    pub width: u32,
    pub height: u32,
}

struct Egl {
    window: EglWindow,
    program: gl::types::GLuint,
    loc_position: gl::types::GLint,
    gbm_bo: Option<gbm::BufferObject<()>>,
    frame_buffer: drm::control::framebuffer::Handle,
    gl_tex: gl::types::GLuint,
    pub gl_fb: gl::types::GLuint,
}

impl DrmDevice {
    pub fn open(path: &str) -> Self {
        let mut options = OpenOptions::new();
        options.read(true);
        options.write(true);
        DrmDevice(options.open(path).unwrap())
    }

    pub fn open_global() -> Self {
        Self::open("/dev/dri/card0")
    }

    pub fn open_control() -> Self {
        Self::open("/dev/dri/controlD64")
    }
}

impl Egl {
    pub fn new() -> Self {
        Egl {
            window: EglWindow {
                display: std::ptr::null_mut(),
                context: std::ptr::null_mut(),
                width: 0,
                height: 0,
                image: None,
            },
            frame_buffer: drm::control::framebuffer::Handle::from(0),
            gbm_bo: None,
            gl_tex: gl::types::GLuint::default(),
            gl_fb: gl::types::GLuint::default(),
            program: gl::types::GLuint::default(),
            loc_position: gl::types::GLint::default(),
        }
    }

    fn initialize_display(&mut self, gbm: &Device<DrmDevice>) -> Result<bool, String> {
        let width: u32 = 1920;
        let height: u32 = 1080;
        // call eglGetDisplay
        let display = egl_tools::get_gbm_display(gbm.as_raw() as egl::EGLNativeDisplayType);
        let display = if let Ok(display) = display {
            display
        } else {
            return Err(format!("Failed to create display"));
        };

        let mut major = 0;
        let mut minor = 0;
        if !egl::initialize(display, &mut major, &mut minor) {
            return Err(format!("Failed to initialize EGL"));
        };

        println!("EGL majer {} minor {}", major, minor);

        if !egl::bind_api(egl::EGL_OPENGL_ES_API) {
            return Err(format!("Failed to bind API"));
        };

        let extension_string = egl::query_string(display, egl::EGL_EXTENSIONS);
        println!("EGL Extension List = {:?}", extension_string);

        // Check extensions
        if !egl_tools::has_extension(egl::EGL_NO_DISPLAY, "EGL_MESA_platform_gbm") {
            return Err(format!("EGL does not provide GBM extension"));
        }

        if !egl_tools::has_extension(display, "EGL_KHR_image_base") {
            return Err(format!("EGL does not provide KHR image base extension"));
        }

        if !egl_tools::has_extension(display, "EGL_EXT_image_dma_buf_import") {
            return Err(format!("EGL does not provide dma buf import extension"));
        }

        const CONTEXT_ATTRIB_LIST: [egl::EGLint; 3] =
            [egl::EGL_CONTEXT_CLIENT_VERSION, 2, egl::EGL_NONE];
        let ctx = egl::create_context(
            display,
            std::ptr::null_mut(),
            egl::EGL_NO_CONTEXT,
            &CONTEXT_ATTRIB_LIST,
        );

        let context = if let Some(context) = ctx {
            context
        } else {
            return Err(format!("Failed to create EGL context"));
        };

        // Make context current
        if !egl::make_current(display, egl::EGL_NO_SURFACE, egl::EGL_NO_SURFACE, context) {
            return Err(format!("Failed to make current"));
        }

        // Create the window
        self.window = EglWindow {
            // device: device,
            display: display,
            context: context,
            image: None,
            width: width as u32,
            height: height as u32,
        };

        return Ok(true);
    }

    fn create_framebuffer(
        &mut self,
        gbm: &Device<DrmDevice>,
        width: u32,
        height: u32,
    ) -> Result<bool, String> {
        self.gbm_bo = Some(
            gbm.create_buffer_object::<()>(
                width,
                height,
                Format::XRGB8888,
                BufferObjectFlags::SCANOUT | BufferObjectFlags::RENDERING,
            ).unwrap(),
        );

        let gbm_bo_fd = self.gbm_bo.as_ref().unwrap().as_raw_fd();
        println!("gbm_bo_fd = {}", gbm_bo_fd);

        let offset = 0;
        let stride = self.gbm_bo.as_ref().unwrap().stride().unwrap() as i32;
        println!("stride = {}", stride);

        // Create a FB by calling ffi::mode::AddFB:0
        self.frame_buffer = framebuffer::create(gbm, self.gbm_bo.as_ref().unwrap())
            .unwrap()
            .handle();

        let mut attribs = [egl::EGL_NONE; 13];
        let format = Format::XRGB8888;

        attribs[0] = egl_tools::ext::DMA_BUF_PLANE0_FD_EXT;
        attribs[1] = self.gbm_bo.as_ref().unwrap().as_raw_fd();
        attribs[2] = egl::EGL_WIDTH;
        attribs[3] = width as i32;
        attribs[4] = egl::EGL_HEIGHT;
        attribs[5] = height as i32;
        attribs[6] = egl_tools::ext::LINUX_DRM_FOURCC_EXT;
        attribs[7] = format.as_ffi() as i32;
        attribs[8] = egl_tools::ext::DMA_BUF_PLANE0_PITCH_EXT;
        attribs[9] = stride;
        attribs[10] = egl_tools::ext::DMA_BUF_PLANE0_OFFSET_EXT;
        attribs[11] = offset;
        attribs[12] = egl::EGL_NONE;

        // Create an egl image.
        let create_egl_image = egl_tools::get_proc_addr_of_create_image_khr();
        if let Some(create_egl_image) = create_egl_image {
            let egl_image = create_egl_image(
                self.window.display,
                egl::EGL_NO_CONTEXT,
                egl_tools::ext::LINUX_DMA_BUF_EXT,
                std::ptr::null_mut(),
                &attribs as *const _,
            );
            self.window.image = Some(egl_image);
        } else {
            return Err(format!("Failed to call create EGL surface"));
        };
        Ok(true)
    }

    fn set_crtc(&self, gbm: &Device<DrmDevice>, crt: &crtc::Info, con: &connector::Info) {
        // Set the crtc
        let &mode = con.modes().iter().next().expect(
            "No modes found on connector",
        );

        // On many setups, this requires root access.
        crtc::set(
            gbm,
            crt.handle(),
            self.frame_buffer,
            &[con.handle()],
            (0, 0),
            Some(mode),
        ).expect("Could not set CRTC");
    }

    fn page_flip(&self, gbm: &Device<DrmDevice>, crt: &crtc::Info) {
        crtc::page_flip(
            gbm,
            crt.handle(),
            self.frame_buffer,
            &[crtc::PageFlipFlags::PageFlipEvent],
        ).expect("Failed to queue Page Flip");
    }

    fn init_gl(&mut self) -> Result<(), String> {
        // Initialize GL.
        gl::load_with(|s| egl::get_proc_address(s) as *const std::os::raw::c_void);

        // Prepare shader program
        self.program = gl_tools::prepare_shader_program(
            VERTEX_SHADER_CODE.to_string(),
            FRAGMENT_SHADER_CODE.to_string(),
        ).unwrap();
        self.loc_position = gl_tools::get_attrib_location(self.program, "position".to_owned())?;
        unsafe {
            gl::UseProgram(self.program);
            gl::Viewport(0, 0, 1920, 1080);
        }
        Ok(())
    }

    fn create_gl_framebuffers(&mut self) {
        let image_target_texture: Option<egl_tools::ImageTargetTexture2DOesFn>;
        image_target_texture = egl_tools::get_proc_addr_of_image_target_texture_2d_oes();
        if let Some(image_target_tex) = image_target_texture {
            unsafe {
                gl::GenTextures(1, &mut self.gl_tex);
                gl::BindTexture(gl::TEXTURE_2D, self.gl_tex);
                image_target_tex(gl::TEXTURE_2D, self.window.image.unwrap());
                gl::BindTexture(gl::TEXTURE_2D, 0);

                gl::GenFramebuffers(1, &mut self.gl_fb);
                gl::BindFramebuffer(gl::FRAMEBUFFER, self.gl_fb);
                gl::FramebufferTexture2D(
                    gl::FRAMEBUFFER,
                    gl::COLOR_ATTACHMENT0,
                    gl::TEXTURE_2D,
                    self.gl_tex,
                    0,
                );

                if gl::CheckFramebufferStatus(gl::FRAMEBUFFER) != gl::FRAMEBUFFER_COMPLETE {
                    println!("Failed framebuffer check for created taret buffer");
                }
            }
        }
    }

    pub fn draw(&self) {
        let mut vbo_vertices = 0;
        let vertices: [gl::types::GLfloat; 9] = [0.0, 0.5, 0.0, -0.5, -0.5, 0.0, 0.5, -0.5, 0.0];
        let float_size = std::mem::size_of::<gl::types::GLfloat>();
        let vertices_size = float_size * vertices.len();
        unsafe {
            gl::ClearColor(0.5, 0.5, 0.5, 1.0);
            gl::Clear(gl::COLOR_BUFFER_BIT);
            gl::GenBuffers(1, &mut vbo_vertices);

            // Load vetices.
            gl::BindBuffer(gl::ARRAY_BUFFER, vbo_vertices);
            gl::EnableVertexAttribArray(self.loc_position as _);
            gl::VertexAttribPointer(
                self.loc_position as _,
                3,
                gl::FLOAT,
                gl::FALSE,
                3 * float_size as egl::EGLint,
                std::ptr::null(),
            );

            gl::BufferData(gl::ARRAY_BUFFER,
                           vertices_size as isize,
                           vertices.as_ptr() as *const _,
                           gl::DYNAMIC_DRAW);

            // Draw
            gl::DrawArrays(gl::TRIANGLES, 0, 3);
            gl::Finish();
        }
    }

    pub fn destroy(&self, gbm: &Device<DrmDevice>) {
        framebuffer::destroy(gbm, self.frame_buffer).unwrap();
    }
}

fn load_information<T, U>(drm_device: &DrmDevice, handles: &[T]) -> Vec<U>
where
    T: ResourceHandle,
    U: ResourceInfo<Handle = T>,
{
    handles
        .iter()
        .map(|&h| {
            drm_device.resource_info(h).expect(
                "Could not load resource info",
            )
        })
        .collect()
}

pub fn main() {
    // Open("/dev/dri/card0");
    let drm_device = DrmDevice::open_global();
    let drm_fd = drm_device.as_raw_fd();
    println!("drm_fd = {}", drm_fd);
    // Initialize a gbm device: gbm_create_device(drm.fd)
    let gbm = Device::new(drm_device).unwrap();
    let gbm_raw = gbm.as_raw_fd();
    println!("gbm raw fd = {}", gbm_raw);

    // Load the information.
    let res = gbm.resource_handles().expect(
        "Could not load normal resource ids.",
    );
    let coninfo: Vec<connector::Info> = load_information(&gbm, res.connectors());
    let crtcinfo: Vec<crtc::Info> = load_information(&gbm, res.crtcs());

    // Filter each connector until we find one that's connected.
    let con = coninfo
        .iter()
        .filter(|&i| i.connection_state() == connector::State::Connected)
        .next()
        .expect("No connected connectors");

    // Find a crtc and FB
    let crtc = crtcinfo.iter().next().expect("No crtcs found");

    let mut egl_platform = Egl::new();
    egl_platform.initialize_display(&gbm).expect("Failed to initialize EGL");
    egl_platform.create_framebuffer(&gbm, 1920, 1080).expect("Failed to create a frame buffer");
    egl_platform.init_gl().expect("Failed to initialize GL");
    egl_platform.create_gl_framebuffers();
    egl_platform.set_crtc(&gbm, &crtc, &con);

    let mut count = 0;
    loop {
        unsafe {
            gl::BindFramebuffer(gl::FRAMEBUFFER, egl_platform.gl_fb);
        }
        egl_platform.draw();
        egl_platform.page_flip(&gbm, &crtc);

        let delay = time::Duration::from_millis(100);
        thread::sleep(delay);
       /* count += 1;
        if count == 10 {
            break;
        }
*/
    }

    egl_platform.destroy(&gbm);
}
