# Picolibc for RP2040
This directory contains a simple CMake wrapper around Picolibc to help integrate it into the MicroPython build system. It only builds Picolibc for RP2040. It does not build the full multilib collection of all ARM variants.

## Compiler selection
Picolibc must be compiled with a version of GCC configured with thread-local storage (TLS) support. Although Picolibc can be compiled without TLS, TLS is needed for multicore support on the RP2040.

Unfortuntately the standard GNU toolchain from [ARM](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) is not built with TLS support configured, so you can't use that.

Depending on your Linux distribution,
```
sudo apt-get install arm-none-eabi-gcc
```
might install a version of GCC configured with TLS.

If that fails then you may need to build the toolchain yourself.

## Building
Meson (the Picolibc build system) expects to find a suitable `arm-none-eabi-gcc` compiler on the PATH.

To manually build using CMake:
```
cd ports/rp2
cmake -S picolibc -B build-picolibc
cmake --build build-picolibc
```

However it is automatically built as part of the top-level Makefile for MicroPython.

## References
 - https://github.com/picolibc/picolibc/blob/1.8.6/doc/build.md
