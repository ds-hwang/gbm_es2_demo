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

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <getopt.h>
#include <string>

#include "gbm_es2_demo.h"

static const char* shortopts = "AD:M";

static const struct option longopts[] = {{"atomic", no_argument, 0, 'A'},
                                         {"device", required_argument, 0, 'D'},
                                         {"map", no_argument, 0, 'M'},
                                         {0, 0, 0, 0}};

static void usage(const char* name) {
  printf(
      "Usage: %s [-ADMmV]\n"
      "\n"
      "options:\n"
      "    -A, --atomic             use atomic modesetting and fencing\n"
      "    -D, --device=DEVICE      use the given device\n"
      "    -M, --map                mmap test\n",
      name);
}

int main(int argc, char* argv[]) {
  const char* card = "/dev/dri/card0";
  bool atomic = false;
  bool map = false;
  int opt;

  while ((opt = getopt_long_only(argc, argv, shortopts, longopts, nullptr)) !=
         -1) {
    switch (opt) {
      case 'A':
        atomic = true;
        break;
      case 'D':
        card = optarg;
        break;
      case 'M':
        map = true;
        break;
      default:
        usage(argv[0]);
        return -1;
    }
  }

  std::unique_ptr<demo::ES2Cube> demo;
  if (map) {
    demo.reset(new demo::ES2CubeMapImpl());
  } else {
    demo.reset(new demo::ES2CubeImpl());
  }
  if (!demo->Initialize(card)) {
    fprintf(stderr, "failed to initialize ES2Cube.\n");
    return -1;
  }

  if (!demo->Run()) {
    fprintf(stderr, "something wrong happened.\n");
    return -1;
  }

  return 0;
}
