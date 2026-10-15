
#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>

// ================= CONFIGURATION =================

#define NUM_LEDS    129
#define DIVISIONS   300
#define STRIPS      6

#define LED_PIN     2
#define HALL_PIN    27
#define ACK_PIN     19
#define BOARD_ID    1

#define RX_TIMEOUT 5000

#if BOARD_ID == 1
  const int inda = 0;
  const int indb = 3;

#elif BOARD_ID == 2
  const int inda = 1;
  const int indb = 4;

#elif BOARD_ID == 3
  const int inda = 2;
  const int indb = 5;
#endif

const uint8_t DATA_PIN_A = 21;
const uint8_t DATA_PIN_B = 25;
const uint8_t CLK_PIN = 18;

const size_t STRIP_SIZE = NUM_LEDS * 3;
const size_t SLICE_SIZE = STRIPS * STRIP_SIZE;


// ================= GLOBAL VARIABLES =================

// LED strips
Adafruit_DotStar stripa(NUM_LEDS, DATA_PIN_A, CLK_PIN, DOTSTAR_BGR);
Adafruit_DotStar stripb(NUM_LEDS, DATA_PIN_B, CLK_PIN, DOTSTAR_BGR);

// Image display variables
File imageFile;
int32_t lastDisplayedDiv = -1;
uint8_t stripBuffer[STRIP_SIZE];

// UART variables
uint8_t rxBuffer[4096];
uint32_t imageSize = 0;
uint32_t received = 0;

// Hall sensor variables
volatile uint32_t last_pulse_us = 0;
volatile uint32_t period_us = 0;

const uint32_t DEBOUNCE_US = 1000;
const float ANGLE_OFFSET_DEG = 80.0f;


// ================= HALL INTERRUPT =================

IRAM_ATTR void hallISR() {

  uint32_t now = micros();
  uint32_t dt = now - last_pulse_us;

  if (dt > DEBOUNCE_US) {
    if (period_us == 0) {
      period_us = dt;
    }
    else {
      period_us = (period_us * 7 + dt) >> 3;
    }
    last_pulse_us = now;
  }
}


// ================= FILE MANAGEMENT =================

bool openImage() {

  // Close previously opened image
  if (imageFile) {
    imageFile.close();
  }

  lastDisplayedDiv = -1;

  // Open stored image for reading
  imageFile = LittleFS.open("/image.bin", "r");

  if (!imageFile) {
    Serial.println("No image available");
    return false;
  }

  // Check that the file isn't empty
  if (imageFile.size() == 0) {
    Serial.println("Image file is empty");
    imageFile.close();
    return false;
  }

  Serial.print("Image opened. File size: ");
  Serial.println(imageFile.size());

  return true;
}


// ================= IMAGE RECEIVER =================


void receiveImage() {

  // Receive image size from Raspberry Pi
  if (Serial2.readBytes((uint8_t*)&imageSize, sizeof(imageSize))
      != sizeof(imageSize)) {
    return;
  }

  Serial.print("Incoming size: ");
  Serial.println(imageSize);

  if (imageSize == 0) {
    Serial.println("Invalid incoming image size");
    return;
  }

  if (imageFile) {
    imageFile.close();
  }

  File file = LittleFS.open("/image.bin", "w");

  if (!file) {
    Serial.println("Failed to open image.bin");
    openImage();
    return;
  }

  received = 0;
  bool transmissionFailed = false;

  while (received < imageSize) {

    size_t expected = min((uint32_t)sizeof(rxBuffer),imageSize - received);

    size_t chunkReceived = 0;
    unsigned long lastByteTime = millis();

    while (chunkReceived < expected) {

      int availableBytes = Serial2.available();

      if (availableBytes > 0) {

        size_t toRead = min((size_t)availableBytes,expected - chunkReceived);

        size_t n = Serial2.readBytes(rxBuffer + chunkReceived,toRead);

        if (n > 0) {
          chunkReceived += n;
          lastByteTime = millis();
        }
      }

      if (millis() - lastByteTime >= RX_TIMEOUT) {
        Serial.println("Transmission fail");
        transmissionFailed = true;
        break;
      }
    }

    if (transmissionFailed) {
      break;
    }

    size_t written = file.write(rxBuffer, chunkReceived);

    if (written != chunkReceived) {
      Serial.println("FILE WRITE ERROR");
      transmissionFailed = true;
      break;
    }

    received += chunkReceived;

    Serial.print("Stored: ");
    Serial.println(received);

    // Acknowledge completed chunk
    digitalWrite(ACK_PIN, HIGH);
    delay(15);
    digitalWrite(ACK_PIN, LOW);
  }

  file.close();

  if (!transmissionFailed) {

    // Verify the stored file against the Pi's declared size
    File checkFile = LittleFS.open("/image.bin", "r");

    if (!checkFile) {
      transmissionFailed = true;
    }
    else {
      size_t storedSize = checkFile.size();

      Serial.print("Expected size: ");
      Serial.println(imageSize);

      Serial.print("Stored file size: ");
      Serial.println(storedSize);

      if (storedSize != imageSize) {
        transmissionFailed = true;
      }

      checkFile.close();
    }
  }

  if (transmissionFailed) {

    digitalWrite(ACK_PIN, LOW);
    LittleFS.remove("/image.bin");

    while (Serial2.available() > 0) {
      Serial2.read();
    }

    Serial.println("Transmission failed");
    Serial.println("Waiting for new image");
  }
  else {

    Serial.println("IMAGE RECEIVED AND VERIFIED");

    openImage();
  }

  received = 0;
  imageSize = 0;
}


// ================= IMAGE DISPLAY =================

static inline uint32_t color32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) |
         ((uint32_t)g << 8) |
         (uint32_t)b;
}

void displayDivision(uint16_t div) {

  if (!imageFile) {
    return;
  }

  if (div == lastDisplayedDiv) {
    return;
  }

  size_t base = (size_t)div * SLICE_SIZE;

  Adafruit_DotStar* strips[2] = { &stripa, &stripb };
  const int stripIndices[2] = {inda, indb};

  for (int s = 0; s < 2; s++) {

    int idxStrip = stripIndices[s];

    // Location of required strip in image.bin
    size_t offset = base + idxStrip * STRIP_SIZE;

    if (offset + STRIP_SIZE > imageFile.size()) {
      Serial.println("ERROR: Outside image file");
      return;
    }

    // Seek to selected strip
    if (!imageFile.seek(offset)) {
      Serial.println("ERROR: File seek");
      return;
    }

    // Read only selected strip
    size_t bytesRead = imageFile.read(stripBuffer, STRIP_SIZE);

    if (bytesRead != STRIP_SIZE) {
      Serial.println("ERROR: Size mismatch");
      return;
    }

    // Send RGB data to LED strip
    for (int i = 0; i < NUM_LEDS; i++) {

      uint8_t r = stripBuffer[i * 3 + 0];
      uint8_t g = stripBuffer[i * 3 + 1];
      uint8_t b = stripBuffer[i * 3 + 2];

      strips[s]->setPixelColor(i, color32(r, g, b));
    }

    strips[s]->show();
  }

  lastDisplayedDiv = div;
}


// ================= DISPLAY CONTROLLER =================

void updateDisplay() {

  if (!imageFile) {
    return;
  }

  uint32_t pulse;
  uint32_t period;

  noInterrupts();
  pulse = last_pulse_us;
  period = period_us;
  interrupts();

  if (pulse == 0 || period == 0) {
    return;
  }

  uint32_t now = micros();
  uint32_t elapsed = (now - pulse) % period;

  uint32_t div = (uint64_t)elapsed * DIVISIONS / period;
  div = (DIVISIONS - 1 - div) % DIVISIONS;

  div = (div + (uint32_t)(
    (ANGLE_OFFSET_DEG / 360.0f) * DIVISIONS
  )) % DIVISIONS;

  displayDivision(div);
}


// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  // Initialize acknowledgement pin
  pinMode(ACK_PIN, OUTPUT);
  digitalWrite(ACK_PIN, LOW);

  // Initialize UART
  Serial2.setRxBufferSize(65536);
  Serial2.begin(1000000, SERIAL_8N1, 16, 17);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    while (1);
  }

  Serial.println("LittleFS mounted");

  // Initialize LED strips
  stripa.begin();
  stripb.begin();

  stripa.setBrightness(50);
  stripb.setBrightness(50);

  stripa.show();
  stripb.show();

  // Initialize Hall sensor
  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  attachInterrupt(
    digitalPinToInterrupt(HALL_PIN),
    hallISR,
    FALLING
  );

  digitalWrite(LED_PIN, HIGH);

  // Load existing image if available
  openImage();

  Serial.println("System ready");
}


// ================= MAIN LOOP =================

void loop() {

  // Check for new image
  if (Serial2.available() >= 4) {
    receiveImage();
    return;
  }

  // Display image normally
  updateDisplay();
}
