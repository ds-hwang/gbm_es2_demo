# GBM ES2 Demo
* Example OpenGL ES2 demo using GBM and DRM (KMS) modesetting
* This shows how ChromeOS renders GLES2 contents on the screen.
* It uses OpenGL ES2/3 without X11 dependency, so it fits pretty good with the embedded devices world.

![Alt text](https://github.com/ds-hwang/gbm_es2_demo/blob/master/images/screenshot.jpg "dma_buf_mmap_demo screenshot")

# Build
* If you are on Ubuntu, first make sure you have the dependencies installed
```
> sudo apt-get install libgles2-mesa-dev libgbm-dev
```

```
> mkdir build; cd build
> cmake ../
> make
```

# Run
* I have successfully ran it on Ubuntu, ChromeOS and Yocto.

## Ubuntu
* Go to tty1 with Ctrl + Alt + F1
* Kill gdm or lightdm because they are DRM master now. This demo has to be DRM master.
```
> sudo service lightdm stop
```

* Run the demo
```
> gbm_es2_demo
or
> gbm_es2_demo -M # mmap test
```

## Yocto
* The easiest way to build embedded linux image is to use Yocto.
* I make Yocto recipes to make standalone emebeded OpenGL ES2 demo image.
* Check [Yocto GBM ES2 Demo](https://github.com/ds-hwang/yocto-gbm_es2_demo)
* Enjoy building Linux image from the scratch.

# Demo detail
## gbm_es2_demo
* Show how to glue DRM, GBM and EGL
* Show how to swap buffer and vsync
* Show how to implement GLES2 app

## gbm_es2_demo -M
* Same to gbm_es2_demo except for updating cube surface using dma_buf mmap
* It's very nice demo to show how Chromium zero-copy texture upload works. Chromium doesn't use glTexImage2D to update texture thank to new dma_buf mmap API. For more Chromium detail, check [the chrome issue](crbug.com/475633)
* After Linux kernel v4.6, you can use following code. (currently only Intel Architecture supports it)
* [Tiago Vignatti](https://github.com/tiagovignatti) made [this change](http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=c11e391da2a8fe973c3c2398452000bed505851e) in kernel.
```
  void* data = mmap(dma_buf_fd)
  update contents on |data|
  munmap(data)
```

# Code style
* The style complying with [Chromium’s style guide](http://www.chromium.org/developers/coding-style)
* Before submitting a patch, always run `clang-format`
```
> clang-format-5.0 -i `find . -name "*.cpp" -o -name "*.h"`
```

# Reference
* [overview by jbarnes](http://virtuousgeek.org/blog/index.php/jbarnes/2011/10/31/writing_stanalone_programs_with_egl_and_)
* [My mesa-demo](https://lists.freedesktop.org/archives/mesa-dev/2016-April/114985.html)
* [My ChromeOS dma-test demo](https://chromium-review.googlesource.com/#/c/340953/5)
* [greatest drm tutorial by dvdhrm](https://github.com/dvdhrm/docs)
* [kmscube by robclark](https://github.com/robclark/kmscube)

