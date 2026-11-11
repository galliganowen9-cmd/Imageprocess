import re
from pathlib import Path

# ==========================================
# CONFIGURATION
# ==========================================

NUM_LEDS = 129
BLADES = 6
DIVISIONS = 600
BYTES_PER_LED = 3

INPUT_FILE = "image.h"

# Each board controls two opposite blades
BOARDS = [
    (0, 3),  # ESP32 1
    (1, 4),  # ESP32 2
    (2, 5)   # ESP32 3
]

# ==========================================
# DERIVED CONSTANTS
# ==========================================

STRIP_SIZE = NUM_LEDS * BYTES_PER_LED

SLICE_SIZE = BLADES * STRIP_SIZE

IMAGE_SIZE = DIVISIONS * SLICE_SIZE

STRIPS_PER_BOARD = 2

BOARD_SLICE_SIZE = STRIPS_PER_BOARD * STRIP_SIZE

BOARD_IMAGE_SIZE = DIVISIONS * BOARD_SLICE_SIZE

# ==========================================
# READ IMAGE.H
# ==========================================

text = Path(INPUT_FILE).read_text()

# Remove comments
text = re.sub(
    r'/\*.*?\*/|//[^\n]*',
    '',
    text,
    flags=re.S
)

# Find the image array initializer
match = re.search(
    r'=\s*\{(.*?)\}\s*;',
    text,
    re.S
)

if not match:
    raise ValueError("Could not find image array in image.h")

# Extract hexadecimal or decimal byte values
values = re.findall(
    r'0[xX][0-9a-fA-F]+|\b\d+\b',
    match.group(1)
)

image = bytes(
    int(value, 16) if value.lower().startswith("0x")
    else int(value)
    for value in values
)

# ==========================================
# VALIDATE IMAGE SIZE
# ==========================================

if len(image) != IMAGE_SIZE:
    raise ValueError(
        f"Image size mismatch!\n"
        f"Expected: {IMAGE_SIZE} bytes\n"
        f"Received: {len(image)} bytes"
    )

print(f"Image loaded: {len(image)} bytes")
print(f"Divisions: {DIVISIONS}")
print(f"Bytes per division: {SLICE_SIZE}")

# ==========================================
# SPLIT IMAGE FOR EACH ESP32
# ==========================================

for board_id, (blade_a, blade_b) in enumerate(BOARDS, start=1):

    output = bytearray(BOARD_IMAGE_SIZE)

    for div in range(DIVISIONS):

        # Starting position of this division
        source_base = div * SLICE_SIZE

        # Starting position in board-specific output
        dest_base = div * BOARD_SLICE_SIZE

        # Find both blades in original image
        a = source_base + blade_a * STRIP_SIZE
        b = source_base + blade_b * STRIP_SIZE

        # Copy blade A
        output[
            dest_base:dest_base + STRIP_SIZE
        ] = image[a:a + STRIP_SIZE]

        # Copy blade B
        output[
            dest_base + STRIP_SIZE:
            dest_base + BOARD_SLICE_SIZE
        ] = image[b:b + STRIP_SIZE]

    # Save board-specific image
    filename = f"image_board{board_id}.bin"

    Path(filename).write_bytes(output)

    print(
        f"Board {board_id}: "
        f"Blades {blade_a}, {blade_b} | "
        f"{len(output)} bytes -> {filename}"
    )

print("\nImage conversion complete!")
