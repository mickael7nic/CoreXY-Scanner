// CoreXY Laser-Oberflächenscanner – ESP32, Arduino-ESP32 Core >= 3.0
// Scandaten liegen im Flash (LittleFS) -> Partitionsschema "Default 4MB with spiffs"
// Web-UI: http://192.168.55.113/

#include <WiFi.h>
#include <WebServer.h>
#include <esp_mac.h>
#include <Update.h>
#include <LittleFS.h>
#include <time.h>
#include "config.h"
#include "secrets.h"
#include "motion.h"
#include "sensor.h"
#include "webpage.h"

WebServer server(80);

enum ScanState : uint8_t { ST_IDLE, ST_SCAN, ST_RETURN, ST_DONE, ST_ABORTED, ST_FRAME, ST_PAUSED };
static const char* STATE_NAMES[] = { "IDLE", "SCAN", "RETURN", "DONE", "ABORTED", "FRAME", "PAUSED" };

struct ScanParams { float w, h, res, feed, travel, latMs; uint32_t settleMs; uint16_t avg; bool bidir, fly; };

static ScanParams         sp;
// Scandaten: Zeile für Zeile im Flash, Rohabstand in 0.01 mm als int16, INVALID16 = ungültig
static constexpr int16_t  INVALID16 = INT16_MIN;
static const char*        DATA_FILE = "/scan.bin";
static const char*        META_FILE = "/scan.meta";
struct ScanMeta { uint32_t magic, nx, ny; float res; uint8_t complete, partial, pad[2]; uint32_t e0, e1; };
static constexpr uint32_t META_MAGIC = 0x53434E32;   // "SCN2" (mit Start-/Endzeit)
static int16_t*           rowBuf = nullptr;          // aktuelle Zeile im RAM
static uint32_t           rowBufN = 0;
static bool               rowHasData = false;
static volatile uint32_t  rowsWritten = 0;
static inline int16_t toQ(float d) { return isnan(d) ? INVALID16 : (int16_t)constrain(lroundf(d * 100.0f), -32767L, 32767L); }
static uint32_t           gridNx = 0, gridNy = 0;
static float              gridRes = 0;
static volatile uint32_t  pointsDone = 0;
static volatile ScanState state = ST_IDLE;
static volatile bool      abortReq = false;
static volatile bool      wasAborted = false;   // abgebrochener Scan, als Teilscan übernommen
static volatile uint32_t  lastActivity = 0;     // für Motor-Abschaltung bei Inaktivität
static volatile uint32_t  scanT0 = 0, scanT1 = 0; // millis() Start / Ende des letzten Scans (0 = unbekannt)
static volatile bool      pauseReq = false;
static volatile uint32_t  pausedMs = 0, pauseT0 = 0; // Pausenzeit wird von der Laufzeit abgezogen
static volatile uint32_t  scanE0 = 0, scanE1 = 0; // Unix-Zeit Start / Ende (NTP), 0 = unbekannt

// aktuelle Unix-Zeit, 0 solange NTP noch nicht synchronisiert
static uint32_t nowEpoch() { time_t t; time(&t); return t > 1700000000 ? (uint32_t)t : 0; }
static TaskHandle_t       scanTaskHandle = nullptr;
static TaskHandle_t       frameTaskHandle = nullptr;
static float              frW = 0, frH = 0, frF = DEFAULT_TRAVEL_SPEED;
static volatile float     lastDist = NAN, lastRaw = NAN;
static const char*        FW_BUILD = __DATE__ " " __TIME__;

// ------------------------------------------------------------ Flash-Speicher
static void writeMeta(uint32_t ny, bool complete, bool partial) {
  ScanMeta m{ META_MAGIC, gridNx, ny, gridRes, (uint8_t)complete, (uint8_t)partial, {0, 0}, scanE0, scanE1 };
  File f = LittleFS.open(META_FILE, FILE_WRITE);
  if (f) { f.write((const uint8_t*)&m, sizeof m); f.close(); }
}

static void clearRow() {
  for (uint32_t i = 0; i < gridNx; ++i) rowBuf[i] = INVALID16;
  rowHasData = false;
}

// Zeile anhängen – nur aufrufen, wenn die Achsen stehen (Flash-Zugriff pausiert die Step-ISR)
static void flushRow() {
  if (!rowHasData) return;
  File f = LittleFS.open(DATA_FILE, FILE_APPEND);
  if (f) { f.write((const uint8_t*)rowBuf, gridNx * sizeof(int16_t)); f.close(); }
  rowsWritten = rowsWritten + 1;
  clearRow();
}

// Nach Neustart letzten Scan wiederherstellen
static void loadMeta() {
  File f = LittleFS.open(META_FILE, FILE_READ);
  if (!f) return;
  ScanMeta m;
  const bool okm = f.read((uint8_t*)&m, sizeof m) == sizeof m && m.magic == META_MAGIC && m.nx > 0;
  f.close();
  if (!okm) return;
  File d = LittleFS.open(DATA_FILE, FILE_READ);
  const uint32_t rows = d ? d.size() / (m.nx * sizeof(int16_t)) : 0;
  if (d) d.close();
  gridNx = m.nx; gridRes = m.res;
  gridNy = m.complete ? min(m.ny, rows) : rows;
  rowsWritten = gridNy;
  pointsDone = gridNx * gridNy;
  wasAborted = m.partial || !m.complete;
  scanE0 = m.e0; scanE1 = m.e1;
  state = gridNy ? ST_DONE : ST_IDLE;
  Serial.printf("Letzter Scan geladen: %u x %u Punkte%s\n", (unsigned)gridNx, (unsigned)gridNy, wasAborted ? " (Teilscan)" : "");
}

// ------------------------------------------------------------ Scan-Task
static void measurePoint(uint32_t j, uint32_t i, uint16_t avg) {
  (void)j;
  float raw;
  const float d = Sensor::readMm(avg, &raw);
  rowBuf[i] = toQ(d);
  rowHasData = true;
  lastDist = d; lastRaw = raw;
  pointsDone = pointsDone + 1;
}

// Pause an sicherer Stelle: Achsen stehen, Motoren bleiben bestromt (Position bleibt erhalten)
static void pausePoint() {
  if (!pauseReq || abortReq) return;
  Motion::waitDone();
  const ScanState prev = state;
  pauseT0 = millis();
  state = ST_PAUSED;
  while (pauseReq && !abortReq) vTaskDelay(pdMS_TO_TICKS(20));
  pausedMs = pausedMs + (millis() - pauseT0);
  pauseT0 = 0;
  state = prev;
}

// Stop-and-go: jeden Punkt anfahren, beruhigen, messen
static void scanRowStop(uint32_t j, bool rev, const ScanParams& p) {
  if (!p.bidir) {                         // jede Zeile aus -X anfahren
    Motion::moveTo(-PREAPPROACH_MM, j * p.res, p.travel);
    Motion::waitDone();
  }
  for (uint32_t k = 0; k < gridNx && !abortReq; ++k) {
    const uint32_t i = rev ? gridNx - 1 - k : k;
    Motion::moveTo(i * p.res, j * p.res, k == 0 ? p.travel : p.feed);
    Motion::waitDone();
    if (abortReq) break;
    vTaskDelay(pdMS_TO_TICKS(p.settleMs));
    measurePoint(j, i, p.avg);
    pausePoint();                         // Pause nach jedem Punkt möglich
  }
}

// Fliegend: Zeile mit konstanter Geschwindigkeit durchfahren, beim Überfahren jedes Rasterpunkts messen
static void scanRowFly(uint32_t j, bool rev, const ScanParams& p) {
  const float y     = j * p.res;
  const float xEnd  = (gridNx - 1) * p.res;
  const float dir   = rev ? -1.0f : 1.0f;
  // Ausgleich Sensor-/ADC-Verzögerung. Beim ADS wird über das Intervall zum vorigen Punkt gemittelt
  // -> Auslösung eine halbe Rasterweite später, damit das Mittelungsfenster auf dem Punkt zentriert ist.
  const bool  ads   = Sensor::isAds();
  const float lead  = dir * (p.feed * p.latMs * 0.001f + (ads ? 0.5f * p.res : 0.0f));
  const float runIn = p.feed * p.feed / (2.0f * ACCEL_MM_S2) + fabsf(lead) + 0.5f;  // Anlauf bis v konstant
  const float xs    = rev ? xEnd + runIn : -runIn;
  const float xe    = rev ? -runIn : xEnd + runIn;
  const float nearMm = p.feed * 0.002f;                          // letzte ~2 ms aktiv warten

  Motion::moveTo(xs, y, p.travel);
  Motion::waitDone();
  if (abortReq) return;
  Motion::moveTo(xe, y, p.feed);
  Sensor::accReset();
  for (uint32_t k = 0; k < gridNx; ++k) {
    const uint32_t i = rev ? gridNx - 1 - k : k;
    const float xt = i * p.res + lead;
    for (;;) {
      if (abortReq) { Motion::stop(); return; }
      const float rem = (xt - Motion::x()) * dir;
      if (rem <= 0.0f || !Motion::busy()) break;
      if (ads) {
        if (rem <= p.res) Sensor::accPoll();          // nur Werte im Fenster dieses Punkts
        vTaskDelay(1);
      } else if (rem > nearMm) vTaskDelay(1);
    }
    if (ads) {
      float raw;
      const float d = Sensor::accTakeMm(&raw);
      rowBuf[i] = toQ(d); rowHasData = true;
      lastDist = d; lastRaw = raw;
      pointsDone = pointsDone + 1;
    } else measurePoint(j, i, p.avg);
  }
  Motion::waitDone();
}

static void scanTask(void*) {
  const ScanParams p = sp;
  for (uint32_t j = 0; j < gridNy && !abortReq; ++j) {
    const bool rev = p.bidir && (j & 1);
    if (p.fly) scanRowFly(j, rev, p);
    else       scanRowStop(j, rev, p);
    Motion::waitDone();                    // Achsen stehen -> Zeile in den Flash
    flushRow();
    pausePoint();                         // fliegend: Pause am Zeilenende
  }
  const bool aborted = abortReq;
  state = ST_RETURN;
  Motion::waitDone();
  Motion::moveTo(0, 0, p.travel);          // zurück zum Startpunkt
  Motion::waitDone();
  scanT1 = millis();
  scanE1 = nowEpoch();
  if (!scanE0 && scanE1) scanE0 = scanE1 - (scanT1 - scanT0) / 1000;   // NTP kam erst während des Scans
  if (aborted) {                           // bereits Gescanntes als fertigen Scan übernehmen
    if (rowsWritten) { gridNy = rowsWritten; wasAborted = true; writeMeta(gridNy, true, true); state = ST_DONE; }
    else             { gridNx = gridNy = 0; LittleFS.remove(META_FILE); state = ST_ABORTED; }
  } else { writeMeta(gridNy, true, false); state = ST_DONE; }
  abortReq = false;
  scanTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

// ------------------------------------------------------------ Rahmen abfahren
// Fährt den Scanbereich ab aktueller Position einmal als Rechteck ab und endet wieder am Startpunkt.
static void frameTask(void*) {
  const ScanState prev = state;
  state = ST_FRAME;
  const float x0 = Motion::x(), y0 = Motion::y();
  const float P[4][2] = { { x0 + frW, y0 }, { x0 + frW, y0 + frH }, { x0, y0 + frH }, { x0, y0 } };
  for (int k = 0; k < 4 && !abortReq; ++k) {
    Motion::moveTo(P[k][0], P[k][1], frF);
    Motion::waitDone();
  }
  state = prev;
  abortReq = false;
  frameTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

// ------------------------------------------------------------ Helfer
static bool  scanRunning() { return scanTaskHandle != nullptr; }
static bool  taskRunning() { return scanTaskHandle != nullptr || frameTaskHandle != nullptr; }
static float argF(const char* n, float def) { return server.hasArg(n) ? server.arg(n).toFloat() : def; }
static void  ok() { server.send(200, "text/plain", "OK"); }
static void  err(int code, const char* m) { server.send(code, "text/plain", m); }
static void  fmtNum(char* b, size_t n, float v) { if (isnan(v)) strcpy(b, "null"); else snprintf(b, n, "%.4f", v); }

// ------------------------------------------------------------ Handler
static void handleStatus() {
  float raw = lastRaw, d = lastDist;
  if (!scanRunning()) { d = Sensor::readMm(16, &raw); lastDist = d; lastRaw = raw; }
  char ds[16], rs[16], buf[700];
  fmtNum(ds, sizeof ds, d);
  fmtNum(rs, sizeof rs, raw);
  snprintf(buf, sizeof buf,
    "{\"state\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"en\":%d,\"busy\":%d,\"dist\":%s,\"raw\":%s,"
    "\"mode\":\"%s\",\"nx\":%u,\"ny\":%u,\"res\":%.4f,\"done\":%u,\"total\":%u,"
    "\"acc\":%.1f,\"spm\":%.4f,\"rows\":%u,\"fsfree\":%u,\"fstotal\":%u,\"fw\":\"%s\",\"partial\":%d,\"adc\":\"%s\",\"cs\":%.6f,\"co\":%.4f,\"el\":%u,\"ago\":%u,\"e0\":%u,\"e1\":%u,\"now\":%u}",
    STATE_NAMES[state], Motion::x(), Motion::y(), (int)Motion::isEnabled(), (int)Motion::busy(),
    ds, rs, SENSOR_MODE_CURRENT ? "I" : "U",
    (unsigned)gridNx, (unsigned)gridNy, gridRes, (unsigned)pointsDone, (unsigned)(gridNx * gridNy),
    ACCEL_MM_S2, STEPS_PER_MM, (unsigned)rowsWritten,
    (unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes()), (unsigned)LittleFS.totalBytes(), FW_BUILD, (int)wasAborted,
    Sensor::isAds() ? "ADS1115" : "ESP32", Sensor::calScale(), Sensor::calOffset(),
    (unsigned)(scanT0 ? (scanT1 ? scanT1 : millis()) - scanT0 - pausedMs - (pauseT0 ? millis() - pauseT0 : 0) : 0),   // Laufzeit ms ohne Pausen
    (unsigned)(scanT1 ? millis() - scanT1 : 0),                                // ms seit Ende
    (unsigned)scanE0, (unsigned)scanE1, (unsigned)nowEpoch());
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

static void handleScan() {
  if (taskRunning())   return err(409, "Scan laeuft bereits");
  if (Motion::busy())  return err(409, "Achsen in Bewegung");

  ScanParams p;
  p.w        = argF("w", 0);
  p.h        = argF("h", 0);
  p.res      = argF("res", 1);
  p.feed     = argF("feed", DEFAULT_SCAN_SPEED);
  p.travel   = argF("travel", DEFAULT_TRAVEL_SPEED);
  p.settleMs = (uint32_t)max(0.0f, argF("settle", DEFAULT_SETTLE_MS));
  p.avg      = (uint16_t)constrain((int)argF("avg", DEFAULT_AVG), 1, 1024);
  p.bidir    = argF("bidir", 1) != 0;
  p.fly      = argF("fly", 0) != 0;
  p.latMs    = constrain(argF("lat", 2), -50.0f, 50.0f);

  if (!(p.res >= 1.0f / STEPS_PER_MM) || p.w < 0 || p.h < 0 || p.feed <= 0 || p.travel <= 0)
    return err(400, "Ungueltige Parameter");

  const uint32_t nx = (uint32_t)floorf(p.w / p.res + 1e-4f) + 1;
  const uint32_t ny = (uint32_t)floorf(p.h / p.res + 1e-4f) + 1;
  const uint64_t n  = (uint64_t)nx * ny;
  if (n > MAX_POINTS) return err(400, "Zu viele Punkte");

  // Platz im Flash prüfen (alter Scan wird überschrieben)
  uint64_t oldSize = 0;
  { File o = LittleFS.open(DATA_FILE, FILE_READ); if (o) { oldSize = o.size(); o.close(); } }
  const uint64_t fsFree = (uint64_t)(LittleFS.totalBytes() - LittleFS.usedBytes()) + oldSize;
  const uint64_t usable = fsFree * 95 / 100 > 16384 ? fsFree * 95 / 100 - 16384 : 0;
  if (n * sizeof(int16_t) > usable) {
    char m[96];
    snprintf(m, sizeof m, "Nicht genug Flash: max. ca. %u Punkte", (unsigned)(usable / sizeof(int16_t)));
    return err(507, m);
  }
  gridNx = gridNy = 0; rowsWritten = 0; pointsDone = 0; state = ST_IDLE;
  LittleFS.remove(DATA_FILE);
  LittleFS.remove(META_FILE);
  if (nx > rowBufN) {
    free(rowBuf);
    rowBuf = (int16_t*)malloc(nx * sizeof(int16_t));
    rowBufN = rowBuf ? nx : 0;
    if (!rowBuf) return err(507, "Nicht genug RAM fuer Zeilenpuffer");
  }

  gridNx = nx; gridNy = ny; gridRes = p.res;
  clearRow();
  scanE0 = nowEpoch(); scanE1 = 0;
  writeMeta(ny, false, false);
  pointsDone = 0; sp = p; abortReq = false; wasAborted = false;
  Motion::setZero();                       // aktuelle Position = Startpunkt
  Motion::enable(true);
  state = ST_SCAN;
  scanT0 = millis(); scanT1 = 0; pausedMs = 0; pauseT0 = 0; pauseReq = false;
  if (xTaskCreatePinnedToCore(scanTask, "scan", 6144, nullptr, 2, &scanTaskHandle, 1) != pdPASS) {
    state = ST_IDLE;
    return err(500, "Task-Start fehlgeschlagen");
  }
  ok();
}

// /api/data?from=R  -> Zeilen R .. rowsWritten-1 (Header X-Row-From / X-Row-To)
static void handleData() {
  if (!gridNx) return err(404, "Keine Daten");
  const uint32_t to   = rowsWritten;
  const uint32_t from = min((uint32_t)max(0.0f, argF("from", 0)), to);
  const size_t rowB   = (size_t)gridNx * sizeof(int16_t);
  const size_t len    = (size_t)(to - from) * rowB;
  File f;
  if (len) {
    f = LittleFS.open(DATA_FILE, FILE_READ);
    if (!f || !f.seek(from * rowB)) return err(500, "Datei-Lesefehler");
  }
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Row-From", String(from));
  server.sendHeader("X-Row-To", String(to));
  server.setContentLength(len);
  server.send(200, "application/octet-stream", "");
  static uint8_t buf[4096];
  for (size_t o = 0; o < len;) {
    const size_t n = f.read(buf, min(sizeof buf, len - o));
    if (!n) break;
    server.sendContent((const char*)buf, n);
    o += n;
  }
  if (f) f.close();
}

static void handleFrame() {
  if (taskRunning() || Motion::busy()) return err(409, "Beschaeftigt");
  frW = argF("w", 0); frH = argF("h", 0); frF = argF("f", DEFAULT_TRAVEL_SPEED);
  if (frW < 0 || frH < 0 || frF <= 0) return err(400, "Ungueltige Parameter");
  abortReq = false;
  if (xTaskCreatePinnedToCore(frameTask, "frame", 4096, nullptr, 2, &frameTaskHandle, 1) != pdPASS)
    return err(500, "Task-Start fehlgeschlagen");
  ok();
}

static void handleFormat() {
  if (taskRunning() || Motion::busy()) return err(409, "Beschaeftigt");
  LittleFS.end();
  const bool f = LittleFS.format();
  LittleFS.begin(true);
  gridNx = gridNy = 0; rowsWritten = 0; pointsDone = 0; wasAborted = false; state = ST_IDLE;
  Serial.printf("LittleFS formatiert (%s): %u kB gesamt\n", f ? "ok" : "FEHLER", (unsigned)(LittleFS.totalBytes() / 1024));
  if (!f) return err(500, "Formatieren fehlgeschlagen");
  ok();
}

// Messung ohne Kalibrierung für die Zwei-Punkt-Kalibrierung
static void handleMeasure() {
  if (taskRunning()) return err(409, "Beschaeftigt");
  float raw;
  const uint16_t n = (uint16_t)constrain((int)argF("n", 64), 1, 1024);
  const float d = Sensor::readNominalMm(n, &raw);
  char ds[16], rs[16], buf[96];
  fmtNum(ds, sizeof ds, d); fmtNum(rs, sizeof rs, raw);
  snprintf(buf, sizeof buf, "{\"dnom\":%s,\"raw\":%s}", ds, rs);
  server.send(200, "application/json", buf);
}

static void handleCal() {
  if (server.hasArg("reset")) { Sensor::setCal(1.0f, 0.0f); return ok(); }
  const float s = argF("s", 1), o = argF("o", 0);
  if (!(s > 0.8f && s < 1.25f) || !isfinite(o)) return err(400, "Kalibrierwerte unplausibel");
  Sensor::setCal(s, o);
  ok();
}

static void handleJog() {
  if (taskRunning() || Motion::busy()) return err(409, "Beschaeftigt");
  Motion::moveRel(argF("dx", 0), argF("dy", 0), argF("f", DEFAULT_TRAVEL_SPEED));
  ok();
}

static void handleHome() {
  if (taskRunning() || Motion::busy()) return err(409, "Beschaeftigt");
  Motion::moveTo(0, 0, argF("f", DEFAULT_TRAVEL_SPEED));
  ok();
}

static void handleZero() {
  if (taskRunning() || !Motion::setZero()) return err(409, "Beschaeftigt");
  ok();
}

static void handlePause() {
  if (!scanRunning()) return err(409, "Kein Scan aktiv");
  pauseReq = server.hasArg("on") ? argF("on", 1) != 0 : !pauseReq;
  server.send(200, "text/plain", pauseReq ? "Pause" : "Weiter");
}

static void handleStop() {
  if (taskRunning()) abortReq = true;
  Motion::stop();
  ok();
}

static void handleEnable() {
  const bool on = argF("on", 1) != 0;
  if (!on && taskRunning()) { abortReq = true; Motion::stop(); return err(409, "Scan wird abgebrochen - erneut ausschalten"); }
  Motion::enable(on);
  ok();
}

// ------------------------------------------------------------ OTA
static void handleUpload() {
  HTTPUpload& up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      Serial.printf("OTA: %s\n", up.filename.c_str());
      if (taskRunning()) abortReq = true;
      Motion::inhibit(true);                       // Motoren stoppen + abschalten, keine Bewegung mehr
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
      break;
    case UPLOAD_FILE_WRITE:
      if (!Update.hasError() && Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
      break;
    case UPLOAD_FILE_END:
      if (Update.end(true)) Serial.printf("OTA ok: %u Bytes\n", (unsigned)up.totalSize);
      else Update.printError(Serial);
      break;
    case UPLOAD_FILE_ABORTED:
      Update.abort();
      Motion::inhibit(false);
      Serial.println("OTA abgebrochen");
      break;
    default: break;
  }
}

static void handleUpdateDone() {
  const bool good = !Update.hasError() && Update.isFinished();
  server.sendHeader("Connection", "close");
  server.send(good ? 200 : 500, "text/plain", good ? "OK" : Update.errorString());
  if (good) { delay(500); ESP.restart(); }
  Motion::inhibit(false);
}

// ------------------------------------------------------------ Setup / Loop
void setup() {
  Serial.begin(115200);
  Motion::begin();
  if (!Sensor::begin()) Serial.println("ADS1115 nicht gefunden - Verdrahtung/Adresse pruefen!");
  if (!LittleFS.begin(true)) Serial.println("LittleFS FEHLER - Partitionsschema mit SPIFFS waehlen!");
  else {
    Serial.printf("LittleFS: %u kB gesamt, %u kB frei\n", (unsigned)(LittleFS.totalBytes() / 1024),
                  (unsigned)((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024));
    loadMeta();
  }

  delay(500);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);     // direkt aus eFuse, unabhängig vom WLAN-Status
  Serial.printf("\nMAC-Adresse: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  WiFi.onEvent([](WiFiEvent_t ev, WiFiEventInfo_t info) {
    switch (ev) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("WLAN: mit AP verbunden, warte auf IP ...");
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("WLAN: IP %s  GW %s\n", WiFi.localIP().toString().c_str(),
                      WiFi.gatewayIP().toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
        const uint8_t r = info.wifi_sta_disconnected.reason;
        Serial.printf("WLAN getrennt, Grund %u: %s\n", r,
                      WiFi.disconnectReasonName((wifi_err_reason_t)r));
        break;
      }
      default: break;
    }
  });
#if USE_STATIC_IP
  IPAddress ip(LOCAL_IP), gw(GATEWAY_IP), sn(SUBNET_MASK), dns(DNS_IP);
  WiFi.config(ip, gw, sn, dns);
#endif
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);   // läuft im Hintergrund, synchronisiert sobald WLAN steht
  for (uint32_t t = millis(); WiFi.status() != WL_CONNECTED && millis() - t < 20000;) delay(250);
  Serial.printf("Status %d  IP: %s  Steps/mm: %.3f\n", (int)WiFi.status(),
                WiFi.localIP().toString().c_str(), STEPS_PER_MM);

  server.on("/",            HTTP_GET,  [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status",  HTTP_GET,  handleStatus);
  server.on("/api/data",    HTTP_GET,  handleData);
  server.on("/api/scan",    HTTP_POST, handleScan);
  server.on("/api/stop",    HTTP_POST, handleStop);
  server.on("/api/pause",   HTTP_POST, handlePause);
  server.on("/api/jog",     HTTP_POST, handleJog);
  server.on("/api/frame",   HTTP_POST, handleFrame);
  server.on("/api/format",  HTTP_POST, handleFormat);
  server.on("/api/measure", HTTP_POST, handleMeasure);
  server.on("/api/cal",     HTTP_POST, handleCal);
  server.on("/api/home",    HTTP_POST, handleHome);
  server.on("/api/zero",    HTTP_POST, handleZero);
  server.on("/api/enable",  HTTP_POST, handleEnable);
  server.on("/update",      HTTP_POST, handleUpdateDone, handleUpload);
  server.begin();
}

void loop() {
  server.handleClient();

  // Motoren nach IDLE_DISABLE_MS ohne Bewegung abschalten
  if (taskRunning() || Motion::busy() || !Motion::isEnabled()) lastActivity = millis();
  else if (millis() - lastActivity > IDLE_DISABLE_MS) {
    Motion::enable(false);
    Serial.println("Motoren wegen Inaktivitaet abgeschaltet");
  }
  // Fallback, falls Auto-Reconnect hängen bleibt: nach 60 s ohne Verbindung neu starten
  static uint32_t tLost = 0;
  if (WiFi.status() == WL_CONNECTED) tLost = millis();
  else if (millis() - tLost > 60000) {
    tLost = millis();
    Serial.println("WLAN: Neuverbindung ...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  delay(1);
}
