import re
import serial
import struct
from gpiozero import DigitalInputDevice
from time import time, sleep
from threading import Event

ackinput = DigitalInputDevice(17, pull_up=False)
ack2input = DigitalInputDevice(27, pull_up=False)
ack3input = DigitalInputDevice(22, pull_up=False)
HEADER_FILE = "autoimagedata.h" 
PORT = "/dev/ttyUSB0"
BAUD = 1000000
ack1_seen = Event()
ack2_seen = Event()
ack3_seen = Event()
ackinput.when_activated = lambda: ack1_seen.set()
ack2input.when_activated = lambda: ack2_seen.set()
ack3input.when_activated = lambda: ack3_seen.set()
with open(HEADER_FILE, "r") as f:
	text = f.read()


match = re.search(
	r'autoimagedata\[\]\s*=\s*\{(.*?)\};',
	text,
	re.DOTALL


)

if not match:
	raise RuntimeError("image data yourmom not found")
	

numbers = [int(x) for x in re.findall(r'\d+', match.group(1))]

data = bytes(numbers)

print("Bytes to send:", len(data))
size = len(data)
ser = serial.Serial(PORT, BAUD, timeout=5)
ser.write(size.to_bytes(4, byteorder='little'))
ser.flush()
for i in range(0,len(data),4096):
	chunk = data[i:i+4096]
	ack1_seen.clear()
	ack2_seen.clear()
	ser.write(chunk)
	ser.flush()
	
	if not ack1_seen.wait(timeout=5):
		print("ACK 1 timeout")
		break
	if not ack2_seen.wait(timeout=5):
		print("ACK 2 timeout")
		break
	if not ack3_seen.wait(timeout=5):
		print("ACK 3 timeout")
		break
	print("Sending next")
