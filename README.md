# MicroPythonRT
MicroPythonRT is a fork of [MicroPython](https://github.com/micropython/micropython/) with added support for concurrency and interoperability. 

Concurrency means multiple programs running at the same time. MicroPythonRT achieves this by adopting [FreeRTOS](https://www.freertos.org/) as a foundation layer. FreeRTOS provides robust concurrency and parallelism support for MicroPython and C components. This support extends into the Python API by providing support for the [`threading`](https://docs.python.org/3/library/threading.html), [`select`](https://docs.python.org/3/library/select.html)/[`selectors`](https://docs.python.org/3/library/selectors.html), and [`asyncio`](https://docs.python.org/3/library/asyncio.html) modules.

Interoperability means the ease at which non-Python (e.g., C) code can be integrated into firmware to provide increased functionality in cooperation with or independently of the MicroPython runtime. MicroPythonRT achieves this by providing a common C runtime library and leveraging FreeRTOS to partition software components into independent tasks.

## Highlights
- All use of busy polling and background tasks have been removed from the MicroPython core.

- newlib-nano is used as the system-wide C runtime library. Crucially it implements malloc, allowing C programs to allocate their own memory independent of MicroPython.

- System services such as [lwIP](https://savannah.nongnu.org/projects/lwip/) and [TinyUSB](https://docs.tinyusb.org/en/latest/) run as separate FreeRTOS tasks isolated of MicroPython. There is no problem with MicroPython or Python code being able to interfere with these services, and the MicroPython VM can be restarted without any effect on them.

- MicroPython threads are implemented as FreeRTOS tasks. The user can create any number of Python threads. Whether the threads can execute on separate hardware cores depends on the configuration of FreeRTOS. (The default for RP2 is not multicore.)

- lwIP is configured to use dynamic memory allocation, thus allowing the user to create any number of sockets.

- USB device configuration can be changed at run-time, making it possible to change which USB device classes are exposed to the USB host without rebuilding firmware.

- TinyUSB support for network devices is exposed, allowing the microcontroller to connect to the Internet over a USB cable.

- Arbitrary Python modules can be loaded into flash at run-time instead of RAM. This frees up large amounts of valuable RAM and allows for larger Python programs.

- The MicroPython REPL is extended to support the `await` keyword for `asyncio` programming.

## Supported Hardware
**Ports**: Currently MicroPythonRT is only supported on the RP2 port. However with time and effort it can be supported on any platform that support FreeRTOS. Only the [RPI PICO](https://micropython.org/download/RPI_PICO/) and [RPI PICO W](https://micropython.org/download/RPI_PICO_W/) boards have been tested.

**Multicore**: Multicore support is determined by FreeRTOS, not MicroPythonRT. Multicore support for RP2 exists but is not enabled by default in FreeRTOS. MicroPythonRT has not been tested with multicore support enabled.

**WIFI**: CYW43 support is ported in MicroPythonRT. Other wifi hardware (ninaw10, wiznet5k) are not currently supported.

**Bluetooth**: The recently added Bluetooth support for RP2 in MicroPython has not yet been ported to MicroPythonRT.

## Getting Started ##
### Download
Pre-built firmware is available for:
- [RPI PICO](https://github.com/gneverov/micropythonrt/releases/download/v0.0.0/rpi_pico.uf2)
- [RPI PICO W](https://github.com/gneverov/micropythonrt/releases/download/v0.0.0/rpi_pico_w.uf2)

> [!CAUTION]
> Unlike MicroPython firmware from other sources, these UF2 files contain a full flash image including a filesystem with files for the demo apps. Flashing these UF2 files will erase any existing filesystem on your device.

The default builds are configured as follows:
- One USB MSC device that accesses a FAT file system. The file system is readonly to MicroPython and read/write to the USB host. Similar to how [CircuitPython](https://github.com/adafruit/circuitpython) works, where you can write your py files directly to the board and run them.
- One USB CDC device that is used as stdio.
- One USB RNDIS device for the possibility of USB networking.

### Installing
Installing firmware is the same as MicroPython:
1. Press and hold down BOOTSEL button while connecting the board to USB.
1. A USB drive should appear. Copy the downloaded UF2 file to this drive.
1. Connect to the device using [mpremote](https://docs.micropython.org/en/latest/reference/mpremote.html) or other terminal program.

### Building
Building MicroPythonRT is the same as MicroPython. Refer to the MicroPython building [guide](https://docs.micropython.org/en/latest/develop/gettingstarted.html).

### Examples
Check out the [demo apps](/examples/async/README.md) to see examples of MicroPythonRT's unique capabilities.

## Technical Differences
A detailed list of some of the technical differences from MicroPython.

### Modules
- `array`: The array subscript operator is extended to support store and delete of slices. For example,
```
x = bytearray(10)
x[2:4] = b'hello'
del x[7:]
```

- `audio_mp3`: A new module that provides a Python wrapper of the libhelix-mp3 library for decoding MP3 streams. Primarily  introduced to support the  [audio player demo](/examples/async/audio_player.md). Note that libhelix-mp3 is released under a different [license](/lib/audio/src/libhelix-mp3/LICENSE.txt) that does not permit free commerical use. This module can be disabled at build-time.

- `freeze`: A new module for "freezing" MicroPython runtime data structures into flash to free up RAM. See the [freezing demo](/examples/freeze/README.md) for more details.

- `lvgl`: An interface to the [LVGL](https://github.com/lvgl/lvgl/tree/master) graphics library from MicroPythonRT. See the [LVGL demo](/examples/lvgl/README.md) for more details.

- `micropython`: Information about FreeRTOS tasks is available by calling  new `tasks` method, and information about the C heap is available by calling the `malloc_stats` method.

- `machine.AudioOutPwm`: A class for generating audio through PWM hardware, similar to CircuitPython's [`PWMAudioOut`](https://docs.circuitpython.org/en/8.2.x/shared-bindings/audiopwmio/index.html#audiopwmio.PWMAudioOut) class. See [audio player demo](/examples/async/audio_player.md) for example of use.

- `machine.I2S`: This native class is removed because it can be implemented in pure Python. See [audio_i2s.py](/examples/async/audio_i2s.py) for an example.

- `machine.Pin`: MicroPythonRT does not support user-defined interrupt handlers, so all the interrupt support on the `Pin` class is removed. In its place is the `APin` class (*A* is for asynchronous, I guess). This class has a `wait` method that allows you to wait on 6 types of pin events: level low/high, edge fall/rise, and pulse down/up. The pulse events also return the duration of the pulse similar to Arduino's [`pulseIn`](https://www.arduino.cc/reference/en/language/functions/advanced-io/pulsein/) function. Additionally the `APin` class does not have the concept of modes and just supports a 3-value state:
```
pin.value = None  # input
pin.value = 0     # output low
pin.value = 1     # output high
```

- `machine.PioStateMachine`: Replaces the `rp2.StateMachine` for accessing the RP2040's PIO hardware. The class is redesigned to better support asynchronous operations. The ability to implement all PIO applications in Python is a goal for this redesigned class.

- `network`: This module from MicroPython has been removed. Networking is now hard-coded to only support lwIP.

- `network_cyw43`: This subcomponent of the network module still exists as a way to configure the cyw43 network device (e.g., tell it which wifi network to connect to). The wifi scan method has be updated to use FreeRTOS instead of polling.

- `select`: The select module is rewritten to use FreeRTOS. It exposes a class called `Selector` which is basically a CPython-compatible [`selectors.EpollSelector`](https://docs.python.org/3/library/selectors.html?highlight=selector#selectors.BaseSelector) class. Under the hood, the select module implements a design similar to Linux [epoll](https://linux.die.net/man/4/epoll). In particular, MicroPython streams are extended with a POLL_CTL ioctl, which has similar behavior to Linux's [epoll_ctl](https://linux.die.net/man/2/epoll_ctl) syscall.
The selector is a crucial part of any asyncio implementation. At its heart, an asyncio event loop will contain a selector to bring about IO concurrency between tasks.

- `signal`: A new module similar to the [signal](https://docs.python.org/3/library/signal.html) module from CPython. Signals are important because they are the way of controlling Ctrl-C behavior in Python. For example, when running an asyncio event loop and the user presses Ctrl-C, you don't want to break inside the event loop code and leave the running tasks in an undefined state. Instead you want to cancel all the tasks and wait for their cleanup to finish. The signal module provides a portable way of overriding Ctrl-C behavior. To this end, the only signal type MicroPythonRT supports is SIGINT.

- `socket`: The socket module is rewritten to interact with lwIP through FreeRTOS while maintaining a CPython-compatible API. The `settimeout` method on socket objects is used to control whether the socket is blocking or non-blocking. A blocking socket will block the caller's task but allow other system tasks to run, including other MicroPython threads.

- `socket.netif`: The socket module contains a new global object `netif` that allows Python code to interact with the lwIP netif API for controlling network interfaces. Some example uses:
```
list(socket.netif)          # List all network interfaces
socket.netif['wl0']         # Access a network interface by name
socket.netif['wl0'].wait()  # Wait/blocks until the network interface has an IP address
```
- `_thread` and `threading`: The `_thread` module is implemented using FreeRTOS tasks. Python code can create any number of threads (limited by available RAM). The implementation relies on a GIL, do just as in CPython, there is no automatic compute parallelism advantage to using multiple threads. `_thread` is a low-level module you don't use directly. The higher-level `threading` module in MicroPython is currently minimal. Much work remains to fully implement this module.

- `usb`: A new module that allows Python code to interact with TinyUSB. Support is somewhat limited and currently only exposes CDC devices, in addition to network devices through `socket.netif`.

- `usb.UsbConfig`: Typically the USB configuration of a board is fixed in firmware. However this class allows you to reconfigure the USB configuration at runtime (e.g., change which device classes the board exposes to a USB host). To use this class some prior understanding of the [USB descriptors](https://www.beyondlogic.org/usbnutshell/usb5.shtml) is required. For example, to set the board to have a MSC (storage) device and 5 CDC (serial) devices, you could run:
```
usb_config = usb.UsbConfig()
usb_config.device()
usb_config.configuration()
usb_config.msc()
for _ in range(5):
  usb_config.cdc()
usb_config.save()
```

This class's methods can also take keyword arguments to define values for various USB metadata such as VID:PID and name strings. See the source [code](/extmod/usb/usb_config.c) for details.

### Framework
- **blocking**: Functions such as sleep, blocking I/O, and select will block the calling task using a FreeRTOS API, thus allowing the CPU to continue executing other tasks. Additionally the GIL is released before the blocking call to give other MicroPython threads a chance to run. The macro `MICROPY_EVENT_POLL_HOOK` is not used anywhere.

- **main**: The C main function is responsible for starting FreeRTOS tasks for various subsystems. MicroPython, lwIP, and TinyUSB all execute as separate tasks. cyw43_driver executes as a FreeRTOS timer.

- **memory map**: The 8 kB of RAM in the scratch X and Y regions is used as the stack for the main MicroPython task. The stack space for other tasks, including non-main MicroPython threads, is allocated from the C heap. It does not make sense to have a dedicated stack for the second core.

- **MICROPY_FREERTOS**: The rp2 port of this fork is hard-coded to depend on FreeRTOS. The C preprocessor macro `MICROPY_FREERTOS` is used in common code outside of the port directory to optionally refer to FreeRTOS functionally.

- **argument parsing**: New functions that provide an alternate way of parsing Python function arguments in C that is based on the [PyArg](https://docs.python.org/3/c-api/arg.html) API from CPython.

- **REPL await**: You can use "await" syntax directly from the MicroPython REPL:
```
>>> await coro()
```
This functionality requires that the expression "asyncio.repl_runner" evaluates to an asyncio `Runner` object.

- **stdio** and **libc**: The MicroPython HAL for stdio is implemented by directly calling stdio functions in newlib-nano. MicroPythonRT then contains a newlib "OS implementation" that can send stdio to one of a bare bones UART used during boot, a fancy UART with DMA, or USB CDC. The MicroPythonRT newlib implemenation does not depend on MicroPython and is intended to provide a common API surface for any C components you include in your firmware.

- **USB networking**: TinyUSB provides 3 kinds of USB network devices: RNDIS, ECM, and NCM. Only RNDIS works for me on Windows, and I haven't tried anything on Linux. All 3 kinds can be enabled through the `UsbConfig` class. However TinyUSB only compiles with NCM only or both ECM and RNDIS support. It doesn't compile with all three. Overall I wouldn't say this support is easy to use because of configuration issues on the host side.
Additionally lwIP supports SLIP network interfaces, and running SLIP over a USB CDC is possibly a more reliable way to get networking over USB. However I don't know how to set up a SLIP interface on Windows.

## Acknowledgements
Thanks to the authors of MicroPython and CircuitPython for their awesome projects I was able to build upon.
