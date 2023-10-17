import casyncio as asyncio
import machine
import socket


def socket_accept(socket):
    return asyncio.stream_wait(socket, None, type(socket).accept, socket)


async def stream_copy(dst, src, buf_size=64):
    buf = bytearray(buf_size)
    br = await src.readinto(buf)
    while br > 0:
        await dst.write(buf, br)
        br = await src.readinto(buf)


TX_PIN = machine.Pin(0)
RX_PIN = machine.Pin(1)
BAUD_RATE = 115200
SERVER_PORT = 8080


async def main():
    uart = machine.UART(RX_PIN, TX_PIN, BAUD_RATE)
    uart = asyncio.Stream(uart)

    server = socket.create_server(("", SERVER_PORT))
    server.settimeout(0)
    print("listening...")

    try:
        conn, address = await socket_accept(server)
        while True:
            print("connected", address)
            conn = asyncio.Stream(conn)

            async def run(conn):
                task = asyncio.create_task(stream_copy(conn, uart))
                try:
                    await stream_copy(uart, conn)
                finally:
                    print("closed", address)
                    conn.close()
                    task.cancel()

            task = asyncio.create_task(run(conn))

            try:
                conn, address = await socket_accept(server)
            finally:
                task.cancel()
    finally:
        server.close()
