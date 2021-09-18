#include <M5StickC.h>

#define INPUT_PIN 33
#define PUMP_PIN 32

// M5 Stick C LCD is 80 x 160

int rawADC; // Value read from ADC
RTC_DATA_ATTR int waterADC = 2000; // ADC Level at which we turn on watering

// Run every time after a deep sleep
void setup() { 
  M5.begin();

  setCpuFrequencyMhz(80); // Reduce CPU frequency to save power, default 240Mhz?

  M5.axp.EnableCoulombcounter();

  M5.Lcd.setTextSize(2); // 1 = smallest
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.setTextDatum(TC_DATUM); // Text Top Centre Datum - Where in the text should the x, y position refer to
  M5.Lcd.drawString("Water", 40, 10); // X, Y, Font
 
  pinMode(INPUT_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);
  
  //pinMode(25, OUTPUT);  // G25 is shared with G36, but not sure why they were set low here?
  //digitalWrite(25, 0);
}

// Convert Battery Voltage to Battery %
static const float levels[] = {4.13, 4.06, 3.98, 3.92, 3.87, 3.82, 3.79, 3.77, 3.74, 3.68, 3.45, 3.00};
int getBatteryLevel(float voltage) {
  float level = 1;
  if (voltage >= levels[0]) {
    level = 1;
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
  M5.Lcd.drawString("ADC:", 40, 63); // X, Y, Font
  M5.Lcd.drawString(String(rawADC), 40, 80); 
  
  // Serial.print("ADC value: ");
  // Serial.println(rawADC);
}


// Print Battery / Charge Value
void outputBatt() {
  if (M5.Axp.GetBatChargeCurrent() > 1) {
    M5.Lcd.drawString("Chrg:", 40, 123); 
    M5.Lcd.drawString(String(int(M5.Axp.GetBatChargeCurrent())) + "mA", 40, 140);  
  } else {
    M5.Lcd.drawString("Batt:", 40, 123); 
    M5.Lcd.drawString(String(getBatteryLevel(M5.Axp.GetBatVoltage())) + "%", 40, 140);  
  }
}


void loop() { 

  // Print battery state
  outputBatt();
  
  // TODO Red led on if GetWarningLevel is 2, flash if 1?
  //digitalWrite(M5_LED, LOW); // LED On
  //delay(2000);
  //digitalWrite(M5_LED, HIGH); // LED Off

  // If Button B is pressed, reset waterADC value and increase by 20 every 200ms until button released
  int first = true; // First time through the Reset Watering ADC value loop?
  while(M5.BtnB.read()){
    if (first) { // First time through this loop reset to 490
      waterADC = 490;
      first = false;
    }
    waterADC += 20;  // Every time through this loop (whilst button still pressed) increase waterADC value and display
    // ToDo This is not saved, as deep sleep looses it!!!!
    M5.Lcd.drawString("Set:", 40, 63); // X, Y, Font
    M5.Lcd.drawString(String(waterADC), 40, 80);
    delay(200); 
  }
  if (!first) { // If we have just set waterADC, leave it displayed for 2 seconds.
    delay(2000);
  }

  // If water is needed, turn pump on, with a limited maximum time watering
  int timeWatering = 0; // How long have we been watering for this loop?
  int water = true; // Remain in the Watering loop?
  while (water) {
    rawADC = analogRead(INPUT_PIN);
    outputADC(rawADC);
    if (rawADC < waterADC) {
      digitalWrite(PUMP_PIN, HIGH);
    } else {
      digitalWrite(PUMP_PIN, LOW);
      water = false;
    } 

    timeWatering += 500;
    // Leave watering on for a maximum of X ms each time we wake up
    if (timeWatering > 2000) {
      digitalWrite(PUMP_PIN, LOW);
      water = false;
    }
    
    delay(500); // Test and display the ADC reading every Xms
  }

  // Update printed battery state
  outputBatt();

  delay(2000);

  // ToDo What else can we turn off here to save power?
  M5.Axp.DeepSleep(SLEEP_SEC(5)); // Button A will wake us up straight away
  
}
