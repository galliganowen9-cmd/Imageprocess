#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>

//#include "image_data_1.h"

#include "image_data_barc_389div.h" //Image included here

#define NUM_LEDS    129
#define DIVISIONS   300
#define STRIPS      6
#define LED_PIN 2
#define HALL_PIN    27
const uint8_t DATA_PIN_1 = 21;
const uint8_t DATA_PIN_2 = 22;
const uint8_t DATA_PIN_3 = 19;
const uint8_t DATA_PIN_4 = 25;
const uint8_t DATA_PIN_5 = 33;
const uint8_t DATA_PIN_6 = 32; //Pins initialized for fan blades

const uint8_t CLK_PIN    = 18; 
unsigned long StartTime;
bool interruptDetached = false;
//Interrupt pin initialized
Adafruit_DotStar strip1(NUM_LEDS, DATA_PIN_1, CLK_PIN, DOTSTAR_BRG);
Adafruit_DotStar strip2(NUM_LEDS, DATA_PIN_2, CLK_PIN, DOTSTAR_BRG);
Adafruit_DotStar strip3(NUM_LEDS, DATA_PIN_3, CLK_PIN, DOTSTAR_BRG);
Adafruit_DotStar strip4(NUM_LEDS, DATA_PIN_4, CLK_PIN, DOTSTAR_BRG);
Adafruit_DotStar strip5(NUM_LEDS, DATA_PIN_5, CLK_PIN, DOTSTAR_BRG);
Adafruit_DotStar strip6(NUM_LEDS, DATA_PIN_6, CLK_PIN, DOTSTAR_BRG);

const size_t SLICE_SIZE = STRIPS*NUM_LEDS*3;
uint8_t sliceBuffer[SLICE_SIZE];
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
//Im not sure what this one does
void displayDivision(uint16_t div) { 
  //div %= DIVISIONS;
  if (div == lastDisplayedDiv){
    return;
  }
  lastDisplayedDiv = div;
  size_t base = (size_t)div * SLICE_SIZE;

  if(base + SLICE_SIZE > imageFile.size()){
    Serial.println("ERROR division outside image file");
    return;
  }
  imageFile.seek(base);

  size_t bytesRead = imageFile.read(sliceBuffer, SLIZE_SIZE);
  if (bytesRead != SLICE_SIZE){
    Serial.print("error size mismatch")
    Serial.println(SLICE_SIZE);
    Serial.println(bytesRead);
    return;
  }



  Adafruit_DotStar* strips[STRIPS] = { &strip1, &strip2, &strip3, &strip4, &strip5, &strip6 };

//TODO change this function to skip the bytes not needed
  for (int s = 0; s < STRIPS; s++) {
    if (s != 0 && s != 3) {//THIS SKIPS THE UNESSECARY BYTES PLEASE HOLD ON TO THIS
        continue;
    }
    size_t idx = base + s * (NUM_LEDS * 3);
    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t r = sliceBuffer[idx + i*3 + 0]; //First use of "readImageByte"
      uint8_t g = sliceBuffer[idx + i*3 + 1];
      uint8_t b = sliceBuffer[idx + i*3 + 2];
      strips[s]->setPixelColor(i, color32(g, r, b));
    }
    strips[s]->show();
  }
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
  strip1.begin(); 
  strip2.begin(); 
  strip3.begin(); 
  strip4.begin(); 
  strip5.begin(); 
  strip6.begin();
  strip1.show(); 
  strip2.show(); 
  strip3.show(); 
  strip4.show(); 
  strip5.show(); 
  strip6.show();
  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, FALLING);
  lastSwitch = millis();
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
