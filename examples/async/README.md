# MicroPythonRT demos
- [audio player](/examples/async/audio_player.md)
- [remote UART](/examples/async/remote_uart.md)
- [LVGL](/examples//lvgl/README.md)

## Network setup
Some apps require a network connection to the device. Getting a network connection depends on which board you are using are how it connects to the Internet. See [network configuration](/network.md) for detailed instructions.

## CPython libraries
Some apps required libraries from standard desktop Python. MicroPython ports of these libraries can generally be found [here](/lib/micropython-lib/python-stdlib/). Particular libraries to call out are:
- [threading](/lib/micropython-lib/python-stdlib/threading/threading.py)
- asyncio, named [casyncio](/lib/micropython-lib/python-stdlib/casyncio/) to avoid conflict with other implementations
