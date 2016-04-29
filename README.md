# GBM ES2 Demo
* Example OpenGL ES2 demo using GBM and DRM(KMS) modesetting
* This shows how ChromeOS renders GLES2 contents on the screen.
* It's the best way for embedded device to use OpenGL ES2/3.

# Build
```
> ./autogen.sh
> ./configure
> make
```

# Run
* I successfully run it on Ubuntu as well as ChromeOS.

## Ubuntu
* Go to tty1 with Ctrl + Alt + F1
* Kill gdm or lightdm because they are DRM master now. This demo has to be DRM master.
```
> sudo service lightdm stop
```

* Run the demo
```
> gbm_es2_demo
```

# Code style
* The style complying with [Chromium’s style guide](http://www.chromium.org/developers/coding-style)
* Before submitting a patch, always run `clang-format`
```
> clang-format-3.7 -i *.h *.cpp
```

# Reference
* [overview by jbarnes](http://virtuousgeek.org/blog/index.php/jbarnes/2011/10/31/writing_stanalone_programs_with_egl_and_)
* [mesa-demo](https://cgit.freedesktop.org/mesa/demos/tree/src/egl/opengl/eglkms.c)
* [greatest drm tutorial by dvdhrm](https://github.com/dvdhrm/docs)
* [kmscube by robclark](https://github.com/robclark/kmscube)