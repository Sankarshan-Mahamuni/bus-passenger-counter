/*
  esp32_bus_counter.ino
  Simulated Bus Passenger Counter (for Wokwi)
  - Two buttons simulate beam sensors (A outer, B inner)
  - A->B within window => ENTRY, B->A => EXIT
  - OLED displays count, LEDs indicate outside status
  - Periodically POSTs JSON {count, capacity} to FLASK_URL
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// --------- CONFIG ----------
const char* ssid = "Skyworth_SSM";        // not required for Wokwi preview
const char* password = "AllIndiaCup@2399";

String FLASK_URL = "http://13.220.91.134:5000/update_count"; // update before running
const String THINGSPEAK_KEY = ""; // server handles TS; not used here

const int SENSOR_A_PIN = 32; // outer beam (button)
const int SENSOR_B_PIN = 33; // inner beam
const int LED_GREEN = 2;
const int LED_RED = 15;

const int MAX_CAPACITY = 40;
volatile int current_count = 0;

unsigned long lastEventTime = 0;
const unsigned long DEBOUNCE_MS = 60;
const unsigned long SEQ_WINDOW_MS = 600; // ms between A and B for single crossing
const unsigned long POST_INTERVAL_MS = 8000; // post every 8s

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

enum State { IDLE, A_TRIGGERED, B_TRIGGERED };
State stateMachine = IDLE;
unsigned long triggerTime = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(SENSOR_A_PIN, INPUT_PULLUP);
  pinMode(SENSOR_B_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (WiFi.status() == WL_CONNECTED) break;
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected (OK for Wokwi demo)");
  }

  updateDisplay();
  updateLEDs();
}

void loop() {
  bool a = (digitalRead(SENSOR_A_PIN) == LOW);
  bool b = (digitalRead(SENSOR_B_PIN) == LOW);
  unsigned long now = millis();

  switch (stateMachine) {
    case IDLE:
      if (a) { stateMachine = A_TRIGGERED; triggerTime = now; }
      else if (b) { stateMachine = B_TRIGGERED; triggerTime = now; }
      break;
    case A_TRIGGERED:
      if (b && (now - triggerTime) <= SEQ_WINDOW_MS) {
        handleEntry();
        stateMachine = IDLE;
      } else if ((now - triggerTime) > SEQ_WINDOW_MS) {
        stateMachine = IDLE;
      }
      break;
    case B_TRIGGERED:
      if (a && (now - triggerTime) <= SEQ_WINDOW_MS) {
        handleExit();
        stateMachine = IDLE;
      } else if ((now - triggerTime) > SEQ_WINDOW_MS) {
        stateMachine = IDLE;
      }
      break;
  }

  static unsigned long lastPost = 0;
  if (now - lastPost >= POST_INTERVAL_MS) {
    lastPost = now;
    postCountToServer();
  }

  delay(10);
}

void handleEntry() {
  unsigned long now = millis();
  if (now - lastEventTime < DEBOUNCE_MS) return;
  lastEventTime = now;

  if (current_count < MAX_CAPACITY) {
    current_count++;
    Serial.printf("ENTRY -> count=%d\n", current_count);
    updateDisplay();
    updateLEDs();
  } else {
    Serial.println("ENTRY blocked: Bus Full");
  }
}

void handleExit() {
  unsigned long now = millis();
  if (now - lastEventTime < DEBOUNCE_MS) return;
  lastEventTime = now;

  if (current_count > 0) {
    current_count--;
    Serial.printf("EXIT -> count=%d\n", current_count);
    updateDisplay();
    updateLEDs();
  } else {
    Serial.println("EXIT ignored: count already zero");
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.printf("Bus Occupancy\n");
  display.printf("Count: %d/%d\n", current_count, MAX_CAPACITY);
  if (current_count >= MAX_CAPACITY) display.printf("Status: FULL\n");
  else display.printf("Status: SPACE %d\n", MAX_CAPACITY - current_count);
  display.display();
}

void updateLEDs() {
  if (current_count >= MAX_CAPACITY) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
  } else {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
  }
}

void postCountToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected: skip POST");
    return;
  }
  if (FLASK_URL.indexOf("REPLACE_WITH_YOUR_SERVER_OR_NGROK") >= 0) {
    Serial.println("FLASK_URL not set - update sketch with server URL");
    return;
  }
  HTTPClient http;
  http.begin(FLASK_URL);
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"count\":" + String(current_count) + ",\"capacity\":" + String(MAX_CAPACITY) + "}";
  int code = http.POST(payload);
  if (code > 0) {
    Serial.printf("POST code=%d\n", code);
  } else {
    Serial.printf("POST failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}
