#include "sensor.h"
#include "config.h"
#include <Wire.h>
#include <Preferences.h>

namespace {
float gScale = 1.0f, gOffset = 0.0f;

// Rohwert (Volt am Wandlereingang) -> mA bzw. V am Sensor -> nominaler Abstand
float voltsToNominal(float v, float* raw) {
#if SENSOR_MODE_CURRENT
  const float mA = v * 1000.0f / SHUNT_OHM;
  if (raw) *raw = mA;
  const float f = (mA - 4.0f) / 16.0f;
#else
  const float vs = v * VDIV_RATIO;
  if (raw) *raw = vs;
  const float f = vs / 5.0f;
#endif
  if (f < -SENSOR_VALID_MARGIN || f > 1.0f + SENSOR_VALID_MARGIN) return NAN;
  return DIST_AT_MIN_MM + f * (DIST_AT_MAX_MM - DIST_AT_MIN_MM);
}

inline float cal(float d) { return isnan(d) ? NAN : d * gScale + gOffset; }

#if SENSOR_ADC_ADS1115
// ---------------- ADS1115 (Dauerwandlung, 860 SPS) ----------------
constexpr float    ADS_FS[]   = { 6.144f, 4.096f, 2.048f, 1.024f, 0.512f, 0.256f };
constexpr float    ADS_LSB_V  = ADS_FS[ADS_PGA] / 32768.0f;
constexpr uint32_t ADS_PERIOD_US = 1250;   // 1/860 s + Toleranz des internen Oszillators
bool     adsOk = false;
uint32_t tLast = 0;
int32_t  accSum = 0;
uint16_t accN = 0;

bool adsWriteReg(uint8_t reg, uint16_t v) {
  Wire.beginTransmission(ADS_ADDR);
  Wire.write(reg); Wire.write(v >> 8); Wire.write(v & 0xFF);
  return Wire.endTransmission() == 0;
}

bool adsRead(int16_t& v) {   // Pointer steht dauerhaft auf Conversion-Register
  if (Wire.requestFrom((uint8_t)ADS_ADDR, (uint8_t)2) != 2) return false;
  v = (int16_t)((Wire.read() << 8) | Wire.read());
  return true;
}

// auf nächste neue Wandlung warten (kurz aktiv, sonst vTaskDelay)
bool adsNext(int16_t& v) {
  for (;;) {
    const uint32_t dt = micros() - tLast;
    if (dt >= ADS_PERIOD_US) break;
    if (ADS_PERIOD_US - dt > 1500) vTaskDelay(1);
  }
  tLast = micros();
  return adsRead(v);
}

float adsReadNominal(uint16_t n, float* raw) {
  if (!adsOk) { if (raw) *raw = NAN; return NAN; }
  if (n == 0) n = 1;
  tLast = micros();                           // erst die nächste, frische Wandlung verwenden
  int32_t sum = 0; uint16_t got = 0;
  for (uint16_t i = 0; i < n; ++i) { int16_t v; if (adsNext(v)) { sum += v; ++got; } }
  if (!got) { if (raw) *raw = NAN; return NAN; }
  return voltsToNominal((float)sum / got * ADS_LSB_V, raw);
}
#endif
} // namespace

namespace Sensor {

bool begin() {
  Preferences pr;
  pr.begin("scanner", true);
  gScale  = pr.getFloat("cs", 1.0f);
  gOffset = pr.getFloat("co", 0.0f);
  pr.end();

#if SENSOR_ADC_ADS1115
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  const uint16_t mux = ADS_DIFFERENTIAL ? 0b000 : 0b100;          // AIN0-AIN1 bzw. AIN0-GND
  const uint16_t cfg = (mux << 12) | ((uint16_t)ADS_PGA << 9) | (0 << 8) /*Dauerwandlung*/
                     | (0b111 << 5) /*860 SPS*/ | 0b00011 /*Komparator aus*/;
  adsOk = adsWriteReg(0x01, cfg);
  if (adsOk) {
    Wire.beginTransmission(ADS_ADDR); Wire.write(0x00);           // Pointer -> Conversion
    adsOk = Wire.endTransmission() == 0;
  }
  tLast = micros();
  return adsOk;
#else
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SENSOR, ADC_11db);   // ~0..3.1 V (klassischer ESP32 linear bis ~2.45 V)
  return true;
#endif
}

bool isAds() { return SENSOR_ADC_ADS1115; }

float readNominalMm(uint16_t samples, float* raw) {
#if SENSOR_ADC_ADS1115
  return adsReadNominal(samples, raw);
#else
  if (samples == 0) samples = 1;
  uint32_t sum = 0;
  for (uint16_t i = 0; i < samples; ++i) sum += analogReadMilliVolts(PIN_SENSOR);
  return voltsToNominal((float)sum / samples / 1000.0f, raw);
#endif
}

float readMm(uint16_t samples, float* raw) { return cal(readNominalMm(samples, raw)); }

void accReset() {
#if SENSOR_ADC_ADS1115
  accSum = 0; accN = 0;
#endif
}

void accPoll() {
#if SENSOR_ADC_ADS1115
  if (!adsOk || micros() - tLast < ADS_PERIOD_US) return;
  tLast = micros();
  int16_t v;
  if (adsRead(v)) { accSum += v; ++accN; }
#endif
}

uint16_t accCount() {
#if SENSOR_ADC_ADS1115
  return accN;
#else
  return 0;
#endif
}

float accTakeMm(float* raw) {
#if SENSOR_ADC_ADS1115
  if (!accN) return readMm(1, raw);
  const float d = cal(voltsToNominal((float)accSum / accN * ADS_LSB_V, raw));
  accReset();
  return d;
#else
  return readMm(1, raw);
#endif
}

void setCal(float scale, float offset) {
  gScale = scale; gOffset = offset;
  Preferences pr;
  pr.begin("scanner", false);
  pr.putFloat("cs", scale);
  pr.putFloat("co", offset);
  pr.end();
}

float calScale()  { return gScale; }
float calOffset() { return gOffset; }

} // namespace Sensor
