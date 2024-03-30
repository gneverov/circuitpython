# Getting Started
MicroPythonRT takes advantage of its dynamic linking functionality to build a board out of independent modules. First the base firmware is flashed onto the device, then after that zero or more extensions modules can also be flashed.

## Installing base firmware

### Download
Download a firmware UF2 image for your board:
- [RPI PICO](https://github.com/gneverov/micropythonrt/releases/download/v0.0.1/firmware.uf2)

Only the RPI PICO and RPI PICO W boards are supported. This firmware may also work on other RP2040 boards, but it is not tested.

> [!CAUTION]
> If you have another MicroPython/CircuitPython installation on theboard, flashing this UF2 file will erase any existing filesystem on your board.

The default builds are configured as follows:
- One USB MSC device for accessing the on-board filesystem, and acting as a UF2 loader for extension modules.
- One USB CDC device that is used as stdio.

### Install
Installing firmware is the same as MicroPython:
1. Press and hold down BOOTSEL button while connecting the board to USB.
1. A USB drive should appear. Copy the downloaded UF2 file to this drive.
1. Connect to the device using [mpremote](https://docs.micropython.org/en/latest/reference/mpremote.html) or other terminal program.

## Note about storage modes
Once the firmware is flashed you can connect to the device and get a MicroPython REPL. There is a helper module, [`fsutil`](/ports/rp2/modules/fsutil.py), that helps manage the USB mass storage device. There are three storage modes:
- `local`: The on-board filesystem is mounted as the root and is accessible to MicroPython.
- `usb`: The on-board filesystem is exposed as a USB mass storage device. The host computer can copy files to/from the board, but the filesystem is not accessible from MicroPython.
- `uf2`: The USB mass storage device exposes to the host computer a UF2 loader for flashing extension modules.

To put the board in a particular mode, import the `fsutil` module and call the appropriate function. For example,
```
import fsutil
fsutil.local()
```
This put the board in local filesystem mode where MicroPython code can read and write files.

## Installing extension modules
To put the board in UF2 mode, run the following, and a USB drive named "MPRT_UF2" should appear on the host computer.
```
import fsutil
fsutil.uf2()
```

Next choose the extension module you wish to flash. The following are provided prebuilt.
- [audio_mp3](https://github.com/gneverov/micropythonrt/releases/download/v0.0.1/libaudio_mp3.uf2): An MP3 audio stream decoder.
- [cyw43](https://github.com/gneverov/micropythonrt/releases/download/v0.0.1/libcyw43.uf2): The wifi driver for the PICO W.
- [lvgl](https://github.com/gneverov/micropythonrt/releases/download/v0.0.1/liblvgl.uf2): The [LVGL](https://github.com/lvgl/lvgl) graphics library.

Download a UF2 file and copy it to the UF2 drive the same way you copied the firmware UF2 file.

If flashing was successful, then MicroPython will automatically restart and you can import the new module. If flashing was unsuccessful, then nothing will happen. To see a potentially helpful error message, run:
```
import os
os.dlerror()
```

Note that the PICO only has 2 MB of on-board flash and this is not enough space to flash all of the prebuilt extension modules simultaneously. Attempt to do so will eventually lead to an out of space error. Also be aware that the same "space" is used for [freezing](/examples/freeze/README.md) MicroPython modules. So freezing MicroPython modules reduces the size of extension modules that can be flashed.

To get back to a clean slate, run:
```
import freeze
freeze.clear()
```
This deletes all extension modules and all frozen MicroPython modules, and returns to the state after having just flashed the firmware UF2. From there you can reflash the things you want.

## Building
Building MicroPythonRT firmware is the same as MicroPython. Refer to the MicroPython building [guide](https://docs.micropython.org/en/latest/develop/gettingstarted.html).

Building an extension module is new. Refer to the [audio_mp3](/extmod/audio_mp3/modaudio_mp3.c) module for an example. In addition to creating a CMake library target for your module (i.e., micropy_lib_audio_mp3, for the audio example), the main [CMakeLists.txt](/ports/rp2/CMakeLists.txt) needs to be modified to build the top-level extension module executable and UF2 file. Add the following line to CMakeLists.txt where "audio_mp3" will be the name of the ELF/UF2 files produced.
```
add_micropy_extension_library(audio_mp3 micropy_lib_audio_mp3)
```

## Examples
Check out the [demo apps](/examples/async/README.md) to see examples of MicroPythonRT's unique capabilities.
