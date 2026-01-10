#include <Arduino.h>

/****************************************************
 * ESP32 Breath Detection
 * MQ-138 + XGZP6847A
 ****************************************************/

// ---------- FUNCTION PROTOTYPE ----------
void calibratePressureBaseline();
float movingAverage(float newValue);

// ---------- PIN CONFIG ----------
#define MQ138_PIN       26
#define PRESSURE_PIN    27

// ---------- ADC ----------
#define ADC_MAX         4095.0
#define VREF            3.3

// ---------- PRESSURE SENSOR ----------
#define PRESSURE_RANGE_KPA   150.0
#define VOUT_MIN             0.5
#define VOUT_MAX             4.5

// ---------- DETECTION PARAM ----------
#define PRESSURE_THRESHOLD_MMHG   1.0   // ต่ำกว่า = ไม่เป่า
#define STABLE_COUNT_REQUIRED     5     // ต้องเกิน threshold กี่ครั้งติด

// ---------- FILTER ----------
#define MA_WINDOW 10

// ---------- GLOBAL ----------
float pressureBaselineVoltage = 0.0;
float maBuffer[MA_WINDOW];
int maIndex = 0;
bool maFilled = false;

int stableCounter = 0;
bool isBlowing = false;

// --------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ138_PIN, ADC_11db);
  analogSetPinAttenuation(PRESSURE_PIN, ADC_11db);

  Serial.println("Calibrating baseline... DO NOT BLOW");
  calibratePressureBaseline();
  Serial.println("Ready");
}

void loop() {

  // ---------- READ PRESSURE ----------
  int raw = analogRead(PRESSURE_PIN);
  float voltage = (raw / ADC_MAX) * VREF;
  float deltaV = voltage - pressureBaselineVoltage;

  float pressure_kPa =
    (deltaV / (VOUT_MAX - VOUT_MIN)) * PRESSURE_RANGE_KPA;

  float pressure_mmHg = pressure_kPa * 7.50062;

  // ---------- FILTER ----------
  float filteredPressure = movingAverage(pressure_mmHg);

  // ---------- DETECTION ----------
  if (filteredPressure > PRESSURE_THRESHOLD_MMHG) {
    stableCounter++;
    if (stableCounter >= STABLE_COUNT_REQUIRED) {
      isBlowing = true;
    }
  } else {
    stableCounter = 0;
    isBlowing = false;
  }

  // ---------- OUTPUT ----------
  if (isBlowing) {
    int mq = analogRead(MQ138_PIN);

    Serial.print("BLOW | Pressure: ");
    Serial.print(filteredPressure, 2);
    Serial.print(" mmHg | MQ-138: ");
    Serial.println(mq);
  } else {
    Serial.println("IDLE | Pressure: 0 mmHg | MQ-138: 0");
  }

  delay(100);
}

// --------------------------------------------------
void calibratePressureBaseline() {
  float sum = 0;

  for (int i = 0; i < 100; i++) {
    int raw = analogRead(PRESSURE_PIN);
    float v = (raw / ADC_MAX) * VREF;
    sum += v;
    delay(10);
  }
  pressureBaselineVoltage = sum / 100.0;
}

// --------------------------------------------------
float movingAverage(float newValue) {
  maBuffer[maIndex++] = newValue;
  if (maIndex >= MA_WINDOW) {
    maIndex = 0;
    maFilled = true;
  }

  float sum = 0;
  int count = maFilled ? MA_WINDOW : maIndex;

  for (int i = 0; i < count; i++) sum += maBuffer[i];
  return sum / count;
}
