#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

// 센서 설정
Adafruit_MPU6050 mpu;
float initialAx = 0, initialAy = 0, initialAz = 0;
const float accelThreshold = 0.1;
const unsigned long alertDuration = 7000;
unsigned long changeStartTime = 0;
bool isChanged = false;
bool alertSent = false;

// Wi-Fi 설정
const char* ssid = "PCU_PB_4F";
const char* password = "";a
const char* SENSOR_ID = "1";
const char* baseUrl = "https://capstone-back.fly.dev/api/management/shake/";

// 시간 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// 9시간(32400초) 추가된 ISO 8601 시간 반환
String getISO8601TimeKST() {
  time_t now;
  time(&now);
  now += 9 * 60 * 60; // 9시간(32400초) 추가
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

void setup() {
  Serial.begin(115200);

  // MPU6050 초기화
  if (!mpu.begin()) {
    Serial.println("MPU6050 연결 실패!");
    while(1);
  }
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  initialAx = a.acceleration.x;
  initialAy = a.acceleration.y;
  initialAz = a.acceleration.z;

  // Wi-Fi 연결
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi 연결 성공");

  // NTP 시간 동기화
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void sendShakeEvent() {
  String iso8601 = getISO8601TimeKST();
  String fullUrl = String(baseUrl) + SENSOR_ID;

  DynamicJsonDocument doc(128);
  doc["status"] = 1;
  doc["shake_date"] = iso8601;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, fullUrl);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.sendRequest("PATCH", jsonPayload);
  String response = http.getString();

  Serial.print("HTTP 코드: ");
  Serial.println(httpCode);
  Serial.print("응답: ");
  Serial.println(response);

  http.end();
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  unsigned long currentTime = millis();

  float diffX = abs(a.acceleration.x - initialAx);
  float diffY = abs(a.acceleration.y - initialAy);
  float diffZ = abs(a.acceleration.z - initialAz);

  bool isAccelChanged = (diffX > accelThreshold) ||
                        (diffY > accelThreshold) ||
                        (diffZ > accelThreshold);

  if (isAccelChanged) {
    if (!isChanged) {
      // 흔들림이 처음 감지된 시점 기록
      changeStartTime = currentTime;
      isChanged = true;
      alertSent = false;
    }

    // 흔들림이 일정 시간(alertDuration) 지속될 때만 한 번만 전송
    if (!alertSent && (currentTime - changeStartTime >= alertDuration)) {
      Serial.println("🚨 소화기 흔들림 감지! 서버에 보고");
      sendShakeEvent();
      alertSent = true; // 한 번만 전송
    }
  } else {
    // 흔들림이 멈췄을 때만 플래그 초기화
    if (isChanged) {
      isChanged = false;
      alertSent = false;
    }
  }

  delay(100);
}


