#include <Arduino.h>

/****************************************************
 * ESP32 Breath Detection (Realtime for Game Physics)
 * MQ-138 (GPIO 26) + XGZP6847A Pressure (GPIO 27)
 ****************************************************/

// ---------- PIN CONFIG ----------
#define MQ138_PIN       26
#define PRESSURE_PIN    27

// ---------- ADC ----------
#define ADC_MAX         4095.0
#define VREF            3.3     // ESP32 ADC reference

// ---------- PRESSURE SENSOR ----------
#define PRESSURE_RANGE_KPA   150.0
#define VOUT_MIN             0.5
#define VOUT_MAX             4.5

// ---------- SETTINGS ----------
#define PRESSURE_THRESHOLD_MMHG   0.5  // ลด Threshold ลงเพื่อให้เกมตอบสนองไวขึ้นแม้เป่าเบาๆ
#define MA_WINDOW 5                    // ลด Window ลง (จาก 10 เหลือ 5) เพื่อลด Input Lag

// ---------- GLOBAL ----------
float pressureBaselineVoltage = 0.0;
int   mqBaseline = 0;

float maBuffer[MA_WINDOW];
int maIndex = 0;
bool maFilled = false;

// ---------- FUNCTION PROTOTYPE ----------
void calibratePressureBaseline();
float movingAverage(float newValue);

// --------------------------------------------------
void setup() {
  Serial.begin(115200); // ใช้ Baudrate สูง
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ138_PIN, ADC_11db);
  analogSetPinAttenuation(PRESSURE_PIN, ADC_11db);

  Serial.println("Calibrating... DO NOT BLOW");
  calibratePressureBaseline();
  Serial.println("System Ready");
}

// --------------------------------------------------
void loop() {

  // 1. อ่าน Pressure
  int rawP = analogRead(PRESSURE_PIN);
  float voltageP = (rawP / ADC_MAX) * VREF;
  
  // คำนวณ Delta (เทียบกับตอนเปิดเครื่อง)
  float deltaV = voltageP - pressureBaselineVoltage;
  if(deltaV < 0) deltaV = 0; // ป้องกันค่าติดลบ

  float pressure_kPa = (deltaV / (VOUT_MAX - VOUT_MIN)) * PRESSURE_RANGE_KPA;
  float pressure_mmHg = pressure_kPa * 7.50062;
  
  // ใช้ Moving Average เล็กน้อยเพื่อกันค่าแกว่ง แต่ไม่ให้ Lag เกินไป
  float filteredPressure = movingAverage(pressure_mmHg);

  // 2. อ่าน MQ-138 (VOC)
  int mqRaw = analogRead(MQ138_PIN);
  float mqVoltage = (mqRaw / ADC_MAX) * VREF;

  // 3. ส่งข้อมูล (Serial Output)
  // Logic: ถ้าแรงดันเกิน Threshold ให้ส่งค่าจริง, ถ้าไม่เกิน ให้ส่ง 0
  // สำคัญ: รูปแบบ Text ต้องเหมือนกันเพื่อให้ Python Regex จับได้เสมอ
  
  if (filteredPressure > PRESSURE_THRESHOLD_MMHG) {
    Serial.print("BLOW | Pressure: ");
    Serial.print(filteredPressure, 2);
    Serial.print(" | MQ Volt: ");
    Serial.println(mqVoltage, 3); // *** ใช้ println เพื่อจบประโยคทันที ***
  } else {
    // ส่ง Format เดียวกัน แต่ค่าเป็น 0.00 เพื่อให้เกมรู้ว่า "ปล่อยปุ่ม/หยุดเป่า"
    Serial.println("IDLE | Pressure: 0.00 | MQ Volt: 0.00"); 
  }

  // 4. Loop Speed
  // ใช้ 33ms เพื่อให้ได้ประมาณ 30 Hz (เพียงพอสำหรับเกม และไม่ Load คอมเกินไป)
  delay(33); 
}

// --------------------------------------------------
void calibratePressureBaseline() {
  float sum = 0.0;
  // อ่าน 50 ครั้งหาค่าเฉลี่ยตอนเปิดเครื่อง (Zero Offset)
  for (int i = 0; i < 50; i++) {
    int raw = analogRead(PRESSURE_PIN);
    sum += (raw / ADC_MAX) * VREF;
    delay(10);
  }
  pressureBaselineVoltage = sum / 50.0;
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
