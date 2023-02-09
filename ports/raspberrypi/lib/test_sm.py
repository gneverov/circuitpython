import asyncio
import rp2pio
import supervisor
import adafruit_pioasm

loop = asyncio.get_event_loop()


async def input_async(prompt=""):
    print(prompt, end="")
    while not supervisor.runtime.serial_bytes_available:
        yield None
    return input()


async def recv_loop(sm):
    while True:
        await sm.wait(False)
        data = sm.recv(128)
        print(data.decode())


async def send_loop(sm):
    while True:
        data = await input_async()
        n = sm.send(data)
        yield None


program = adafruit_pioasm.assemble(
    """out x, 8
in x, 8"""
)
sm = rp2pio.Sm(program, [])
sm.set_shift(False, False, True, 8)
sm.set_shift(True, False, True, 8)
sm.reset(0)
sm.set_enabled(True)

if __name__ == "__main__":
    rx = loop.create_task(recv_loop(sm))
    tx = loop.create_task(send_loop(sm))
    loop.run_forever()
