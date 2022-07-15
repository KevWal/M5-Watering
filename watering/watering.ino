// Have to be defined before the #includes
#define DEBUG_ESP_PORT Serial

//#define NODEBUG_WEBSOCKETS
//#define NDEBUG
//#define NODEBUG_SINRIC


#include <M5StickC.h>
#undef min // Workaround https://github.com/m5stack/M5Stack/issues/97
#include <Wire.h>
#include <WiFi.h>
#include <SinricPro.h>
#include "TemperatureAlert.h"


#define INPUT_PIN 33 // Water Sensor
#define PUMP_PIN 32  // Water Pump

#define I2C_SDA 0
#define I2C_SCL 26
#define SHT_ADDRESS 0x44 // SHT30 Temperature Sensor

// WIFI_SSID & WIFI_PASS defined in an external file excluded from Github
// Also used for SinricPro APP_KEY, APP_SECRET & TEMP_SENSOR_ID
#include "ssid.h"

// M5 Stick C LCD is 80 x 160
#define LINE1 5
#define LINE2 22
#define LINE3 39

#define LINE4 63
#define LINE5 80

#define LINE6 106
#define LINE7 123
#define LINE8 140


// Global Variables
int rawADC; // Value read from ADC
bool wifiConnected = false;
bool sinricConnected = false;
RTC_DATA_ATTR int waterADC = 1900; // ADC Level at which we turn on watering, save in RTC memory to keep across a deep sleep


// -------------------------------------------
// setup() routines

// Shutdown the SH200Q IMU, not controlled by the AXP
// https://github.com/eggfly/M5StickCProjects/blob/master/demo/StickWatch2PowerManagment/StickWatch2PowerManagment.ino
void shutdownSH200Q() {
  Serial.printf("KW shutdownSH200Q(): Start\r\n");
  Wire.beginTransmission(0x6C);
  Wire.write(0x75);
  byte success = Wire.endTransmission(false);
  Serial.printf("  Wire.endTransmission() success: %d \n", success);
  byte byteCount = Wire.requestFrom(0x6C, 1);
  Serial.printf("  Wire.requestFrom() byteCount: %d \n", byteCount);
  uint8_t data = Wire.read();
  // uint8_t b = read_register(0x6C, 0x0E);
  Serial.print("  Wire.read() byte: ");
  Serial.println(data, BIN);

  Wire.beginTransmission(0x6C);
  Wire.write(0x75);
  Wire.write(0x80);  // SH200Q shutdown mode (only i2c alive)
  byte succ = Wire.endTransmission();
  Serial.printf("  Wire.endTransmission() succ: %d \n", succ);
  Serial.printf("KW shutdownSH200Q(): Complete\r\n");
}


// setup function for WiFi connection
void setupWiFi() {
  Serial.printf("KW setupWiFi(): Start\r\n");
  int loop_count = 0;
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    loop_count += 1;
    Serial.printf("w");
    M5.Lcd.drawString("WiFi", 40, LINE2);
    delay(250);
    M5.Lcd.drawString("    ", 40, LINE2);
    delay(250);

    // If we don't get WiFi within 100 loops give up and move on
    if (loop_count >= 100) {
      Serial.printf(" No WiFi!\r\n");
      M5.Lcd.drawString("No WiFi!\r\n", 40, LINE2);
      return; // Exit setupWiFi
    }
  }
  Serial.printf(" WiFi Connected.\r\n");
  wifiConnected = true;
  IPAddress localIP = WiFi.localIP();
  Serial.printf("KW setupWiFi(): IP-Address is %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
  M5.Lcd.drawString("." + String(localIP[3]), 40, LINE2);

  Serial.printf("KW setupWiFi(): Complete\r\n");
}

// Comms with SinricPro through seperate task that runs in the background
void handle(void * parameter) {
  while (true) {
    SinricPro.handle();
    vTaskDelay(500 / portTICK_PERIOD_MS);  // Pause the task for 500ms
  }
}


// SinricPro has to be able to send a device on / off message, we just ignore it
bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Temperaturesensor turned %s (via SinricPro) \r\n", state?"on":"off");
  return true; // request handled properly
}


// setup function for SinricPro
void setupSinricPro() {
  int loop_count = 0;
  
  Serial.printf("KW setupSinricPro(): Start\r\n");
  // add device to SinricPro
  TemperatureAlert &temperatureAlert = SinricPro[DEVICE_ID];
  //temperatureAlert.onPowerState(onPowerState);
  
  SinricPro.onConnected([](){ sinricConnected = true;}); 
  SinricPro.onDisconnected([](){ Serial.printf("  Disconnected from SinricPro!\r\n"); sinricConnected = false;});
  //SinricPro.restoreDeviceStates(true);
  SinricPro.begin(APP_KEY, APP_SECRET);

  //mySensor.sendPowerStateEvent(1, "Awake");

  // Comms with SinricPro through seperate task that runs in the background
  // xTaskCreate(Function, Name, Stack, Parameter, Priority, Handle)
  xTaskCreate(handle, "SinricPro Handle", 20000, NULL, 1, NULL);

  // If we dont have a connection to WiFi no point waiting here
  if (!wifiConnected) { loop_count = 100; }

  while(!sinricConnected) {
    loop_count += 1;
    Serial.printf("s");
    M5.Lcd.drawString("Sinric", 40, LINE3);
    delay(500);
    M5.Lcd.drawString("      ", 40, LINE3);
    delay(500);

    // If we don't connect within 100 loops give up and move on
    if (loop_count >= 100) {
      Serial.printf(" NOT Connected!\r\n");
      M5.Lcd.drawString("N Cnct!", 40, LINE3);
      return; // Exit upload
    }
  }
  
  Serial.printf(" Connected.\r\n");
  M5.Lcd.drawString("Cnctd", 40, LINE3);
  
  Serial.printf("KW setupSinricPro(): Complete\r\n");
}


// Run every time after a deep sleep
void setup() { 

  setCpuFrequencyMhz(80); // Reduce CPU frequency to save power, default 240Mhz?
  
  // Replaced with specific commands we need to save power
  // M5.begin(); // Calls Serial.begin, Axp.begin and LCD.begin

  Serial.begin(115200);

  // LDO2: Display backlight. LDO3: Display Control. RTC: Always ON, Switch RTC charging. DCDC1: Main rail - when not set the controller shuts down. DCDC3: Use unknown.  LDO0: MIC
  // begin(bool disableLDO2 = false, bool disableLDO3 = false, bool disableRTC = false, bool disableDCDC1 = false, bool disableDCDC3 = false, bool disableLDO0 = false);
  // Don't power up Microphone
  M5.Axp.begin(false, false, false, false, false, true);
  
  M5.Lcd.begin();
  // Lower LCD brightness
  M5.Axp.ScreenBreath(10);
  
  //shutdownSH200Q(); // Shutdown the IMU (not controlled by the AXP)  SH200Q or MPU6886?
  //M5.Rtc.begin();

  // M5.axp.EnableCoulombcounter(); // Doesnt seem to work, perhaps not saved across a deep sleep?

  M5.Lcd.setRotation(2); // Vertical, USB C at top
  M5.Lcd.setTextSize(2); // 1 = smallest
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.setTextDatum(TC_DATUM); // Top Centre - Sets where in the text the drawString x, y position refers to
  M5.Lcd.drawString("Water", 40, LINE1); // X, Y, Font
 
  pinMode(INPUT_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH); // Make sure LED is off

  // Print battery state
  outputBatt();
  
  // Connect to WiFi
  setupWiFi();

  // SinricPro reports to Alexa
  setupSinricPro();
  
  //pinMode(25, OUTPUT);  // G25 is shared with G36, but not sure why they were set low here?
  //digitalWrite(25, 0);

  Serial.printf("KW setup(): Complete\r\n");
}




// -------------------------------------------
// loop() routines


// Get and Print Battery / Charge Value
void outputBatt() {
  if (M5.Axp.GetBatChargeCurrent() > 1) {
    //M5.Lcd.drawString(" Chrg: ", 40, LINE7); 
    M5.Lcd.drawString("C " + String(int(M5.Axp.GetBatChargeCurrent())) + "mA", 40, LINE8);  
  } else {
    //M5.Lcd.drawString(" Batt: ", 40, LINE7); 
    M5.Lcd.drawString("B " + String(getBatteryLevel(M5.Axp.GetBatVoltage())) + "%", 40, LINE8);  
  }
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


// Get, Print and send to Alexa Temperature Value
void outputTemperature() {
  unsigned int data[6];
  float temp = 0.0;
  float humid = 0.0;

  Serial.printf("KW outputTemperature(): Start\r\n");
  
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
  humid = ((((data[3] * 256.0) + data[4]) * 100) / 65535.0);

  //M5.Lcd.setCursor(0, 10, 106);
  //M5.Lcd.printf("T %2.1f", temp);
  M5.Lcd.drawString("T " + String(temp,1), 40, LINE7);  

  uploadTemperature(temp, humid);
   
  Serial.printf("KW outputTemperature(): Complete\r\n");
}

// Upload temperature to Sinric
void uploadTemperature(float temp, float humid) {
  Serial.printf("KW uploadTemperature(): Start\r\n");
  int loop_count = 0;

  // If we dont have a connection to the SinrcPro Server no point waiting here
  if (!sinricConnected) { loop_count = 100; }
  
  TemperatureAlert &temperatureAlert = SinricPro[DEVICE_ID];  // get temperaturesensor device
  
  while (!temperatureAlert.sendTemperatureEvent(temp, humid)) {
    loop_count += 1;
    Serial.printf("t");
    M5.Lcd.drawString("Temp", 40, LINE6);
    delay(500);
    M5.Lcd.drawString("      ", 40, LINE6);
    delay(500);

    // If we don't send within 100 loops give up and move on
    if (loop_count >= 100) {
      Serial.printf(" NOT Sent!\r\n");
      M5.Lcd.drawString("N Sent!", 40, LINE6);
      return; // Exit upload
    }
  }
  
  Serial.printf(" Sent.\r\n");
  M5.Lcd.drawString("T Sent", 40, LINE6);


  if (temp > 30) {
    Serial.printf("Temperature Alert Sent.\r\n");
    temperatureAlert.sendContactEvent(true);
  } else {
    temperatureAlert.sendContactEvent(false);
  }

  
  Serial.printf("KW uploadTemperature(): Complete\r\n");
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


// Print ADC Value
void outputADC(int rawADC) {
  M5.Lcd.drawString(" ADC: ", 40, LINE4); // X, Y, Font
  M5.Lcd.drawString(" " + String(rawADC) + " ", 40, LINE5); 
  
  // Serial.print("ADC value: ");
  // Serial.println(rawADC);
}


// https://github.com/eggfly/M5StickCProjects/blob/master/demo/StickWatch2PowerManagment/StickWatch2PowerManagment.ino
void shutdown_all_except_self() {
  Wire.beginTransmission(0x34);
  Wire.write(0x12);
  Wire.write(0x01);
  Wire.endTransmission();
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


// Main Loop
void loop() { 
  Serial.printf("KW loop(): Start\r\n");
  
  outputTemperature();

  // If rawADC 10% above or below waterADC keep in built red LED on
  bool ledOn = false;
  //rawADC = analogRead(INPUT_PIN);
  //if (rawADC > (waterADC * 1.1) || rawADC < (waterADC * 0.9)) {
  //  digitalWrite(M5_LED, LOW); // LED On
  //  ledOn = true;
  //} else {
  //  digitalWrite(M5_LED, HIGH); // LED Off
  //  ledOn = false;
  //}
  
  // If battery level low (and LED not already on solid) then blink in built red LED
  if (getBatteryLevel(M5.Axp.GetBatVoltage()) < 15 && !ledOn) {
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
      M5.Lcd.drawString(" Set: ", 40, LINE4); // X, Y, Font
      M5.Lcd.drawString(String(waterADC), 40, LINE5);
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
      M5.Lcd.drawString("Curr:", 40, LINE4); // X, Y, Font
      M5.Lcd.drawString(String(waterADC), 40, LINE5);
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

  // Leave display on for X ms so it can be read
  delay(5000);

  //SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  //mySensor.sendPowerStateEvent(0, "Deep_Sleep");

  Serial.printf("KW loop(): Complete\r\n");

  // Go to sleep until button is pressed or X seconds, whichever is sooner
  myDeepSleep(30);
}
