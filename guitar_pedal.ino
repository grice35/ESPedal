/* ESP32 GUITAR PEDAL */
//
// inspired by Pedal-Pi open-source project by ElectroSmash - https://www.electrosmash.com/pedal-pi
//
// I2C LCD API - https://github.com/sstaub/LCD-I2C-HD44780.git
//
// DAC API - https://github.com/adafruit/Adafruit_MCP4725.git
//
// ESP Sleep Mode API - https://github.com/espressif/esp-idf/blob/fcae32885b0296b32044cb99ecbdc50d98dddb83/components/esp_hw_support/include/esp_sleep.h
//

#include "LCDi2c.h"
#include "Adafruit_MCP4725.h"
#include "esp_sleep.h"

#define RED 18
#define SWITCH 0
#define FOOT 21
#define LED 23
#define INPUT 4
#define TBASE 7
#define SDA 1
#define SCL 10

LCDi2c lcd;
Adafruit_MCP4725 dac;

uint16_t input_signal = 0;
RTC_DATA_ATTR uint8_t mode = 0;   // value is preserved whenever system goes into deep sleep
bool footEvent = false;
bool redEvent = false;
bool switchEvent = false;
unsigned long lastFootInterrupt = 0;
unsigned long lastRedInterrupt = 0;
unsigned long lastSwitchInterrupt = 0;

// function for switching between guitar effects
uint8_t next(uint8_t e) {
  return (e + 1) % 2;
}

// interrupt for footswitch
void IRAM_ATTR isr1() {
  unsigned long now = micros();
  if (now - lastFootInterrupt > 400000) { 
    footEvent = true;
    lastFootInterrupt = now;
  }
}

// interrupt for button
void IRAM_ATTR isr2() {
  unsigned long now = micros();
  if (now - lastRedInterrupt > 200000) { 
    redEvent = true;
    lastRedInterrupt = now;
  }
}

// interrupt for toggle switch
void IRAM_ATTR isr3() {
  unsigned long now = micros();
  if (now - lastSwitchInterrupt > 300000) { 
    switchEvent = true;
    lastSwitchInterrupt = now;
  }
}

void setup() {
  pinMode(RED, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);
  pinMode(FOOT, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  pinMode(TBASE, OUTPUT);

  attachInterrupt(FOOT, isr1, FALLING);
  attachInterrupt(RED, isr2, RISING);
  attachInterrupt(SWITCH, isr3, FALLING);

  // wakes up when switch goes high
  esp_sleep_enable_ext1_wakeup(1ULL << SWITCH, ESP_EXT1_WAKEUP_ANY_HIGH);

  Wire.begin(SDA, SCL, 1000000);

  while(!dac.begin(0x60)) {
    Serial.println("Error");
  }

  Serial.begin(9600);

  // Startup sequence
  switchEvent = !digitalRead(SWITCH);
  lcd.begin();
  lcd.printf("ESPedal");
  delay(100);
  lcd.display(DISPLAY_ON);
  lcd.display(BACKLIGHT_ON);
  delay(1000);
  
}

void loop() {
  //read input from guitar
  input_signal = analogRead(INPUT);
  if (footEvent) {
    footEvent = false;
    //foot switch toggles LED and displays current mode on LCD
    if (!digitalRead(FOOT)) {
      digitalWrite(LED, HIGH);
      lcd.cls();
      display_mode();
    } else {
      lcd.cls();
      lcd.printf("ESPedal");
      digitalWrite(LED, LOW);
    }
  }

  //Red button changes the current mode
  if (redEvent) {
    redEvent = false;
    Serial.println("RED");
    mode = next(mode);
    lcd.cls();
    display_mode();
  }
  
  //Switch toggles deep sleep mode
  if (switchEvent) {
    lcd.display(DISPLAY_OFF);
    lcd.display(BACKLIGHT_OFF);
    digitalWrite(LED, LOW);
    esp_deep_sleep_start();
  }

  if (!digitalRead(FOOT)) {

    //**** 12-BIT BITCRUSHER EFFECT ***///
    //the input signal goes directly to the 12-bit DAC output.
    if (mode == 1) {
      //Serial.println(input_signal);
      dac.setVoltage(input_signal, false);
    }

    //**** DISTORTION EFFECT ***///
    //the input signal bypasses the ESP32 through a transistor directly to the output circuit.
    if (mode == 0) {
      digitalWrite(TBASE, HIGH);
    } else {
      digitalWrite(TBASE, LOW);
    }
  }
}

void display_mode() {
  //display effect mode to LCD screen
    switch (mode) {
      case 0:
        lcd.printf("DISTORTION");
        break;
      case 1:
        lcd.printf("12-BIT");
        break;
      default:
        lcd.printf("ERROR");
    }
}
