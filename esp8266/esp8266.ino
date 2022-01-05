#include "ESP8266WiFi.h"
#include "ESP8266HTTPClient.h"

#include "private.h"

const char *WIFI_NAME = "MIT";
const char *WIFI_PASS = "";

const char *START_TOKEN = "*START*";
const uint8_t START_TOKEN_LEN = 7;

const char *END_TOKEN = "*END*";
const uint8_t END_TOKEN_LEN = 5;

void setup() {
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(250);

  //Serial.println("Scanning for networks...");
  //int numNetworks = WiFi.scanNetworks();
  //Serial.print(numNetworks);
  //Serial.println(" network(s) found");
  //for (int i = 0; i < numNetworks; ++i) {
    //Serial.print("    ");
    //Serial.println(WiFi.SSID(i));
  //}

  //Serial.println("Connecting to WiFi... ");
  WiFi.begin(WIFI_NAME);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  //Serial.println();
  //Serial.print("Connected with IP address ");
  //Serial.println(WiFi.localIP());
}

void sendDiscordMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection.");
    Serial.println(END_TOKEN);
    return;
  }
  
  Serial.print("Attempting to send message: \"");
  Serial.print(message);
  Serial.println("\"...");
  
  WiFiClientSecure wifiClient;
  wifiClient.setInsecure();

  Serial.println("Setting up request...");
  HTTPClient http;
  http.begin(wifiClient, Private::WEBHOOK_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "webhook");

  Serial.println("Performing post request...");
  int httpCode = http.POST(String("{\"content\": \"") + message + "\"}");
  Serial.print("    Response code: ");
  Serial.println(httpCode);

  Serial.println("Getting response...");
  String response = http.getString();
  Serial.print("    Response: ");
  Serial.println(response);
  
  http.end();

  Serial.println(END_TOKEN);
}

char serBuffer[128];
uint8_t serBufferLen = 0;

uint8_t tokenCheckIndex = 0;

bool isRecordingData = false;

void loop() {
  if (Serial.available() > 0) {
    char currentByte = (char) Serial.read();
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
        Serial.println(START_TOKEN);
        sendDiscordMessage(dataStr);
        serBufferLen = 0;
        isRecordingData = false;
      }
    }
  }
}
