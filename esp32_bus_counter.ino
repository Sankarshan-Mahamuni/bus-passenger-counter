#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server URL (update with your Flask server IP/domain)
const char* SERVER_URL = "http://YOUR_SERVER_IP:5000/update_count";

// Pins for IR sensors (beam broken = LOW)
const int SENSOR_A_PIN = 32;
const int SENSOR_B_PIN = 33;

// OLED display config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// LEDs for status
const int LED_GREEN = 2;
const int LED_RED = 15;

// Max bus capacity
const int MAX_CAPACITY = 40;

// Passenger count
volatile int current_count = 0;

// Timing constants
const unsigned long DEBOUNCE_MS = 50;
const unsigned long SEQ_WINDOW_MS = 800; // max time between sensor triggers for one event
const unsigned long POST_INTERVAL_MS = 10000; // post every 10 seconds

// State machine for detecting entry/exit
enum State { IDLE, A_TRIGGERED, B_TRIGGERED };
State stateMachine = IDLE;
unsigned long triggerTime = 0;
unsigned long lastEventTime = 0;

unsigned long lastPostTime = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(SENSOR_A_PIN, INPUT_PULLUP);
  pinMode(SENSOR_B_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Connect to WiFi
  connectWiFi();

  updateDisplay();
  updateLEDs();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  bool a = (digitalRead(SENSOR_A_PIN) == LOW); // beam broken = LOW
  bool b = (digitalRead(SENSOR_B_PIN) == LOW);
  unsigned long now = millis();

  // State machine to detect entry/exit
  switch (stateMachine) {
    case IDLE:
      if (a) {
        stateMachine = A_TRIGGERED;
        triggerTime = now;
      } else if (b) {
        stateMachine = B_TRIGGERED;
        triggerTime = now;
      }
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

  // Periodically post data to server
  if (now - lastPostTime >= POST_INTERVAL_MS) {
    lastPostTime = now;
    postCountToServer();
  }

  delay(20);
}

void handleEntry() {
  unsigned long now = millis();
  if (now - lastEventTime < DEBOUNCE_MS) return;
  lastEventTime = now;

  if (current_count < MAX_CAPACITY) {
    current_count++;
    Serial.printf("Entry detected. Count: %d\n", current_count);
    updateDisplay();
    updateLEDs();
  } else {
    Serial.println("Bus full. Entry blocked.");
  }
}

void handleExit() {
  unsigned long now = millis();
  if (now - lastEventTime < DEBOUNCE_MS) return;
  lastEventTime = now;

  if (current_count > 0) {
    current_count--;
    Serial.printf("Exit detected. Count: %d\n", current_count);
    updateDisplay();
    updateLEDs();
  } else {
    Serial.println("Count already zero. Exit ignored.");
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Bus Occupancy");
  display.printf("Count: %d / %d\n", current_count, MAX_CAPACITY);
  if (current_count >= MAX_CAPACITY) {
    display.println("Status: FULL");
  } else {
    display.printf("Space: %d\n", MAX_CAPACITY - current_count);
  }
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
    Serial.println("WiFi not connected. Skipping POST.");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"count\":" + String(current_count) + ",\"capacity\":" + String(MAX_CAPACITY) + "}";
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    Serial.printf("POST success, code: %d\n", httpResponseCode);
  } else {
    Serial.printf("POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi SSID: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  // Wait 10 seconds max
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Failed to connect to WiFi.");
  }
}
