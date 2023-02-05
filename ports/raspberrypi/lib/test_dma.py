import rp2pio
import demo_asyncio as asyncio


async def main():
    src = bytearray(10000)
    src[-1] = 255
    dst = bytearray(10000)
    await rp2pio.dmachannel_transfer(src, dst)
    return dst[-1]


loop = asyncio.get_event_loop()

if __name__ == "__main__":
    print(loop.run_until_complete(main()))
