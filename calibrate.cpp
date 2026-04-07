#include <Arduino.h>

/******** PIN ********/
#define PRESSURE_PIN 33

/******** ADC ********/
#define ADC_MAX 4095.0
#define VREF 3.3

/******** SENSOR PARAM ********/
#define PRESSURE_RANGE_KPA 150.0
#define VOUT_MIN 0.5
#define VOUT_MAX 4.5

/******** FILTER ********/
#define MA_WINDOW 15
#define MEDIAN_WINDOW 5

/******** NOISE ********/
#define PRESSURE_DEADZONE_VOLT 0.01
#define PRESSURE_IDLE_THRESHOLD 3.0

float pressureBaselineVoltage = 0;

/******** BUFFERS ********/
float maBuffer[MA_WINDOW];
int maIndex = 0;
bool maFilled = false;

float medianBuffer[MEDIAN_WINDOW];
int medianIndex = 0;


/******** MEDIAN FILTER ********/
float medianFilter(float newValue)
{
  medianBuffer[medianIndex++] = newValue;

  if(medianIndex >= MEDIAN_WINDOW)
    medianIndex = 0;

  float temp[MEDIAN_WINDOW];

  for(int i=0;i<MEDIAN_WINDOW;i++)
    temp[i] = medianBuffer[i];

  for(int i=0;i<MEDIAN_WINDOW-1;i++)
    for(int j=i+1;j<MEDIAN_WINDOW;j++)
      if(temp[i] > temp[j])
      {
        float t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }

  return temp[MEDIAN_WINDOW/2];
}


/******** MOVING AVERAGE ********/
float movingAverage(float newValue)
{
  maBuffer[maIndex++] = newValue;

  if(maIndex >= MA_WINDOW)
  {
    maIndex = 0;
    maFilled = true;
  }

  float sum = 0;

  int count = maFilled ? MA_WINDOW : maIndex;

  for(int i=0;i<count;i++)
    sum += maBuffer[i];

  return sum / count;
}


/******** BASELINE CALIBRATION ********/
void calibratePressureBaseline()
{
  Serial.println("Calibrating baseline...");

  float sum = 0;

  for(int i=0;i<300;i++)
  {
    int raw = analogRead(PRESSURE_PIN);

    float voltage = (raw / ADC_MAX) * VREF;

    sum += voltage;

    delay(10);
  }

  pressureBaselineVoltage = sum / 300;

  Serial.print("Baseline voltage = ");
  Serial.println(pressureBaselineVoltage,4);
}


/******** SETUP ********/
void setup()
{
  Serial.begin(115200);
  delay(2000);

  analogReadResolution(12);
  analogSetPinAttenuation(PRESSURE_PIN, ADC_11db);

  calibratePressureBaseline();
}


/******** LOOP ********/
void loop()
{
  int raw = analogRead(PRESSURE_PIN);

  float voltage = (raw / ADC_MAX) * VREF;

  float deltaV = abs(voltage - pressureBaselineVoltage);

  if(deltaV < PRESSURE_DEADZONE_VOLT)
    deltaV = 0;

  float pressure_kPa =
      (deltaV / (VOUT_MAX - VOUT_MIN)) * PRESSURE_RANGE_KPA;

  float pressure_mmHg = pressure_kPa * 7.50062;

  // median filter
  float medianPressure = medianFilter(pressure_mmHg);

  // moving average
  float filteredPressure = movingAverage(medianPressure);

  // idle clamp
  if(filteredPressure < PRESSURE_IDLE_THRESHOLD)
    filteredPressure = 0;

  Serial.print("Pressure(mmHg): ");
  Serial.println(filteredPressure,2);

  delay(100);
}
