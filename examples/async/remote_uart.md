# Remote UART
This example shows creating a TCP server that acts as a bridge between a UART and the Internet. 

Why is this useful? Say, you have a tiny microcontroller that doesn't have wifi deployed somewhere, but you need to temporarily connect to it for configuration, debugging, etc. You can connect another board running this example to the tiny microcontroller via its UART connection. Then you can connect to this example app from any computer and interact with the tiny microcontroller like it is on a local serial connection.

## Hardware setup
```
TX_PIN = machine.Pin(0)
RX_PIN = machine.Pin(1)
BAUD_RATE = 115200
SERVER_PORT = 8080
```
`TX_PIN` and `RX_PIN` are the UART pins on the RP2040 that you'll connect to the RX and TX pins respectively of the target microcontroller.

`BAUD_RATE` is the baud rate of the UART connection to the target, and `SERVER_PORT` is the TCP port to listen on.

## Async version
```
def socket_accept(socket):
    return asyncio.stream_wait(socket, None, type(socket).accept, socket)
```
Next we define an async version of the `socket.accept` method. See the discussion [here](/examples/async/audio_player.md#async-version) for more explanation.

```
    uart = machine.UART(RX_PIN, TX_PIN, BAUD_RATE)
    uart = asyncio.Stream(uart)
```
Initialize the UART and wrap it as an async stream.

```
    server = socket.create_server(("", SERVER_PORT))
    server.settimeout(0)
```
Create the listen socket and make it non-blocking. It is not wrapped as an async stream because listen sockets don't support read/write methods.

```
async def run(conn):
    task = asyncio.create_task(stream_copy(conn, uart))
    try:
        await stream_copy(uart, conn)
    finally:
        print('closed', address)
        conn.close()
        task.cancel()

task = asyncio.create_task(run(conn))
```
When we recieve a new TCP connection, start a new async task (the `run` coroutine) for processing the connection. This task first creates another task for copying from the UART to the TCP connection. Then the task itself starts copying data from the TCP connection to the UART. Now available data is being copied between the UART and TCP connection in both directions concurrently.

Next one of three things can happen on the line:
```
    await stream_copy(uart, conn)
```
1. The TCP connection is closed and the `stream_copy` function returns normally.
1. The TCP connection is reset and the `stream_copy` function raises an `OSError`.
1. The task itself is cancelled and the `stream_copy` function raises a `CancelledError`.

In all these cases, the finally block is executed. Here we close our end of the TCP connection and cancel the other task we created at the beginning of the `run` function to stream copy in the other direction. Thus cleaning up all our resources before returning.

Back in the main task:
```
    try:
        conn, address = await socket_accept(server)
    finally:
        task.cancel()
```
While the async task for the client is running concurrently, we wait for a new client to connect. When we get a new connection we cancel the task for the old connection, go around the loop, and create a new task for the new connection. Of course many other behaviors are also possible for handling a second connection.

## Sync version
A synchronous version of this example is possible but not provided. Some strategies are discussed here.

The code in this example needs to do 3 things concurrently: 
1. Listen for new TCP connections
1. Copy data from the TCP connection to the UART
1. Copy data from the UART to the TCP connection

A synchronous version could be implemented using 2 additional threads. The default stack size for a thread is 4 kB. So the overhead for this approach is 8 kB of RAM, which is a lot compared to the 100s of bytes of overhead for the tasks in the async version.

Another way to implement a synchronous version is to use "select". Here the 3 stream objects (the listen socket, the client socket, and the UART) are added to a "selector" object and then only one thread is required to wait on the selector. For example,
```
import select

poll = select.Selector()
poll.register(uart, select.EVENT_READ)
poll.register(server, select.EVENT_READ)
poll.register(conn, select.EVENT_READ)

...

    for event in poll.select():
        # do stuff
```

This approach is certainly more efficient than the async version. However once you finish filling out "do stuff", you might conclude that the code is less readable and less modular when compared to the async version.

Async/await programming is a popular choice for many concurrent applications because it achieves this balance between efficiency and maintainability that is difficult to achieve with synchronous programming.
