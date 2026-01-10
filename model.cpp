#include <Arduino.h>

/****************************************************
 * ESP32 Breath + VOC Index System
 * MQ-138 + XGZP6847A
 ****************************************************/

// ---------- FUNCTION PROTOTYPE ----------
void calibratePressureBaseline();
void calibrateVOCBaseline();
float movingAverage(float v);

// ---------- PIN ----------
#define MQ138_PIN       26
#define PRESSURE_PIN    27

// ---------- ADC ----------
#define ADC_MAX   4095.0
#define VREF      3.3

// ---------- PRESSURE SENSOR ----------
#define PRESSURE_RANGE_KPA 150.0
#define VOUT_MIN 0.5
#define VOUT_MAX 4.5

// ---------- DETECTION ----------
#define PRESSURE_THRESHOLD_MMHG  1.0
#define STABLE_COUNT_REQUIRED   5

// ---------- FILTER ----------
#define MA_WINDOW 10

// ---------- VOC INDEX ----------
#define VOC_INDEX_MAX 100

// ---------- GLOBAL ----------
float pressureBaselineV = 0.0;
int vocBaselineADC = 0;

float maBuf[MA_WINDOW];
int maIdx = 0;
bool maFull = false;

bool isBlowing = false;
int stableCounter = 0;

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

  Serial.println("System Ready");
}

// --------------------------------------------------
void loop() {

  // ---------- PRESSURE ----------
  int rawP = analogRead(PRESSURE_PIN);
  float v = (rawP / ADC_MAX) * VREF;
  float dV = v - pressureBaselineV;

  float p_kPa = (dV / (VOUT_MAX - VOUT_MIN)) * PRESSURE_RANGE_KPA;
  float p_mmHg = p_kPa * 7.50062;

  float p_filt = movingAverage(p_mmHg);

  // ---------- BLOW DETECTION ----------
  if (p_filt > PRESSURE_THRESHOLD_MMHG) {
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

    // VOC Index (normalized)
    int vocIndex = map(mq, vocBaselineADC, ADC_MAX, 0, VOC_INDEX_MAX);
    vocIndex = constrain(vocIndex, 0, VOC_INDEX_MAX);

    Serial.print("BLOW | Pressure: ");
    Serial.print(p_filt, 2);
    Serial.print(" mmHg | VOC Index: ");
    Serial.println(vocIndex);
  } else {
    Serial.println("IDLE | Pressure: 0 mmHg | VOC Index: 0");
  }

  delay(2000);
}

// --------------------------------------------------
void calibratePressureBaseline() {
  float sum = 0;
  for (int i = 0; i < 100; i++) {
    int r = analogRead(PRESSURE_PIN);
    sum += (r / ADC_MAX) * VREF;
    delay(10);
  }
  pressureBaselineV = sum / 100.0;
}

// --------------------------------------------------
void calibrateVOCBaseline() {
  long sum = 0;
  for (int i = 0; i < 1000; i++) {
    sum += analogRead(MQ138_PIN);
    delay(10);
  }
  vocBaselineADC = sum / 100;
}

// --------------------------------------------------
float movingAverage(float v) {
  maBuf[maIdx++] = v;
  if (maIdx >= MA_WINDOW) {
    maIdx = 0;
    maFull = true;
  }

  float s = 0;
  int c = maFull ? MA_WINDOW : maIdx;
  for (int i = 0; i < c; i++) s += maBuf[i];
  return s / c;
}
