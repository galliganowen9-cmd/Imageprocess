#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>


#define NUM_LEDS    129
#define DIVISIONS   300
#define STRIPS      6
#define LED_PIN 2
#define HALL_PIN    27
#define BOARD_ID 1

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


const uint8_t CLK_PIN    = 18; 
unsigned long StartTime;
bool interruptDetached = false;
//Interrupt pin initialize
Adafruit_DotStar stripa(NUM_LEDS, DATA_PIN_A, CLK_PIN, DOTSTAR_BGR);

Adafruit_DotStar stripb(NUM_LEDS, DATA_PIN_B, CLK_PIN, DOTSTAR_BGR);

const size_t STRIP_SIZE = NUM_LEDS * 3;
const size_t SLICE_SIZE = STRIPS * STRIP_SIZE;

uint8_t stripBuffer[STRIP_SIZE];

File imageFile;
int32_t lastDisplayedDiv = -1;

volatile uint32_t last_pulse_us = 0;
volatile uint32_t period_us     = 0;

const uint32_t DEBOUNCE_US = 1000;
const float ANGLE_OFFSET_DEG = 80.0f;
//Debounce and angle offset
IRAM_ATTR void hallISR() {  //Interrupt function this is irrelevant don't touch
  uint32_t now = micros();
  uint32_t dt = now - last_pulse_us;
  if (dt > DEBOUNCE_US) {
    if (period_us == 0){
      period_us = dt;
    } 
    else {
      period_us = (period_us * 7 + dt) >> 3;
    }
    last_pulse_us = now;
  }
}

static inline uint32_t color32(uint8_t r, uint8_t g, uint8_t b) { //Color data (Ithink)
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void displayDivision(uint16_t div) {

  if (div == lastDisplayedDiv) {
    return;
  }

  
  size_t base = (size_t)div * SLICE_SIZE;

  Adafruit_DotStar* strips[2] = { &stripa, &stripb };
  const int stripIndices[2] = {inda, indb};

  for (int s = 0; s < 2; s++) {

    int idxStrip = stripIndices[s];

    // Location of the required strip inside image.bin
    size_t offset = base + idxStrip * STRIP_SIZE;

    if (offset + STRIP_SIZE > imageFile.size()) {
      Serial.println("ERROR: Outside image file");
      return;
    }

    // Jump directly to this strip's data
    if (!imageFile.seek(offset)) {
      Serial.println("ERROR: File seek");
      return;
    }

    // Read ONLY the selected strip's bytes
    size_t bytesRead = imageFile.read(stripBuffer, STRIP_SIZE);

    if (bytesRead != STRIP_SIZE) {
      Serial.println("ERROR: Size mismatch");
      return;
    }

    // Send pixels to the corresponding LED strip
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
void setup() {
  Serial.begin(115200);
  if(!LittleFS.begin(true)){
    Serial.println("Mount failed");
    while(1);
  }
  Serial.println("LittleFS mounted");
  imageFile = LittleFS.open("/image.bin","r");
  if (!imageFile){
    Serial.println("Can't open image");
    while(1);
  }
  stripa.begin(); 
  stripb.begin(); 
  stripa.setBrightness(50);
  stripb.setBrightness(50);
  stripa.show(); 
  stripb.show(); 

  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, FALLING);
  StartTime = millis();
  digitalWrite(LED_PIN, HIGH);
}

void loop() {

  if (last_pulse_us == 0 || period_us == 0)
    return;

  uint32_t now = micros();
  uint32_t elapsed = now - last_pulse_us;
  elapsed %= period_us;

  uint32_t div = (uint64_t)elapsed * DIVISIONS / period_us;
  div = (DIVISIONS - 1 - div) % DIVISIONS;
  div = (div + (uint32_t)((ANGLE_OFFSET_DEG / 360.0f) * DIVISIONS)) % DIVISIONS;

  displayDivision(div);

  //if (!interruptDetached && millis() - StartTime >= 120000) {
 // detachInterrupt(digitalPinToInterrupt(HALL_PIN));
  //digitalWrite(LED_PIN,LOW);
  //interruptDetached = true;
//}
}
