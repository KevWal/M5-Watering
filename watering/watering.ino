#include <M5StickC.h>
#include <Wire.h>

#define INPUT_PIN 33
#define PUMP_PIN 32

#define I2C_SDA 0
#define I2C_SCL 26
#define SHT_ADDRESS 0x44


// M5 Stick C LCD is 80 x 160

int rawADC; // Value read from ADC
RTC_DATA_ATTR int waterADC = 1900; // ADC Level at which we turn on watering, save in RTC memory to keep across a deep sleep


// Shutdown the SH200Q IMU, not controlled by the AXP
// https://github.com/eggfly/M5StickCProjects/blob/master/demo/StickWatch2PowerManagment/StickWatch2PowerManagment.ino
void shutdownSH200Q() {
  Wire.beginTransmission(0x6C);
  Wire.write(0x75);
  byte success = Wire.endTransmission(false);
  //Serial.printf("success: %d \n", success);
  byte byteCount = Wire.requestFrom(0x6C, 1);
  //Serial.printf("byteCount: %d \n", byteCount);
  uint8_t data = Wire.read();
  // uint8_t b = read_register(0x6C, 0x0E);
  //Serial.print("byte: ");
  //Serial.println(data, BIN);

  Wire.beginTransmission(0x6C);
  Wire.write(0x75);
  Wire.write(0x80);  // SH200Q shutdown mode (only i2c alive)
  byte succ = Wire.endTransmission();
  //Serial.printf("succ: %d \n", succ);
}


// https://github.com/eggfly/M5StickCProjects/blob/master/demo/StickWatch2PowerManagment/StickWatch2PowerManagment.ino
void shutdown_all_except_self() {
  Wire.beginTransmission(0x34);
  Wire.write(0x12);
  Wire.write(0x01);
  Wire.endTransmission();
}


// Convert Battery Voltage to Battery %
// Thanks to  https://github.com/eggfly/M5StickCProjects/blob/master/StickWatch2/battery.h
static const float levels[] = {4.14, 4.06, 3.98, 3.92, 3.87, 3.82, 3.79, 3.77, 3.74, 3.68, 3.45, 3.00};
int getBatteryLevel(float voltage) {
  float level = 1;
  if (voltage >= levels[0]) {
    level = 1; // Maximum output of 100%
  } else if (voltage >= levels[1]) {
    level = 0.9;
    level += 0.1 * (voltage - levels[1]) / (levels[0] - levels[1]);
  } else if (voltage >= levels[2]) {
    level = 0.8;
    level += 0.1 * (voltage - levels[2]) / (levels[1] - levels[2]);
  } else if (voltage >= levels[3]) {
    level = 0.7;
    level += 0.1 * (voltage - levels[3]) / (levels[2] - levels[3]);
  } else if (voltage >= levels[4]) {
    level = 0.6;
    level += 0.1 * (voltage - levels[4]) / (levels[3] - levels[4]);
  } else if (voltage >= levels[5]) {
    level = 0.5;
    level += 0.1 * (voltage - levels[5]) / (levels[4] - levels[5]);
  } else if (voltage >= levels[6]) {
    level = 0.4;
    level += 0.1 * (voltage - levels[6]) / (levels[5] - levels[6]);
  } else if (voltage >= levels[7]) {
    level = 0.3;
    level += 0.1 * (voltage - levels[7]) / (levels[6] - levels[7]);
  } else if (voltage >= levels[8]) {
    level = 0.2;
    level += 0.1 * (voltage - levels[8]) / (levels[7] - levels[8]);
  } else if (voltage >= levels[9]) {
    level = 0.1;
    level += 0.1 * (voltage - levels[9]) / (levels[8] - levels[9]);
  } else if (voltage >= levels[10]) {
    level = 0.05;
    level += 0.05 * (voltage - levels[10]) / (levels[9] - levels[10]);
  } else if (voltage >= levels[11]) {
    level = 0.00;
    level += 0.05 * (voltage - levels[11]) / (levels[10] - levels[11]);
  } else {
    level = 0.00;
  }
  return level * 100;
}


// Print ADC Value
void outputADC(int rawADC) {
  M5.Lcd.drawString(" ADC: ", 40, 63); // X, Y, Font
  M5.Lcd.drawString(String(rawADC), 40, 80); 
  
  // Serial.print("ADC value: ");
  // Serial.println(rawADC);
}


// Print Battery / Charge Value
void outputBatt() {
  if (M5.Axp.GetBatChargeCurrent() > 1) {
    //M5.Lcd.drawString("Chrg:", 40, 123); 
    M5.Lcd.drawString("C " + String(int(M5.Axp.GetBatChargeCurrent())) + "mA", 40, 140);  
  } else {
    //M5.Lcd.drawString("Batt:", 40, 123); 
    M5.Lcd.drawString("B " + String(getBatteryLevel(M5.Axp.GetBatVoltage())) + "%", 40, 140);  
  }
}


void outputTemperature() {
  unsigned int data[6];
  float temp = 0.0;

  // Startup I2C for the SHT30 temperature sensor - I2C_SDA = 0, I2C_SCL = 26.
  Wire.begin(I2C_SDA,I2C_SCL);

  // Start I2C Transmission
  Wire.beginTransmission(SHT_ADDRESS);
  // Send measurement command
  Wire.write(0x2C);
  Wire.write(0x06);
  // Stop I2C transmission
  // stat1 = 
  Wire.endTransmission();

  delay(500);

  // Request 6 bytes of data
  Wire.requestFrom(SHT_ADDRESS, 6);

  // Read 6 bytes of data
  // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  for (int i=0;i<6;i++) {
    data[i]=Wire.read();
  };

  delay(50);

  // Should be 0
  // stat2 = Wire.available();

  // Convert the data
  temp = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;

  //M5.Lcd.setCursor(0, 10, 106);
  //M5.Lcd.printf("T %2.1f", temp);
  M5.Lcd.drawString("T " + String(temp,1), 40, 123);  
}


// Blink the inbuild LED
void blinkLED(void * parameter) {
  while (true) {
    digitalWrite(M5_LED, LOW); // LED On
    //delay(100);
    vTaskDelay(500 / portTICK_PERIOD_MS);  // Pause the task for 500ms
    digitalWrite(M5_LED, HIGH); // LED On
    //delay(100);
    vTaskDelay(500 / portTICK_PERIOD_MS); // Pause the task for 500ms
  }
}

void myDeepSleep(int seconds) {
  // ToDo What else can we turn off here to save power?
  shutdownSH200Q(); // Shutdown the IMU (not controlled by the AXP)
  shutdown_all_except_self(); 
  M5.Axp.DeepSleep(SLEEP_SEC(seconds)); // Button A will wake us up straight away
}

void myLightSleep(int seconds) {
  M5.Axp.SetLDO2(false); // turn off power to LCD
  M5.Axp.LightSleep(SLEEP_SEC(seconds)); // Button A will wake us up straight away
  M5.Axp.SetLDO2(true); // turn on power to LCD
}


// Run every time after a deep sleep
void setup() { 

  setCpuFrequencyMhz(80); // Reduce CPU frequency to save power, default 240Mhz?
  
  // Replace with specific commands we need to save power
  //M5.begin();

  //Serial.begin(115200);
  // Don't power up RTC nor Microphone
  // M5.Axp.begin(disableLDO2, disableLDO3, disableRTC, disableDCDC1, disableDCDC3, disableLDO0);
  // LDO1 - RTC, LDO2 - LCD BackLight, LDO3 - LCD Logic, DCDC1 - ESP 3V3 & MPU6886, DCDC3 - ??, GPIO0 (LDO0) - Mic
  M5.Axp.begin(false, false, true, false, false, true);
  M5.Lcd.begin();
  shutdownSH200Q(); // Shutdown the IMU (not controlled by the AXP)  SH200Q or MPU6886?
  //M5.Rtc.begin();

  // M5.axp.EnableCoulombcounter(); // Doesnt seem to work, perhaps not saved across a deep sleep?

  M5.Lcd.setRotation(2);
  M5.Lcd.setTextSize(2); // 1 = smallest
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.setTextDatum(TC_DATUM); // Top Centre - Sets where in the text the drawString x, y position refers to
  M5.Lcd.drawString("Water", 40, 10); // X, Y, Font
 
  pinMode(INPUT_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH); // Make sure LED is off
  
  //pinMode(25, OUTPUT);  // G25 is shared with G36, but not sure why they were set low here?
  //digitalWrite(25, 0);
}


void loop() { 

  // Print battery state
  outputBatt();
  outputTemperature();

  // If rawADC 10% above or below waterADC keep in built red LED on
  bool ledOn = false;
  rawADC = analogRead(INPUT_PIN);
  if (rawADC > (waterADC * 1.1) || rawADC < (waterADC * 0.9)) {
    digitalWrite(M5_LED, LOW); // LED On
    ledOn = true;
  } else {
    digitalWrite(M5_LED, HIGH); // LED Off
    ledOn = false;
  }
  
  // If battery level low (and LED not already on solid) then blink in built red LED
  if (getBatteryLevel(M5.Axp.GetBatVoltage()) < 15 && !ledOn) {
    // digitalWrite(M5_LED, LOW); // LED On
    // Flash LED through seperate task that runs in the background
    // xTaskCreate(Function, Name, Stack, Parameter, Priority, Handle)
    TaskHandle_t blinkTask;
    xTaskCreate(blinkLED, "Blink LED", 1000, NULL, 1, &blinkTask);
  }

  // If Button B is pressed, display current waterADC, then after 2 seconds reset waterADC value and increase by 20 every 200ms until button released
  bool first = true; // First time through the Reset Watering ADC value loop
  bool second = false; // Second time through the Reset Watering ADC value loop
  bool third = false; // Third time or greater through the Reset Watering ADC value loop
  bool reset = false; // Have we reset waterADC this loop()?
  while(M5.BtnB.read()){
    // Third or greater time through this loop increment waterADC and display
    if (third) {
      waterADC += 10;  // Every time through this loop (whilst button still pressed) increase waterADC value and display
      M5.Lcd.drawString(" Set: ", 40, 63); // X, Y, Font
      M5.Lcd.drawString(String(waterADC), 40, 80);
      delay(250); 
    }
    // Second time through this loop reset waterADC to lowest sensible value
    if (second) {
      waterADC = 1390;
      reset = true;
      second = false;
      third = true;
    }
    // First time through this loop print current value of waterADC, and pause for 2 seconds to give user chance to release the button and not reset it.
    if (first) { 
      M5.Lcd.drawString("Curr:", 40, 63); // X, Y, Font
      M5.Lcd.drawString(String(waterADC), 40, 80);
      delay(2000); // Leave current value printed for 2 seconds
      first = false;
      second = true;
    }
  }
  if (reset) { // If we have just set waterADC, leave it displayed for 2 seconds.
    delay(2000);
  }


  // If water is needed, turn pump on, with a limited maximum time watering
  int timeWatering = 0; // How long have we been watering for this loop?
  bool water = true; // Remain in the Watering loop?
  while (water) {
    
    rawADC = analogRead(INPUT_PIN);
    outputADC(rawADC);
    
    if (rawADC > waterADC) {
      digitalWrite(PUMP_PIN, HIGH);
    } else {
      digitalWrite(PUMP_PIN, LOW);
      water = false;
    } 

    timeWatering += 5;
    // Leave watering on for a maximum of 50 10th s each time we wake up
    if (timeWatering > 50) {
      digitalWrite(PUMP_PIN, LOW);
      water = false;
    }
    
    delay(500); // Test water level and display every Xms
  }

  // Update printed battery state
  outputBatt();
  outputTemperature();

  // Leave display on for X ms so it can be read
  delay(2000);
  
  // Go to sleep until button is pressed or X seconds, whichever is sooner
  myDeepSleep(10);
}
