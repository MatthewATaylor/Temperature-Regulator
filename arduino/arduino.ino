#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <AltSoftSerial.h>

const char *START_TOKEN = "*START*";
const uint8_t START_TOKEN_LEN = 7;

const char *END_TOKEN = "*END*";
const uint8_t END_TOKEN_LEN = 5;

const uint8_t DHT_PIN = 2;
const uint8_t BACK_BUTTON_PIN = 3;
const uint8_t FORWARD_BUTTON_PIN = 4;
const uint8_t LCD_SWITCH_PIN = 5;
const uint8_t RELAY_PIN = 6;

const float TARGET_TEMP = 78.0f;

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT11);
AltSoftSerial swSer;

void setup() {
  Serial.begin(9600);
  swSer.begin(9600);
  
  lcd.init();
  lcd.backlight();
  lcd.print("Temperature");
  
  dht.begin();

  pinMode(BACK_BUTTON_PIN, INPUT);
  pinMode(FORWARD_BUTTON_PIN, INPUT);
  pinMode(LCD_SWITCH_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
}

const uint16_t MILLIS_BETWEEN_READINGS = 30000;
const uint16_t TEMPS_PER_RECORD = 60;  // 30 minute records 
unsigned long prevRecordTime = 0;
double tempSum = 0;
uint16_t numTemps = 0;

const uint8_t NUM_PAST_TEMPS = 24;
uint8_t numRecordedPastTemps = 0;
float pastTemps[NUM_PAST_TEMPS] = {0};

const uint16_t INPUT_DELAY_MILLIS = 250;
bool isViewingCurrentTime = true;
unsigned long prevInputTime = 0;
uint8_t currentViewedTempIndex = 0;
String lastSampledTempStr = "";

bool backlightIsOn = true;

long lastSerMessageMillis = 0;
bool dataSent = false;
bool isRecordingData = false;
char serBuffer[512];
uint16_t serBufferLen = 0;
uint8_t tokenCheckIndex = 0;

void displayCurrentTemp(float temp, bool reset) {
  if (reset) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temperature");
  }
  lcd.setCursor(0, 1);
  String tempStr = String(temp, 2);
  lcd.print(tempStr);
  lcd.print((char) 223);
  lcd.print("F");
  
  uint8_t charsPrinted = tempStr.length() + 2;
  for (uint8_t i = charsPrinted; i < 16; ++i) {
    lcd.print(" ");
  }
}

void displayPastTemp(uint8_t pastTempIndex) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Past ");
  float numHours = (pastTempIndex + 1) / 2.0f;
  lcd.print(String(numHours, 1));
  lcd.print(" Hours");
  lcd.setCursor(0, 1);
  lcd.print(String(pastTemps[pastTempIndex], 2));
  lcd.print((char) 223);
  lcd.print("F");
}

void sendSerialTemp() {
  swSer.print(START_TOKEN);
  swSer.print(lastSampledTempStr);
  swSer.print(" deg. F");
  swSer.println(END_TOKEN);
}

void loop() {
  if (dataSent) {
    if (millis() - lastSerMessageMillis > 15000) {
      Serial.println("Resending data to ESP...");
      sendSerialTemp();
      lastSerMessageMillis = millis();
      serBufferLen = 0;
      tokenCheckIndex = 0;
      isRecordingData = false;
    }
    if (swSer.available() > 0) {
      char currentByte = (char) swSer.read();
      if (!isRecordingData) {
        if (currentByte == START_TOKEN[tokenCheckIndex]) {
          ++tokenCheckIndex;
        }
        else {
          tokenCheckIndex = 0;
        }
  
        if (tokenCheckIndex == START_TOKEN_LEN) {
          tokenCheckIndex = 0;
          isRecordingData = true;
        }
      }
      else {
        serBuffer[serBufferLen] = currentByte;
        ++serBufferLen;
        if (currentByte == END_TOKEN[tokenCheckIndex]) {
          ++tokenCheckIndex;
        }
        else {
          tokenCheckIndex = 0;
        }
  
        if (tokenCheckIndex == END_TOKEN_LEN) {
          tokenCheckIndex = 0;
          uint8_t dataLen = serBufferLen - END_TOKEN_LEN;
          String dataStr = "";
          for (uint8_t i = 0; i < dataLen; ++i) {
            dataStr += serBuffer[i];
          }
          Serial.println("Received from ESP:");
          Serial.println(dataStr);
          Serial.println();
          serBufferLen = 0;
          isRecordingData = false;
          dataSent = false;
        }
      }
    }
  }
  
  if (millis() - prevRecordTime >= MILLIS_BETWEEN_READINGS || !prevRecordTime) {
    float temp = dht.readTemperature(true);
    lastSampledTempStr = String(temp, 2);
    if (isViewingCurrentTime) {
      displayCurrentTemp(temp, false);
    }

    if (!dataSent) {
      Serial.println("Sending data to ESP...");
      sendSerialTemp();
      dataSent = true;
      lastSerMessageMillis = millis();
    }
  
    tempSum += temp;
    ++numTemps;
  
    if (numTemps >= TEMPS_PER_RECORD) {
      for (uint8_t i = numRecordedPastTemps; i > 0; --i) {
        if (i < NUM_PAST_TEMPS) {
          pastTemps[i] = pastTemps[i - 1];
        }
      }
      pastTemps[0] = tempSum / numTemps;
      if (numRecordedPastTemps < NUM_PAST_TEMPS) {
        ++numRecordedPastTemps;
      }
      if (!isViewingCurrentTime) {
        displayPastTemp(currentViewedTempIndex);
      }
      tempSum = 0;
      numTemps = 0;
    }

    prevRecordTime = millis();
  }

  if (digitalRead(BACK_BUTTON_PIN) == HIGH) {
    if (millis() - prevInputTime >= INPUT_DELAY_MILLIS) {
      if (isViewingCurrentTime) {
        if (numRecordedPastTemps > 0) {
          isViewingCurrentTime = false;
          currentViewedTempIndex = 0;
          displayPastTemp(0);
        }
      }
      else {
        if (currentViewedTempIndex + 1 < numRecordedPastTemps) {
          ++currentViewedTempIndex;
          displayPastTemp(currentViewedTempIndex);
        }
      }
      prevInputTime = millis();
    }
  }
  else if (digitalRead(FORWARD_BUTTON_PIN) == HIGH) {
    if (millis() - prevInputTime >= INPUT_DELAY_MILLIS) {
      if (!isViewingCurrentTime) {
        if (currentViewedTempIndex == 0) {
          isViewingCurrentTime = true;
          float temp = dht.readTemperature(true);
          displayCurrentTemp(temp, true);
        }
        else {
          --currentViewedTempIndex;
          displayPastTemp(currentViewedTempIndex);
        }
      }
      prevInputTime = millis();
    }
  }

  if (backlightIsOn && !digitalRead(LCD_SWITCH_PIN)) {
    backlightIsOn = false;
    lcd.noBacklight();
  }
  else if (!backlightIsOn && digitalRead(LCD_SWITCH_PIN)) {
    backlightIsOn = true;
    lcd.backlight();
  }

  if (dht.readTemperature(true) < TARGET_TEMP) {
    digitalWrite(RELAY_PIN, HIGH);
  }
  else {
    digitalWrite(RELAY_PIN, LOW);
  }
}
