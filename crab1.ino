#define BLYNK_TEMPLATE_ID   "TMPL3Xckio2DE"
#define BLYNK_TEMPLATE_NAME "SENSEFORGE"
#define BLYNK_AUTH_TOKEN    "4EJniUqrwWGrbE2RzNdDBjRaq_u-uj4V"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>

char ssid[] = "GalaxyF225A78";
char pass[] = "12345678";

// ESP32 GPIO connections
const int TEMP_PIN  = 4;
const int PH_PIN    = 34;
const int TDS_PIN   = 35;
const int LEVEL_PIN = 32;

// DS18B20
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

BlynkTimer timer;

// ---------- CALIBRATION SETTINGS ----------
float PH4_VOLTAGE  = 3.00;  // Adjust based on your buffer solution readings
float PH7_VOLTAGE  = 2.50;  // Adjust based on your buffer solution readings
float TDS_FACTOR   = 0.50;

int LEVEL_LOW_THRESHOLD  = 900;
int LEVEL_HIGH_THRESHOLD = 1800;

// ---------- ANALOG VOLTAGE READING ----------
float readVoltage(int pin) {
  uint32_t totalMilliVolts = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    totalMilliVolts += analogReadMilliVolts(pin);
    delay(5);
  }

  return (totalMilliVolts / (float)samples) / 1000.0;
}

// ---------- READ TEMPERATURE (V1 & V8) ----------
float readTemperature() {
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C || tempC < -40 || tempC > 125) {
    Serial.println("Temperature sensor error");
    Blynk.virtualWrite(V8, "Sensor Error");
    return 25.0; // Return fallback for TDS calculation
  }

  Serial.printf("Temperature: %.2f C\n", tempC);
  Blynk.virtualWrite(V1, tempC);

  // Temperature threshold alert (e.g. standard crablet range: 25 - 32 C)
  if (tempC < 24.0) {
    Blynk.virtualWrite(V8, "Low Temp Alert");
  } else if (tempC > 33.0) {
    Blynk.virtualWrite(V8, "High Temp Alert");
  } else {
    Blynk.virtualWrite(V8, "Normal");
  }

  return tempC;
}

// ---------- READ PH (V0 & V7) ----------
float readPH() {
  float voltage = readVoltage(PH_PIN);

  if (abs(PH4_VOLTAGE - PH7_VOLTAGE) < 0.01) {
    Serial.println("Invalid pH calibration voltages");
    Blynk.virtualWrite(V7, "Calibration Error");
    return -1.0;
  }

  float phValue = 4.0 + (voltage - PH4_VOLTAGE) * (7.0 - 4.0) / (PH7_VOLTAGE - PH4_VOLTAGE);

  Serial.printf("pH voltage: %.3f V, pH: %.2f\n", voltage, phValue);

  if (phValue < 0 || phValue > 14) {
    Blynk.virtualWrite(V0, 0);
    Blynk.virtualWrite(V7, "Sensor Fault");
    return -1.0;
  }

  Blynk.virtualWrite(V0, phValue);

  // pH Alert thresholds (e.g. acceptable aquaculture range: 7.5 - 8.5)
  if (phValue < 7.0) {
    Blynk.virtualWrite(V7, "Low pH Alert");
  } else if (phValue > 8.5) {
    Blynk.virtualWrite(V7, "High pH Alert");
  } else {
    Blynk.virtualWrite(V7, "Optimal");
  }

  return phValue;
}

// ---------- READ TDS (V2) ----------
float readTDS(float tempC) {
  float voltage = readVoltage(TDS_PIN);

  float compensation = 1.0 + 0.02 * (tempC - 25.0);
  float compensatedVoltage = voltage / compensation;

  float tdsPPM = (133.42 * compensatedVoltage * compensatedVoltage * compensatedVoltage
                - 255.86 * compensatedVoltage * compensatedVoltage
                + 857.39 * compensatedVoltage) * TDS_FACTOR;

  if (tdsPPM < 0) tdsPPM = 0;

  Serial.printf("TDS voltage: %.3f V, TDS: %.0f ppm\n", voltage, tdsPPM);
  Blynk.virtualWrite(V2, tdsPPM);

  return tdsPPM;
}

// ---------- READ WATER LEVEL / DISTANCE (V3) ----------
void readWaterLevel() {
  int raw = analogRead(LEVEL_PIN);

  Serial.printf("Water Level reading: %d\n", raw);
  Blynk.virtualWrite(V3, raw);
}

// ---------- AGGREGATE OVERALL WATER QUALITY (V9) ----------
void evaluateOverallQuality(float ph, float tempC, float tds) {
  if (ph < 0 || tempC < -40) {
    Blynk.virtualWrite(V9, "Sensor Error");
    return;
  }

  if ((ph >= 7.2 && ph <= 8.5) && (tempC >= 25.0 && tempC <= 32.0)) {
    Blynk.virtualWrite(V9, "Optimal Quality");
  } else {
    Blynk.virtualWrite(V9, "Requires Attention");
  }
}

// ---------- MAIN SENSOR ROUTINE ----------
void sendSensorReadings() {
  float currentTemp = readTemperature();
  float currentPH   = readPH();
  float currentTDS  = readTDS(currentTemp);
  readWaterLevel();

  evaluateOverallQuality(currentPH, currentTemp, currentTDS);

  Serial.println("------------------------------------");
}

// ---------- BLYNK CONTROL HANDLERS ----------
// V4: Water Pump Status/Switch
BLYNK_WRITE(V4) {
  int val = param.asInt();
  Serial.printf("Water Pump (V4) toggled: %d\n", val);
}

// V6: System Mode Switch
BLYNK_WRITE(V6) {
  int val = param.asInt();
  Serial.printf("System Mode (V6) set to: %d\n", val);
}

// V10: Pump Control Switch
BLYNK_WRITE(V10) {
  int val = param.asInt();
  Serial.printf("Pump Control (V10) set to: %d\n", val);
}

// V11: Aerator Control Switch
BLYNK_WRITE(V11) {
  int val = param.asInt();
  Serial.printf("Aerator Control (V11) set to: %d\n", val);
  // Reflect state to LED on V5
  Blynk.virtualWrite(V5, val ? 255 : 0);
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);

  tempSensor.begin();

  pinMode(PH_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(LEVEL_PIN, INPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Send readings every 5 seconds
  timer.setInterval(5000L, sendSensorReadings);
}

void loop() {
  Blynk.run();
  timer.run();
}
