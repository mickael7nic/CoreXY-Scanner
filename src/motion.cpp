#include "motion.h"
#include "config.h"
#include "soc/gpio_reg.h"

static_assert(PIN_STEP_A < 32 && PIN_STEP_B < 32, "STEP-Pins muessen < GPIO32 sein");

namespace {
constexpr uint32_t FRAC   = 1UL << 24;   // Phasenakku: 1 Step = 2^24
constexpr uint32_t MASK_A = 1UL << PIN_STEP_A;
constexpr uint32_t MASK_B = 1UL << PIN_STEP_B;

hw_timer_t* timer = nullptr;
volatile bool mBusy = false, mStopReq = false, mEnabled = false, mInhibit = false;
volatile int32_t posA = 0, posB = 0;

// Bewegungsdaten (vom Task gesetzt solange !mBusy, danach nur ISR)
volatile int32_t  nTotal = 0, nDone = 0, nA = 0, nB = 0, eA = 0, eB = 0;
volatile int8_t   sA = 1, sB = 1;
volatile uint32_t inc = 0, incMax = 0, incMin = 0, dInc = 1, acc = 0;

// Achtung: kein float in der ISR (Xtensa-FPU)!
void ARDUINO_ISR_ATTR onTick() {
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_A | MASK_B);   // Step-Puls vom letzten Tick beenden
  if (!mBusy) return;

  uint32_t i = inc;
  const uint32_t rem = (uint32_t)(nTotal - nDone);
  // Bremsweg v²/(2a) >= Reststrecke  <=>  inc² >= rem * 2*dInc * FRAC
  const uint64_t lim = ((uint64_t)rem * (uint64_t)(2UL * dInc)) << 24;
  const bool decel = mStopReq || ((uint64_t)i * i >= lim);

  if (decel) {
    if (mStopReq && i <= incMin) { mBusy = false; mStopReq = false; return; }
    i = (i > incMin + dInc) ? i - dInc : incMin;
  } else if (i < incMax) {
    i += dInc;
    if (i > incMax) i = incMax;
  }
  inc = i;

  uint32_t a = acc + i;
  if (a >= FRAC) {
    a -= FRAC;
    const int32_t n = nTotal;
    uint32_t mask = 0;
    int32_t e = eA + nA;
    if (e >= n) { e -= n; mask |= MASK_A; posA = posA + sA; }
    eA = e;
    e = eB + nB;
    if (e >= n) { e -= n; mask |= MASK_B; posB = posB + sB; }
    eB = e;
    if (mask) REG_WRITE(GPIO_OUT_W1TS_REG, mask);
    const int32_t done = nDone + 1;
    nDone = done;
    if (done >= n) { mBusy = false; mStopReq = false; }
  }
  acc = a;
}

uint32_t toInc(float stepsPerSec) {
  float v = stepsPerSec * (float)FRAC / (float)TICK_HZ;
  if (v > FRAC / 2) v = FRAC / 2;   // min. 1 Tick low zwischen zwei Pulsen
  if (v < 1.0f) v = 1.0f;
  return (uint32_t)v;
}

bool startMove(int32_t dA, int32_t dB, float speed) {
  if (mBusy || mInhibit) return false;
  if (dA == 0 && dB == 0) return true;
  if (!mEnabled) Motion::enable(true);

  digitalWrite(PIN_DIR_A, ((dA >= 0) != DIR_INVERT_A) ? HIGH : LOW);
  digitalWrite(PIN_DIR_B, ((dB >= 0) != DIR_INVERT_B) ? HIGH : LOW);
  sA = dA >= 0 ? 1 : -1;
  sB = dB >= 0 ? 1 : -1;
  const int32_t a = abs(dA), b = abs(dB), n = a > b ? a : b;
  nA = a;
  nB = b;
  nTotal = n;
  nDone = 0;
  eA = n / 2;
  eB = n / 2;
  acc = 0;

  const float spm = STEPS_PER_MM;
  incMax = toInc(speed * spm);
  incMin = toInc(START_SPEED_MM_S * spm);
  if (incMin > incMax) incMin = incMax;
  const float d = ACCEL_MM_S2 * spm * (float)FRAC / ((float)TICK_HZ * (float)TICK_HZ);
  dInc = d < 1.0f ? 1 : (uint32_t)d;
  inc = incMin;

  delayMicroseconds(DIR_SETUP_US);
  mStopReq = false;
  mBusy = true;   // zuletzt setzen -> ISR startet
  return true;
}
} // namespace

namespace Motion {

void begin() {
  pinMode(PIN_STEP_A, OUTPUT); pinMode(PIN_DIR_A, OUTPUT); pinMode(PIN_EN_A, OUTPUT);
  pinMode(PIN_STEP_B, OUTPUT); pinMode(PIN_DIR_B, OUTPUT); pinMode(PIN_EN_B, OUTPUT);
  digitalWrite(PIN_STEP_A, LOW);
  digitalWrite(PIN_STEP_B, LOW);
  mEnabled = true;     // erzwingt Schreiben in enable()
  enable(false);

  timer = timerBegin(1000000);                       // 1 MHz (Arduino-ESP32 Core 3.x)
  timerAttachInterrupt(timer, &onTick);
  timerAlarm(timer, 1000000 / TICK_HZ, true, 0);
}

void enable(bool on) {
  if (on == mEnabled) return;
  if (!on) { mStopReq = true; while (mBusy) delay(1); }
  const uint8_t lvl = on ? EN_LEVEL_ACTIVE : !EN_LEVEL_ACTIVE;
  digitalWrite(PIN_EN_A, lvl);
  digitalWrite(PIN_EN_B, lvl);
  mEnabled = on;
  if (on) delay(ENABLE_DELAY_MS);
}

bool isEnabled() { return mEnabled; }
bool busy()      { return mBusy; }
void stop()      { if (mBusy) mStopReq = true; }

bool moveTo(float xmm, float ymm, float speed) {
  const int32_t xs = lroundf(xmm * STEPS_PER_MM);
  const int32_t ys = lroundf(ymm * STEPS_PER_MM);
  return startMove((xs + ys) - posA, (xs - ys) - posB, speed);
}

bool moveRel(float dx, float dy, float speed) { return moveTo(x() + dx, y() + dy, speed); }

void waitDone() { while (mBusy) vTaskDelay(1); }

bool setZero() {
  if (mBusy) return false;
  posA = 0; posB = 0;
  return true;
}

void inhibit(bool on) {
  mInhibit = on;
  if (on) { stop(); while (mBusy) vTaskDelay(1); enable(false); }
}

float x() { return 0.5f * (float)(posA + posB) / STEPS_PER_MM; }
float y() { return 0.5f * (float)(posA - posB) / STEPS_PER_MM; }

} // namespace Motion
