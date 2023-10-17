# Audio player
This example shows how to stream audio files over a network, decode them, and play them to a speaker. It also allows the user to presss a button to pause audio playback.

Audio files can be streamed from a HTTP URL or local file. The audio files themselves can be MP3 or WAV. The decoded audio signal can be output via I2S or PWM. The I2S output is implemented purely in Python using the RP2's PIO state machine.

The example is implemented in two ways: a synchronous version using threads and an asynchronous version using asyncio.

## Module dependencies
```
import casyncio as asyncio
import audio_i2s
import audio_mp3
import audio_wav
import http.client
import machine
import re
import threading
import time
```
`casyncio` is a MicroPython port of CPython's [`asyncio`](https://docs.python.org/3/library/asyncio.html)  module. It is called "casyncio" to differentiate it from other asyncio implementations, but should be called "asyncio" once imported.

`audio_i2s` is a local Python module that contains the I2S implementation using PIO.

`audio_mp3` is a native module for decoding MP3 streams.

`audio_wav` is a local Python module for reading WAV files.

`http.client` is a MicroPython port of CPython's [`http.client`](https://docs.python.org/3/library/http.client.html) module.

The remaining modules should already be familiar to a MicroPython/Python users.

## Hardware setup
```
I2S_SD = machine.Pin(26)
I2S_SCK = machine.Pin(27)
I2S_WS = machine.Pin(28)

PWM_A = machine.Pin(16)
PWM_B = machine.Pin(17)

BUTTON = machine.Pin(15)
```
Next are the pin definitions for connecting to external hardware. The pins should be connected as in this diagram.
![schematic](/examples/async/schematic.png)

There are 3 connected hardware devices: an I2S amplifier, an analog amplifier, and a button, plus a speaker connected to one of the amplifiers. You only need to connect one of the amplifiers depending on which audio output you want to use. If neither amplifier is connected, then the code will still work, you just won't hear any audio. Some suggested amplifiers to use are [this](https://www.adafruit.com/product/3006) for I2S and [this](https://www.adafruit.com/product/2130) for analog.

For the analog amplifier, the passive components are necessary to reduce the input to the amplifier from the RP2040's 3.3 V output and to low-pass filter the PWM signal.

You can use any button. The RP2040 includes a built-in pull-up and Schmitt trigger.

## Sync version
```
def open_stream(path):
  ...
```
This function opens an audio file as a stream. The argument to the function is either a path on the local filesystem or a HTTP URL. The function opens the file, determines if it is a MP3 or WAV file based on its extension, and then wraps it in the appropriate decoder. The return value in either case is a stream object, which is similar a [stream](https://docs.python.org/3/library/io.html#io.RawIOBase) object in CPython.

```
def control(button, audio_out):
  ...
```
This function waits for a button press and toggles the audio output on and off. It is run in a separate thread to the main program.
```
      button.wait(button.PULSE_DOWN)
```
This is the key line in the function that waits for a button press. The button pin is pulled high, so the pin input will be high when the button is not pressed. When the button is pressed, the pin will go low and then return to high when the button is released. This looks like a pulse (in the down direction) to the input of the pin, which is the kind of pin event we wait for. Other things we could wait for are rising or falling edges, or high or low levels. The `wait` function will block the calling thread until a pulse is received on the button pin. The `wait` function returns the duration of the pulse, but we don't use it here.

The function is an infinite loop, so how does it exit? When the main thread is done playing audio it will *close* the button pin. This will invalidate the `wait` call on the button object causing it to raise an exception and exit the function. This is not necessarily a great way of doing it, but thread synchronization is hard, and usually incorrect.

```
def play(path, audio_out_factory, buf_size=2304, **kwargs):
  decoder = open_stream(path)
  
  bytes_per_sample = (decoder.bits_per_sample + 7) // 8
  audio_out = audio_out_factory(decoder.num_channels, decoder.sample_rate, bytes_per_sample, buf_size, **kwargs)

  button = button_factory()
  control_thread = threading.Thread(target=control, args=(button, audio_out))
```
This is the main function that plays the audio stream. First the function opens the audio source with `open_stream`, creates the audio sink using one of the factory methods, and intializes the button. Then we start the control thread to monitor for button presses.

```
      br = decoder.readinto(buf)
      bw = audio_out.write(buf, br)
      assert bw == br
```
Next comes the main loop. The loop works by reading a buffer of data from the `decoder` stream and writing the same buffer to the `audio_out` stream. The size of this buffer is determined by the `buf_size` parameter. The default size of 2304 comes from the buffer size produced by the MP3 decoder, which makes an efficient default.

Both the `readinto` and `write` calls are potentially blocking. Because dealing with partial reads and writes is inefficient (e.g., copying `bytearray` objects) and less readable, a few hacks have been made. First the `write` method takes an extra optional parameter which is the maximum number of bytes to write. So if `readinto` returns a partial read (i.e., `br != len(buf)`) then the partial contents the buffer can be passed to `write` without messing with the `bytearray`. Second, we assume that there are no partial writes, i.e., `write` writes the full amount requested or fails. This way we don't have to mess with the `bytearray` again to write the remainder of the buffer. This is a valid assumption for audio output streams since they are isochronous.

This code also prints a "." every second to give some feedback that it is working.

```
    audio_out.drain()
  finally:
    decoder.close()
    button.close()
    control_thread.join()
    audio_out.close()  
```
Finally we shut everything down. The `drain` call waits for all buffered audio data to be transmitted to the speaker. We close the button pin in order to trigger the control thread to shutdown, and then wait for the control thread to exit.

## Async version
Next comes the async version of the program. This progam has exactly the same functionality as the "sync" version, but uses asyncio instead of threading.
```
def pin_wait(pin, value):
    return asyncio.stream_wait(pin, None, type(pin).wait, pin, value)

stream_readinto = asyncio.stream_readinto

stream_write = asyncio.stream_write

def audio_out_drain(audio_out):
  return asyncio.stream_wait(None, audio_out, type(audio_out).drain, audio_out)
```
In the sync version there were 4 blocking calls used: `wait` to wait for a pin singal, `readinto`/`write` to read/write from streams, and `drain` to wait for the audio sink to drain. In the async world blocking is bad news: you should never make blocking calls in async code as they block the async scheduler and prevent other tasks from running. So we need to somehow get async or awaitable versions of these functions.

What follows here is not compatible with CPython. CPython basically has two implementations of blocking functions: a sync version and an async version. In MicroPythonRT I didn't want to do that because the hardware is resource-constrained and we can't afford to double the code size. (I also didn't want to implement everything twice.) So instead there is a generic mechanism to "async-ify" existing synchronous blocking functions. That mechanism is the function `stream_wait` in the `casyncio` module.

```
def stream_wait(reader, writer, func, *args)
```
`reader` and `writer` are stream objects to be waited on for reading and/or writing respectively. (Typically one of these will be `None` since most operations are pure reads or writes). `func` is a blocking function to call, and `args` is its arguments. The function returns an awaitable object.

All blocking is done via stream objects. Internally stream object implementations have the ability to signal the MicroPython runtime from the outside that they are ready for more operations. To be used on async code, stream objects must first be put into non-blocking mode by calling `settimeout(0)`. This causes blocking methods to not actually block any more and instead return `None` if the operation cannot be completed immediately. Essentially `stream_wait` will keep calling `func` until it returns something other than `None`. If it does return `None`, then it uses the stream signalling mechanism to be notified when to try again, so there is no busy polling. During this time the scheduler can execute other tasks until the stream is ready again.

For common stream operations like `read`, `readinto`, and `write`, the `casyncio` module already provide default async function wrappers which internally use `stream_wait`. However if the object has other blocking methods, we have to wrap them ourselves. This is what we do here for calling `wait` on the pin object and `drain` on the audio out object.

```
async def control_async(button, audio_out): 
  try:
    stopped = False
    while True:
      await pin_wait(button, button.PULSE_DOWN)

      if stopped:
        print('\n', end='')
        audio_out.start()
      else:
        audio_out.stop()
        print('\nstopped', end='')
      stopped = not stopped

  except:
    pass
```
This is the corountine for the button monitoring control loop. It is almost identical to the sync version except that it uses `await` to wait for the button presses.

```
async def play_async(path, audio_out_factory, buf_size=2304, **kwargs):
  ...
```
Finally here is the coroutine implementation of the main play function. Again it is almost identical to the sync version except for the introduction of `await` expressions.

Some other differences are setting the streams to be non-blocking by calling `settimeout`, and creating an async task instead of a thread for the button control loop.

One thing to point out is that `open_stream` is also a blocking call because it opens a socket to a HTTP server. This function should also be async-ified in the async version. However it is harder to async-ify because it involves digging in to the HTTP client implementation. This is where the "viral" nature of async/await programming can kick in as it forces you to rewrite your code.

### A note about the `casyncio` module
This module is a mechanical port of the CPython `asyncio` module. I did this as an expedient way to get a full-featured, reference implementation of `asyncio` up and running to write these examples. I don't think it is a great choice for an `asyncio` module for MicroPython due to its large size.

A similar thing can also be said about the `http.client` module used by this example.

## Running the code
Finally we come to how to call this code. For the sync version it is simply:
```
play('file.mp3', pwm_factory)
```
For the async version, you can type this in the REPL:
```
await play_async('file.mp3', pwm_factory)
```
Or you can run this from inside a main module:
```
asyncio.run(play_async('file.mp3', pwm_factory))
```

## Usage Notes
Decoding MP3 streams is computationally intensive for the Cortex-M0+ processor of the RP2040. It can do it, but it uses almost all the CPU resource. The sync version squeezes by but with the added overhead of asyncio, the async version hits the CPU limit. It still sounds okay but it is slightly distorted due to stretching of audio samples.

If you want to stream files over a network, your network throughput has to be faster than the bitrate of the audio you're streaming. This can be particularly challenging for uncompressed WAV files and a wifi connection. My main motivation for adding USB networking was for consistent throughput for performance testing.

SSL (mbedtls) is also resource intensive for the RP2040. Streaming an MP3 stream over HTTPS is possible but leaves little wiggle room on resource usage. It is easy to run out of RAM especially, as well as CPU.

The libhelix library used for MP3 decoding can also decode AAC. However its out-of-the-box RAM usage is so large that it is basically a non-starter.
