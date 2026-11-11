
import serial
from pathlib import Path
from gpiozero import DigitalInputDevice
from threading import Event

PORT = "/dev/ttyUSB0"
BAUD = 1000000
BOARD = [1, 2, 3]
CHUNK_SIZE = 4096

# ACK setup
ackinput = DigitalInputDevice(17, pull_up=False)
#ack2input = DigitalInputDevice(27, pull_up=False)
#ack3input = DigitalInputDevice(22, pull_up=False)

ack1_seen = Event()
#ack2_seen = Event()
#ack3_seen = Event()

ackinput.when_activated = lambda: ack1_seen.set()
#ack2input.when_activated = lambda: ack2_seen.set()
#ack3input.when_activated = lambda: ack3_seen.set()


def send_image(ser, board_id, data):

    size = len(data)

    print(f"Sending {size} bytes to Board {board_id}")

    # Send image size (4 bytes)
    ser.write(size.to_bytes(4, byteorder='little'))

    # Send destination board ID (1 byte)
    ser.write(board_id.to_bytes(1, byteorder='little'))

    ser.flush()

    # Send image data in chunks
    for i in range(0, size, CHUNK_SIZE):

        chunk = data[i:i + CHUNK_SIZE]

        ack1_seen.clear()

        ser.write(chunk)
        ser.flush()

        if not ack1_seen.wait(timeout=5):
            print(f"ACK timeout for Board {board_id}")
            return False

        print(f"Board {board_id}: {i + len(chunk)}/{size}")

    return True


# Main transmission loop
with serial.Serial(PORT, BAUD, timeout=5) as ser:

    for board_id in BOARD:

        filename = f"image_board{board_id}.bin"

        data = Path(filename).read_bytes()

        if not send_image(ser, board_id, data):
            print("Transfer failed. Stopping.")
            break

        print(f"Board {board_id} complete!")
