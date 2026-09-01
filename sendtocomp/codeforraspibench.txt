import spidev
import time

# ---- CONFIG ----
SPI_BUS = 0
SPI_DEV = 0
SPI_SPEED = 95000000   # 20 MHz (adjust: 10e6, 20e6, 30e6)
TOTAL_BYTES = 100000000   # total transfer size (increase for accuracy)
CHUNK_SIZE = 4096      # safe chunk size for driver

# ---- SETUP ----
spi = spidev.SpiDev()
spi.open(SPI_BUS, SPI_DEV)
spi.max_speed_hz = SPI_SPEED
spi.mode = 0

# Use bytes (NOT list)
data = bytes([0xFF]) * TOTAL_BYTES

print("Starting SPI benchmark...")
print(f"Speed setting: {SPI_SPEED/1e6:.1f} MHz")
print(f"Total bytes: {TOTAL_BYTES}")

# ---- BENCHMARK ----
start = time.time()

for i in range(0, TOTAL_BYTES, CHUNK_SIZE):
    spi.xfer2(data[i:i+CHUNK_SIZE])

end = time.time()

# ---- RESULTS ----
elapsed = end - start
bytes_per_sec = TOTAL_BYTES / elapsed
mb_per_sec = bytes_per_sec / (1024 * 1024)
bits_per_sec = bytes_per_sec * 8

# For your LED project (DotStar = 4 bytes per LED)


print("\n--- RESULTS ---")
print(f"Elapsed time: {elapsed:.6f} s")
print(f"Bytes/sec: {bytes_per_sec:,.2f}")
print(f"MB/sec: {mb_per_sec:.2f}")
print(f"Bits/sec: {bits_per_sec:,.2f}")


spi.close()
