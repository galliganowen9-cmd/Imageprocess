#include <FS.h>
#include <Arduino.h>
#include <LittleFS.h>

#define ACK_PIN 19

uint32_t imageSize = 0;
uint32_t received = 0;

uint8_t buffer[4096];

void setup() {

  Serial.begin(1000000);

  Serial2.setRxBufferSize(65536);
  Serial2.begin(1000000, SERIAL_8N1, 16, 17);

  delay(2000);

  Serial.println("Waiting...");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }

  pinMode(ACK_PIN, OUTPUT);
  digitalWrite(ACK_PIN, LOW);

  Serial.println("Mount success");
}


void loop() {


  if (Serial2.available() < 4) {
    return;
  }

  Serial2.readBytes(
    (uint8_t *)&imageSize,
    sizeof(imageSize)
  );

  Serial.print("Incoming size: ");
  Serial.println(imageSize);


 

  File file = LittleFS.open("/image.bin", "w");

  if (!file) {
    Serial.println("Failed to open image.bin");
    return;
  }

  received = 0;




  while (received < imageSize) {

    size_t expected = min((uint32_t)4096, imageSize - received);

    size_t chunkReceived = 0;


    // Keep reading until THIS chunk is complete
    while (chunkReceived < expected) {

      int availableBytes = Serial2.available();

      if (availableBytes > 0) {

        size_t toRead = min((size_t)availableBytes, expected - chunkReceived);

        size_t n = Serial2.readBytes(buffer + chunkReceived,toRead);

        chunkReceived += n;
      }
    }




    size_t written =
      file.write(buffer, chunkReceived);

    if (written != chunkReceived) {
      Serial.println("FILE WRITE ERROR");
      file.close();
      return;
    }

    received += chunkReceived;


    Serial.print("Chunk size: ");
    Serial.println(chunkReceived);

    Serial.print("Stored: ");
    Serial.println(received);


   

    digitalWrite(ACK_PIN, HIGH);
    delay(15);
    digitalWrite(ACK_PIN, LOW);
  }



  file.close();

  File checkFile =
    LittleFS.open("/image.bin", "r");

  Serial.print("File size: ");
  Serial.println(checkFile.size());

  checkFile.close();

  Serial.println("IMAGE RECEIVED");
}
