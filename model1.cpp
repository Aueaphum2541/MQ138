#include <Arduino.h>

/****************************************************
 * ESP32 Breath Detection
 * MQ-138 + XGZP6847A
 ****************************************************/

// ---------- FUNCTION PROTOTYPE ----------
void calibratePressureBaseline();
void calibrateVOCBaseline();
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
#define PRESSURE_THRESHOLD_MMHG   1.0
#define STABLE_COUNT_REQUIRED     5

// ---------- FILTER ----------
#define MA_WINDOW 10

// ---------- VOC MAPPING ----------
#define MQ_DELTA_MIN   30     // เริ่มถือว่ามี VOC
#define MQ_DELTA_MAX   800    // VOC สูงมาก (ปรับตามจริง)

// ---------- GLOBAL ----------
float pressureBaselineVoltage = 0.0;
int   mqBaseline = 0;

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

  Serial.println("Calibrating... DO NOT BLOW");
  calibratePressureBaseline();
  calibrateVOCBaseline();
  Serial.println("Ready");
}

// --------------------------------------------------
void loop() {

  // ---------- READ PRESSURE ----------
  int rawP = analogRead(PRESSURE_PIN);
  float voltage = (rawP / ADC_MAX) * VREF;
  float deltaV = voltage - pressureBaselineVoltage;

  float pressure_kPa =
    (deltaV / (VOUT_MAX - VOUT_MIN)) * PRESSURE_RANGE_KPA;

  float pressure_mmHg = pressure_kPa * 7.50062;

  float filteredPressure = movingAverage(pressure_mmHg);

  // ---------- BLOW DETECTION ----------
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
    int mqRaw = analogRead(MQ138_PIN);
    int mqDelta = mqRaw - mqBaseline;
    if (mqDelta < 0) mqDelta = 0;

    int vocLevel = map(mqDelta,
                       MQ_DELTA_MIN,
                       MQ_DELTA_MAX,
                       0,
                       100);
    vocLevel = constrain(vocLevel, 0, 100);

    Serial.print("BLOW | Pressure: ");
    Serial.print(filteredPressure, 2);
    Serial.print(" mmHg | VOC Level: ");
    Serial.println(vocLevel);
  } else {
    Serial.println("IDLE | Pressure: 0 mmHg | VOC Level: 0");
  }

  delay(100);
}

// --------------------------------------------------
void calibratePressureBaseline() {
  float sum = 0.0;
  for (int i = 0; i < 100; i++) {
    int raw = analogRead(PRESSURE_PIN);
    sum += (raw / ADC_MAX) * VREF;
    delay(10);
  }
  pressureBaselineVoltage = sum / 100.0;
}

// --------------------------------------------------
void calibrateVOCBaseline() {
  long sum = 0;
  for (int i = 0; i < 200; i++) {
    sum += analogRead(MQ138_PIN);
    delay(10);
  }
  mqBaseline = sum / 200;
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
