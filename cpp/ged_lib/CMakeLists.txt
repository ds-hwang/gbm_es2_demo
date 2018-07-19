#
#  Copyright (c) 2016 Dongseong Hwang <dongseong.hwang@intel.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
#

set(GED_LIB ged)
FILE(GLOB SOURCES *.cpp)
FILE(GLOB HEADERS *.h)

include (FindPkgConfig)

pkg_check_modules (DRM libdrm)
pkg_check_modules (GBM gbm)
pkg_check_modules (EGL egl)
pkg_check_modules (GLESV2 glesv2)
include_directories(
    ${DRM_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLESV2_INCLUDE_DIRS}
)
set(LIBS ${LIBS} ${DRM_LIBRARIES} ${GBM_LIBRARIES} ${EGL_LIBRARIES} ${GLESV2_LIBRARIES})

# add the binary tree directory to the search path for
# include files
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

# add the main library
add_library(${GED_LIB} ${SOURCES} ${HEADERS})
target_link_libraries(${GED_LIB} ${LIBS})
MESSAGE(${GED_LIB} " links " ${LIBS})
