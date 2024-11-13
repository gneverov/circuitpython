import array
import machine


class PioUartTx(machine.PioStateMachine):
    PIO_PROGRAM = array.array("H", [0x9FA0, 0xF727, 0x6001, 0x0642])

    def __init__(self, tx_pin, *, baudrate=115200, fifo_size=256, threshold=0):
        super().__init__(self.PIO_PROGRAM, [tx_pin])
        self.configure_fifo(True, fifo_size, threshold, self.DMA_SIZE_8)
        self.configure_fifo(False, 0)
        self.set_sideset(2, True, False)
        self.set_pin_values([tx_pin], [])
        self.set_pindirs([], [tx_pin])

        self.set_shift(self.OUT, True, False, 32)
        self.set_pins(self.OUT, tx_pin, 1)
        self.set_pins(self.SIDESET, tx_pin, 1)

        self.set_frequency(8 * baudrate)

        self.reset(0)
        self.set_enabled(True)


class PioUartRx(machine.PioStateMachine):
    PIO_PROGRAM = array.array(
        "H", [0x2020, 0xEA27, 0x4001, 0x0642, 0x00C8, 0xC014, 0x20A0, 0x0000, 0x4078, 0x8020]
    )

    def __init__(self, rx_pin, *, baudrate=115200, fifo_size=256):
        super().__init__(self.PIO_PROGRAM, [rx_pin])
        self.configure_fifo(False, fifo_size, 0, self.DMA_SIZE_8)
        self.configure_fifo(True, 0)

        self.set_pindirs([rx_pin], [])
        self.set_pulls([rx_pin], [])

        self.set_pins(self.IN, rx_pin, 1)
        self.set_pins(self.JMP, rx_pin, 1)
        self.set_shift(self.IN, True, False, 32)

        self.set_frequency(8 * baudrate)

        self.reset(0)
        self.set_enabled(True)


class PioUart:
    def __init__(self, tx_pin, rx_pin, baudrate=115200, tx_fifo_size=256, rx_fifo_size=256):
        self.tx = PioUartTx(tx_pin, baudrate=baudrate, fifo_size=tx_fifo_size)
        self.rx = PioUartRx(rx_pin, baudrate=baudrate, fifo_size=rx_fifo_size)

    def close(self):
        self.tx.close()
        self.rx.close()

    def readinto(self, b):
        return self.rx.readinto(b)

    def write(self, b):
        return self.tx.write(b)
