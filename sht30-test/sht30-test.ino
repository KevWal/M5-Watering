
#include <M5StickC.h>
#include <Wire.h>


float temp = 0.0;
float stat1, stat2 = 0.0;

void setup() {
  // put your setup code here, to run once:

  M5.begin();
  Wire.begin(0,26);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.println("ENV TEST");
  pinMode(M5_BUTTON_HOME, INPUT);

}

void loop() {
  // put your main code here, to run repeatedly:

  unsigned int data[6];

  // Start I2C Transmission
  Wire.beginTransmission(0x44);
   // Send measurement command
  Wire.write(0x2C);
  Wire.write(0x06);
  // Stop I2C transmission
  stat1 = Wire.endTransmission();

  delay(500);

  // Request 6 bytes of data
  Wire.requestFrom(0x44, 6);

  // Read 6 bytes of data
  // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  for (int i=0;i<6;i++) {
    data[i]=Wire.read();
  };

  delay(50);

  // Should be 0
  stat2 = Wire.available();

  // Convert the data
  temp = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;

  M5.Lcd.setCursor(0, 20, 2);
  M5.Lcd.printf("Temp: %2.1f", temp);
  M5.Lcd.setCursor(0, 40, 2);
  M5.Lcd.printf("St1: %2.0f St2: %2.0f", stat1, stat2);

}
