
import serial
from pathlib import Path
from gpiozero import DigitalInputDevice
from threading import Event
from time import monotonic
 

# ================= CONFIGURATION =================
PORT = "/dev/ttyUSB0"
BAUD = 1000000
BOARD = [1, 2, 3]
CHUNK_SIZE = 4096
ACK_TIMEOUT = 5

# Raspberry Pi BCM GPIO numbers
ACK_PINS = {
    1: 17,
    2: 27,
    3: 22
}

# ================= ACK SETUP =================
ACK_INPUTS = {
    board_id: DigitalInputDevice(pin, pull_up=False)
    for board_id, pin in ACK_PINS.items()
}

ACK_EVENTS = {
    board_id: Event()
    for board_id in ACK_PINS
}

ACK_COUNTS = {
    board_id: 0
    for board_id in ACK_PINS
}


def log(message):
    print(f"[{monotonic():.3f}] {message}", flush=True)


def make_ack_on(board_id):
    def on_ack():
        ACK_COUNTS[board_id] += 1
        ACK_EVENTS[board_id].set()

        log(
            f">>> ACK HIGH detected: Board {board_id} "
            f"/ GPIO{ACK_PINS[board_id]} "
            f"(rising edges: {ACK_COUNTS[board_id]})"
        )

    return on_ack


def make_ack_off(board_id):
    def on_ack_off():
        log(
            f"<<< ACK LOW: Board {board_id} "
            f"/ GPIO{ACK_PINS[board_id]}"
        )

    return on_ack_off


# Attach GPIO callbacks
for board_id, device in ACK_INPUTS.items():
    device.when_activated = make_ack_on(board_id)
    device.when_deactivated = make_ack_off(board_id)


def print_gpio_states():
    for board_id, device in ACK_INPUTS.items():
        log(
            f"GPIO{ACK_PINS[board_id]} "
            f"(Board {board_id}): "
            f"active={device.is_active}, "
            f"raw_value={device.value}, "
            f"rising_edges={ACK_COUNTS[board_id]}"
        )


# ================= IMAGE TRANSMISSION =================
def send_image(ser, board_id, data):

    if board_id not in ACK_EVENTS:
        log(f"ERROR: Invalid board ID: {board_id}")
        return False

    size = len(data)
    ack_event = ACK_EVENTS[board_id]
    ack_pin = ACK_PINS[board_id]

    log(
        f"Starting Board {board_id}: {size} bytes; "
        f"expecting ACK on GPIO{ack_pin}"
    )

    log("ACK GPIO states before header:")
    print_gpio_states()

    # 4-byte image size + 1-byte board ID
    header = (
        size.to_bytes(4, byteorder="little")
        + board_id.to_bytes(1, byteorder="little")
    )

    log(
        f"TX header: {header.hex(' ')} "
        f"(size={size}, board={board_id})"
    )

    ser.write(header)
    ser.flush()

    # Send image data in chunks
    for offset in range(0, size, CHUNK_SIZE):

        chunk = data[offset:offset + CHUNK_SIZE]

        chunk_num = (offset // CHUNK_SIZE) + 1
        total_chunks = (size + CHUNK_SIZE - 1) // CHUNK_SIZE

        # Reset the expected ACK before transmission
        ack_event.clear()
        counts_before = ACK_COUNTS.copy()

        log(
            f"TX Board {board_id} "
            f"chunk {chunk_num}/{total_chunks}: "
            f"bytes {offset}..{offset + len(chunk) - 1}; "
            f"waiting on GPIO{ack_pin}"
        )

        ser.write(chunk)
        ser.flush()

        # Wait for this board's ACK
        if not ack_event.wait(timeout=ACK_TIMEOUT):

            log(
                f"*** ACK TIMEOUT: Board {board_id}, "
                f"chunk {chunk_num}/{total_chunks}, "
                f"offset {offset} ***"
            )

            log("ACK rising edges during this chunk:")

            for bid in ACK_PINS:
                log(
                    f"Board {bid} / GPIO{ACK_PINS[bid]}: "
                    f"{ACK_COUNTS[bid] - counts_before[bid]} "
                    f"new rising edge(s)"
                )

            log("Current GPIO states:")
            print_gpio_states()

            return False

        log(
            f"ACK received for Board {board_id} "
            f"chunk {chunk_num}; "
            f"progress {offset + len(chunk)}/{size} bytes"
        )

    log(f"Board {board_id}: transfer complete")
    return True


# ================= MAIN =================
def main():

    log("ACK monitor initialized")
    print_gpio_states()

    try:
        with serial.Serial(PORT, BAUD, timeout=5) as ser:

            for board_id in BOARD:

                filename = Path(
                    f"image_board{board_id}.bin"
                )

                data = filename.read_bytes()

                if not send_image(ser, board_id, data):
                    log(
                        "Transfer failed. Stopping; "
                        "remaining boards not sent."
                    )
                    break

    finally:
        for device in ACK_INPUTS.values():
            device.close()


if __name__ == "__main__":
    main()

