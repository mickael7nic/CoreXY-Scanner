// CoreXY Laser-Oberflächenscanner – ESP32, Arduino-ESP32 Core >= 3.0
// Scandaten liegen im Flash (LittleFS) -> Partitionsschema "Default 4MB with spiffs"
// Web-UI: http://192.168.55.113/

#include <WiFi.h>
#include <WebServer.h>
#include <esp_mac.h>
#include <Update.h>
#include <LittleFS.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "secrets.h"
#include "motion.h"
#include "sensor.h"
#include "webpage.h"

WebServer server(80);

enum ScanState : uint8_t { ST_IDLE, ST_SCAN, ST_RETURN, ST_DONE, ST_ABORTED, ST_FRAME, ST_PAUSED, ST_CONTOUR };
static const char* STATE_NAMES[] = { "IDLE", "SCAN", "RETURN", "DONE", "ABORTED", "FRAME", "PAUSED", "CONTOUR" };

struct ScanParams { float w, h, res, feed, travel, latMs; uint32_t settleMs; uint16_t avg; bool bidir, fly; };

static ScanParams         sp;
// Scandaten: Zeile für Zeile im Flash, Rohabstand in 0.01 mm als int16, INVALID16 = ungültig
static constexpr int16_t  INVALID16 = INT16_MIN;
static const char*        DATA_FILE = "/scan.bin";
static const char*        META_FILE = "/scan.meta";
static const char*        CONT_FILE = "/contour.json";   // Konturen (vom Browser verwaltet, hier nur gespeichert)
struct ScanMeta { uint32_t magic, nx, ny; float res; uint8_t complete, partial, pad[2]; uint32_t e0, e1; float ox, oy; uint32_t total; };
static constexpr uint32_t META_MAGIC = 0x53434E33;   // "SCN3" (mit Zeiten, Ursprung, Punktzahl)
static int16_t*           rowBuf = nullptr;          // aktuelle Zeile im RAM
static uint32_t           rowBufN = 0;
static bool               rowHasData = false;
static volatile uint32_t  rowsWritten = 0;
static inline int16_t toQ(float d) { return isnan(d) ? INVALID16 : (int16_t)constrain(lroundf(d * 100.0f), -32767L, 32767L); }
static uint32_t           gridNx = 0, gridNy = 0;
static float              gridRes = 0;
static float              gridOx = 0, gridOy = 0;   // Maschinenkoordinate von Rasterpunkt (0,0)
static uint32_t           totalPts = 0;             // zu messende Punkte (bei Maske < nx*ny)
static float              scanSX = 0, scanSY = 0;   // Startposition (Rückfahrt)
// Maske: je Zeile Abschnitte [a,b] (Rasterindizes), Zeile j nutzt maskIv[2*maskIdx[j] .. 2*maskIdx[j+1]-1]
static bool               maskOn = false;
static uint32_t*          maskIdx = nullptr;
static uint16_t*          maskIv = nullptr;
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
  ScanMeta m{ META_MAGIC, gridNx, ny, gridRes, (uint8_t)complete, (uint8_t)partial, {0, 0}, scanE0, scanE1, gridOx, gridOy, totalPts };
  File f = LittleFS.open(META_FILE, FILE_WRITE);
  if (f) { f.write((const uint8_t*)&m, sizeof m); f.close(); }
}

static void clearRow() {
  for (uint32_t i = 0; i < gridNx; ++i) rowBuf[i] = INVALID16;
  rowHasData = false;
}

// Zeile anhängen – nur aufrufen, wenn die Achsen stehen (Flash-Zugriff pausiert die Step-ISR)
static void flushRow(bool force = false) {   // force: auch leere Zeile schreiben (Maske)
  if (!rowHasData && !force) return;
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
  gridOx = m.ox; gridOy = m.oy;
  totalPts = m.total ? m.total : gridNx * gridNy;
  pointsDone = totalPts;
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

static inline float gX(uint32_t i) { return gridOx + i * gridRes; }
static inline float gY(uint32_t j) { return gridOy + j * gridRes; }

// Stop-and-go: jeden Punkt des Abschnitts [a,b] anfahren, beruhigen, messen
static void scanSegStop(uint32_t j, uint32_t a, uint32_t b, bool rev, const ScanParams& p) {
  if (!p.bidir) {                         // jeden Abschnitt aus -X anfahren
    Motion::moveTo(gX(a) - PREAPPROACH_MM, gY(j), p.travel);
    Motion::waitDone();
  }
  for (uint32_t k = 0; k <= b - a && !abortReq; ++k) {
    const uint32_t i = rev ? b - k : a + k;
    Motion::moveTo(gX(i), gY(j), k == 0 ? p.travel : p.feed);
    Motion::waitDone();
    if (abortReq) break;
    vTaskDelay(pdMS_TO_TICKS(p.settleMs));
    measurePoint(j, i, p.avg);
    pausePoint();                         // Pause nach jedem Punkt möglich
  }
}

// Fliegend: Abschnitt [a,b] mit konstanter Geschwindigkeit durchfahren, beim Überfahren jedes Rasterpunkts messen
static void scanSegFly(uint32_t j, uint32_t a, uint32_t b, bool rev, const ScanParams& p) {
  const float y     = gY(j);
  const float dir   = rev ? -1.0f : 1.0f;
  // Ausgleich Sensor-/ADC-Verzögerung. Beim ADS wird über das Intervall zum vorigen Punkt gemittelt
  // -> Auslösung eine halbe Rasterweite später, damit das Mittelungsfenster auf dem Punkt zentriert ist.
  const bool  ads   = Sensor::isAds();
  const float lead  = dir * (p.feed * p.latMs * 0.001f + (ads ? 0.5f * p.res : 0.0f));
  const float runIn = p.feed * p.feed / (2.0f * ACCEL_MM_S2) + fabsf(lead) + 0.5f;  // Anlauf bis v konstant
  const float xs    = rev ? gX(b) + runIn : gX(a) - runIn;
  const float xe    = rev ? gX(a) - runIn : gX(b) + runIn;
  const float nearMm = p.feed * 0.002f;                          // letzte ~2 ms aktiv warten

  Motion::moveTo(xs, y, p.travel);
  Motion::waitDone();
  if (abortReq) return;
  Motion::moveTo(xe, y, p.feed);
  Sensor::accReset();
  for (uint32_t k = 0; k <= b - a; ++k) {
    const uint32_t i = rev ? b - k : a + k;
    const float xt = gX(i) + lead;
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
  uint32_t rowCount = 0;                   // Zeilen mit Abschnitten (für Serpentine)
  for (uint32_t j = 0; j < gridNy && !abortReq; ++j) {
    uint32_t n0 = 0, n1 = 1;               // ohne Maske: ein Abschnitt [0, nx-1]
    if (maskOn) { n0 = maskIdx[j]; n1 = maskIdx[j + 1]; }
    if (n1 > n0) {
      const bool rev = p.bidir && (rowCount++ & 1);
      for (uint32_t q = 0; q < n1 - n0 && !abortReq; ++q) {
        const uint32_t k = rev ? n1 - 1 - q : n0 + q;
        const uint32_t a = maskOn ? maskIv[2 * k] : 0, b = maskOn ? maskIv[2 * k + 1] : gridNx - 1;
        if (p.fly) scanSegFly(j, a, b, rev, p);
        else       scanSegStop(j, a, b, rev, p);
      }
    }
    Motion::waitDone();                    // Achsen stehen -> Zeile in den Flash
    if (!abortReq || rowHasData) flushRow(true);
    pausePoint();                          // fliegend: Pause am Zeilenende
  }
  const bool aborted = abortReq;
  state = ST_RETURN;
  Motion::waitDone();
  Motion::moveTo(scanSX, scanSY, p.travel);   // zurück zum Startpunkt
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

// ------------------------------------------------------------ Kontur abfahren
// Ablauf: Bezugshöhe (Auflage) am Start messen -> in Suchrichtung schrittweise fahren bis Material
// (Höhe >= Kantenhöhe, Treffer wird bestätigt) -> Kante mit Moore-Nachbarschaftsverfolgung auf einem
// Gitter der Schrittweite umrunden: um den aktuellen Randpunkt im Uhrzeigersinn die 8 Nachbarn prüfen,
// der erste Materialpunkt ist der nächste Randpunkt. Jeder Übergang frei->Material ist eine Kante;
// deren Lage wird per Intervallhalbierung verfeinert. Ende, wenn der Startpunkt wieder auf demselben
// Weg erreicht wird (Jacob-Kriterium). Gemessene Gitterpunkte werden zwischengespeichert.
// Robustheit: ungültige Messungen (Glanz, Schatten) entscheiden die 4 Nachbarn; unplausible
// Einzelwerte (Material zwischen lauter freien Punkten o. ä.) werden zweimal nachgemessen.
struct ContourParams { float step, thr, maxExt, feed; uint16_t avg; uint32_t settleMs; uint8_t dir, refine; };
static ContourParams      cp;
static TaskHandle_t       contourTaskHandle = nullptr;
static float*             cPts = nullptr;           // Kantenpunkte x,y (mm)
static uint32_t           cCap = 0;
static volatile uint32_t  cN = 0;
static volatile uint8_t   cState = 0;               // 0 bereit, 1 läuft, 2 fertig, 3 Fehler
static volatile uint32_t  cId = 0;                  // zählt abgeschlossene Läufe
static volatile bool      cClosed = false;
static uint8_t            cMode = 0;                // 0 Außenkontur, 1 Innenkontur
static char               cErr[64] = "";
static uint32_t*          hKey = nullptr;           // Cache gemessener Gitterpunkte (offene Adressierung)
static int8_t*            hVal = nullptr;
static uint32_t           hCap = 0, hUsed = 0;
static float              cx0 = 0, cy0 = 0, cBase = 0;
static uint64_t           edgeRing[32];
static uint8_t            edgePos = 0;
static const int8_t       DXS[4] = { 1, 0, -1, 0 }, DYS[4] = { 0, 1, 0, -1 };   // +X, +Y, -X, -Y
static const int8_t       D8X[8] = { 1, 1, 0, -1, -1, -1, 0, 1 }, D8Y[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };   // gegen den Uhrzeigersinn

static inline uint32_t gKey(int32_t i, int32_t j) {   // 0 ist reserviert (leer)
  return ((uint32_t)(uint16_t)(i + 32768) << 16) | (uint16_t)(j + 32768);
}
static int8_t hGet(uint32_t k) {
  uint32_t h = (k * 2654435761u) & (hCap - 1);
  for (uint32_t n = 0; n < hCap; ++n) {
    if (!hKey[h]) return -1;
    if (hKey[h] == k) return hVal[h];
    h = (h + 1) & (hCap - 1);
  }
  return -1;
}
static void hPut(uint32_t k, int8_t v) {
  if (hUsed * 5 >= hCap * 4) return;                  // Cache voll -> ohne Cache weiter (misst ggf. doppelt)
  uint32_t h = (k * 2654435761u) & (hCap - 1);
  while (hKey[h] && hKey[h] != k) h = (h + 1) & (hCap - 1);
  if (!hKey[h]) hUsed++;
  hKey[h] = k; hVal[h] = v;
}

// Punkt anfahren und messen: 1 = Material, 0 = frei, -1 = kein gültiger Messwert
static int8_t probeXY(float x, float y) {
  Motion::moveTo(x, y, cp.feed);
  Motion::waitDone();
  vTaskDelay(pdMS_TO_TICKS(cp.settleMs));
  float raw = NAN, d = NAN;
  for (int t = 0; t < 3 && isnan(d); ++t) d = Sensor::readMm(cp.avg, &raw);
  lastDist = d; lastRaw = raw;
  if (isnan(d)) return -1;
  const float h = cBase - d;
  if (h >= cp.thr) return 1;
  if (fabsf(h) < cp.thr * 0.5f) cBase += 0.05f * (d - cBase);   // Auflage nachführen (Schräglage)
  return 0;
}

static inline float gpx(int32_t i) { return cx0 + i * cp.step; }
static inline float gpy(int32_t j) { return cy0 + j * cp.step; }

// Messwert eines Gitterpunkts mit Nachbarn abgleichen
static int8_t classify(int32_t i, int32_t j, int8_t v) {
  if (v < 0) {                                      // ungültig: die 4 direkten Nachbarn entscheiden
    int m = 0, f = 0;
    for (int k = 0; k < 4; ++k) {
      const int32_t ni = i + DXS[k], nj = j + DYS[k];
      if (fabsf(ni * cp.step) > cp.maxExt || fabsf(nj * cp.step) > cp.maxExt) continue;
      const uint32_t nk = gKey(ni, nj);
      int8_t c = hGet(nk);
      if (c < 0) { c = probeXY(gpx(ni), gpy(nj)); if (c >= 0) hPut(nk, c); }
      if (c == 1) ++m; else if (c == 0) ++f;
    }
    return m > f ? 1 : 0;
  }
  int m = 0, f = 0;
  for (int k = 0; k < 8; ++k) {
    const int8_t c = hGet(gKey(i + D8X[k], j + D8Y[k]));
    if (c == 1) ++m; else if (c == 0) ++f;
  }
  if ((v == 1 && f >= 3 && m == 0) || (v == 0 && m >= 5 && f == 0)) {   // unplausibel -> nachmessen
    int votes = v, n = 1;
    for (int t = 0; t < 2; ++t) { const int8_t r = probeXY(gpx(i), gpy(j)); if (r >= 0) { votes += r; ++n; } }
    return votes * 2 > n ? 1 : 0;
  }
  return v;
}

// Gitterpunkt (i,j) relativ zum Start: gecacht, -2 = außerhalb der max. Ausdehnung
static int8_t probeIJ(int32_t i, int32_t j) {
  if (fabsf(i * cp.step) > cp.maxExt || fabsf(j * cp.step) > cp.maxExt) return -2;
  const uint32_t k = gKey(i, j);
  int8_t v = hGet(k);
  if (v >= 0) return v;
  v = classify(i, j, probeXY(gpx(i), gpy(j)));
  hPut(k, v);
  return v;
}

// Kante zwischen freiem Punkt a und Materialpunkt b eintragen (Lage per Halbierung verfeinert)
static void addEdge(int32_t ai, int32_t aj, int32_t bi, int32_t bj) {
  const uint32_t ka = gKey(ai, aj), kb = gKey(bi, bj);
  const uint64_t ek = ka < kb ? ((uint64_t)ka << 32) | kb : ((uint64_t)kb << 32) | ka;
  for (uint64_t e : edgeRing) if (e == ek) return;   // gerade schon erfasst
  edgeRing[edgePos++ & 31] = ek;
  if (cN >= cCap) return;
  float lx = cx0 + ai * cp.step, ly = cy0 + aj * cp.step, hx = cx0 + bi * cp.step, hy = cy0 + bj * cp.step;
  for (uint8_t r = 0; r < cp.refine && !abortReq; ++r) {
    const float mx = 0.5f * (lx + hx), my = 0.5f * (ly + hy);
    if (probeXY(mx, my) == 1) { hx = mx; hy = my; } else { lx = mx; ly = my; }
  }
  cPts[2 * cN]     = 0.5f * (lx + hx);
  cPts[2 * cN + 1] = 0.5f * (ly + hy);
  cN = cN + 1;
}

static void contourFail(const char* m) { strlcpy(cErr, m, sizeof cErr); cState = 3; }

static void contourTask(void*) {
  const ScanState prev = state;
  state = ST_CONTOUR;
  cState = 1; cN = 0; cClosed = false; cErr[0] = 0;
  hUsed = 0; memset(hKey, 0, hCap * sizeof(uint32_t));
  memset(edgeRing, 0, sizeof edgeRing); edgePos = 0;
  cx0 = Motion::x(); cy0 = Motion::y();

  // Bezugshöhe der Auflage am Startpunkt
  vTaskDelay(pdMS_TO_TICKS(cp.settleMs + 50));
  float raw = NAN, d = NAN;
  for (int t = 0; t < 3 && isnan(d); ++t) d = Sensor::readMm(cp.avg * 4, &raw);
  if (isnan(d)) contourFail("Kein gueltiger Messwert am Startpunkt");
  else {
    cBase = d;
    hPut(gKey(0, 0), 0);
    const uint8_t d0 = cp.dir;
    int32_t i = 0, j = 0;
    bool found = false;
    // 1. Kante suchen (Treffer bestätigen: nachmessen und der Punkt dahinter muss auch Material sein,
    //    sonst war es eine Fehlmessung / ein Krümel auf der Auflage)
    const int32_t nmax = (int32_t)(cp.maxExt / cp.step);
    for (int32_t n = 1; n <= nmax; ++n) {
      pausePoint();
      if (abortReq) break;
      const int32_t ti = i + DXS[d0], tj = j + DYS[d0];
      const int8_t v = probeIJ(ti, tj);
      if (v < 0) break;
      if (v == 1) {
        int votes = 0;
        for (int t = 0; t < 2; ++t) if (probeXY(gpx(ti), gpy(tj)) == 1) ++votes;
        const int8_t v2 = votes ? probeIJ(ti + DXS[d0], tj + DYS[d0]) : 0;
        if (votes && v2 == 1) { addEdge(i, j, ti, tj); i = ti; j = tj; found = true; break; }
        hPut(gKey(ti, tj), 0);                         // als frei merken (Cache überschreiben)
      }
      i = ti; j = tj;
    }
    if (abortReq) contourFail("Abgebrochen");
    else if (!found) contourFail("Keine Kante im Suchweg gefunden");
    else {
      // 2. Kante umrunden (Moore-Nachbarschaft, im Uhrzeigersinn um den aktuellen Randpunkt)
      const int32_t si = i, sj = j, sbi = i - DXS[d0], sbj = j - DYS[d0];
      int32_t ci = si, cj = sj, bi = sbi, bj = sbj;
      uint32_t steps = 0;
      for (;;) {
        pausePoint();
        if (abortReq) { contourFail("Abgebrochen"); break; }
        int k0 = 0;
        for (int k = 0; k < 8; ++k) if (D8X[k] == bi - ci && D8Y[k] == bj - cj) { k0 = k; break; }
        int32_t lfi = bi, lfj = bj;
        bool fnd = false, ext = false;
        for (int q = 1; q <= 8; ++q) {
          const int k = (k0 - q + 8) & 7;
          const int32_t ni = ci + D8X[k], nj = cj + D8Y[k];
          const int8_t v = probeIJ(ni, nj);
          if (v < 0) { ext = true; break; }
          if (v == 1) { addEdge(lfi, lfj, ni, nj); bi = lfi; bj = lfj; ci = ni; cj = nj; fnd = true; break; }
          lfi = ni; lfj = nj;
        }
        if (ext) { contourFail("Maximale Ausdehnung ueberschritten"); break; }
        if (!fnd) { cClosed = true; cState = 2; break; }           // einzelner Punkt
        if (ci == si && cj == sj && bi == sbi && bj == sbj) { cClosed = true; cState = 2; break; }
        if (++steps > CONTOUR_MAX_STEPS) { contourFail("Zu viele Schritte"); break; }
        if (cN >= cCap) { contourFail("Zu viele Konturpunkte"); break; }
      }
    }
  }
  const bool aborted = abortReq;
  Motion::waitDone();
  if (!aborted) { Motion::moveTo(cx0, cy0, cp.feed); Motion::waitDone(); }   // zurück zum Startpunkt
  state = prev;
  abortReq = false;
  cId = cId + 1;
  contourTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

// großen Puffer bevorzugt im PSRAM anlegen, sonst im internen RAM
static void* bigAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_malloc(n, MALLOC_CAP_8BIT);
}

// ------------------------------------------------------------ Helfer
static bool  scanRunning() { return scanTaskHandle != nullptr; }
static bool  taskRunning() { return scanTaskHandle != nullptr || frameTaskHandle != nullptr || contourTaskHandle != nullptr; }
static float argF(const char* n, float def) { return server.hasArg(n) ? server.arg(n).toFloat() : def; }
static void  ok() { server.send(200, "text/plain", "OK"); }
static void  err(int code, const char* m) { server.send(code, "text/plain", m); }
static void  fmtNum(char* b, size_t n, float v) { if (isnan(v)) strcpy(b, "null"); else snprintf(b, n, "%.4f", v); }

// ------------------------------------------------------------ Handler
static void handleStatus() {
  float raw = lastRaw, d = lastDist;
  if (!scanRunning() && !contourTaskHandle) { d = Sensor::readMm(16, &raw); lastDist = d; lastRaw = raw; }
  char ds[16], rs[16], buf[1000];
  fmtNum(ds, sizeof ds, d);
  fmtNum(rs, sizeof rs, raw);
  snprintf(buf, sizeof buf,
    "{\"state\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"en\":%d,\"busy\":%d,\"dist\":%s,\"raw\":%s,"
    "\"mode\":\"%s\",\"nx\":%u,\"ny\":%u,\"res\":%.4f,\"done\":%u,\"total\":%u,"
    "\"acc\":%.1f,\"spm\":%.4f,\"rows\":%u,\"fsfree\":%u,\"fstotal\":%u,\"fw\":\"%s\",\"partial\":%d,\"adc\":\"%s\",\"cs\":%.6f,\"co\":%.4f,\"el\":%u,\"ago\":%u,\"e0\":%u,\"e1\":%u,\"now\":%u,\"cst\":%u,\"cn\":%u,\"cid\":%u,\"cm\":%u,\"cerr\":\"%s\",\"ox\":%.4f,\"oy\":%.4f,\"mask\":%d}",
    STATE_NAMES[state], Motion::x(), Motion::y(), (int)Motion::isEnabled(), (int)Motion::busy(),
    ds, rs, SENSOR_MODE_CURRENT ? "I" : "U",
    (unsigned)gridNx, (unsigned)gridNy, gridRes, (unsigned)pointsDone, (unsigned)totalPts,
    ACCEL_MM_S2, STEPS_PER_MM, (unsigned)rowsWritten,
    (unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes()), (unsigned)LittleFS.totalBytes(), FW_BUILD, (int)wasAborted,
    Sensor::isAds() ? "ADS1115" : "ESP32", Sensor::calScale(), Sensor::calOffset(),
    (unsigned)(scanT0 ? (scanT1 ? scanT1 : millis()) - scanT0 - pausedMs - (pauseT0 ? millis() - pauseT0 : 0) : 0),   // Laufzeit ms ohne Pausen
    (unsigned)(scanT1 ? millis() - scanT1 : 0),                                // ms seit Ende
    (unsigned)scanE0, (unsigned)scanE1, (unsigned)nowEpoch(),
    (unsigned)cState, (unsigned)cN, (unsigned)cId, (unsigned)cMode, cErr,
    gridOx, gridOy, (int)maskOn);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

// Scanparameter aus der Anfrage (gemeinsam für normalen und Masken-Scan)
static bool readScanParams(ScanParams& p) {
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
  return p.res >= 1.0f / STEPS_PER_MM && p.w >= 0 && p.h >= 0 && p.feed > 0 && p.travel > 0;
}

// Speicher prüfen, Dateien anlegen, Task starten. Liefert Fehlertext oder nullptr.
static const char* startScanCommon(const ScanParams& p, uint32_t nx, uint32_t ny, float ox, float oy, bool zero, uint32_t total) {
  static char m[96];
  const uint64_t n = (uint64_t)nx * ny;
  if (n > MAX_POINTS) return "Zu viele Punkte";
  // Platz im Flash prüfen (alter Scan wird überschrieben)
  uint64_t oldSize = 0;
  { File o = LittleFS.open(DATA_FILE, FILE_READ); if (o) { oldSize = o.size(); o.close(); } }
  const uint64_t fsFree = (uint64_t)(LittleFS.totalBytes() - LittleFS.usedBytes()) + oldSize;
  const uint64_t usable = fsFree * 95 / 100 > 16384 ? fsFree * 95 / 100 - 16384 : 0;
  if (n * sizeof(int16_t) > usable) {
    snprintf(m, sizeof m, "Nicht genug Flash: max. ca. %u Punkte", (unsigned)(usable / sizeof(int16_t)));
    return m;
  }
  if (nx > rowBufN) {
    free(rowBuf);
    rowBuf = (int16_t*)malloc(nx * sizeof(int16_t));
    rowBufN = rowBuf ? nx : 0;
    if (!rowBuf) return "Nicht genug RAM fuer Zeilenpuffer";
  }
  gridNx = gridNy = 0; rowsWritten = 0; pointsDone = 0; state = ST_IDLE;
  LittleFS.remove(DATA_FILE);
  LittleFS.remove(META_FILE);

  if (zero) Motion::setZero();             // normaler Scan: aktuelle Position = Startpunkt
  scanSX = Motion::x(); scanSY = Motion::y();
  gridNx = nx; gridNy = ny; gridRes = p.res; gridOx = ox; gridOy = oy; totalPts = total;
  clearRow();
  scanE0 = nowEpoch(); scanE1 = 0;
  writeMeta(ny, false, false);
  pointsDone = 0; sp = p; abortReq = false; wasAborted = false;
  Motion::enable(true);
  state = ST_SCAN;
  scanT0 = millis(); scanT1 = 0; pausedMs = 0; pauseT0 = 0; pauseReq = false;
  if (xTaskCreatePinnedToCore(scanTask, "scan", 6144, nullptr, 2, &scanTaskHandle, 1) != pdPASS) {
    state = ST_IDLE;
    return "Task-Start fehlgeschlagen";
  }
  return nullptr;
}

static void handleScan() {
  if (taskRunning())   return err(409, "Scan laeuft bereits");
  if (Motion::busy())  return err(409, "Achsen in Bewegung");
  ScanParams p;
  if (!readScanParams(p)) return err(400, "Ungueltige Parameter");
  const uint32_t nx = (uint32_t)floorf(p.w / p.res + 1e-4f) + 1;
  const uint32_t ny = (uint32_t)floorf(p.h / p.res + 1e-4f) + 1;
  maskOn = false;
  const char* e = startScanCommon(p, nx, ny, 0, 0, true, nx * ny);
  if (e) return err(e[0] == 'N' ? 507 : 400, e);
  ok();
}

// /api/scanmask?ox=&oy=&nx=&ny=&res=...  Body (text/plain): je Zeile "Anzahl a1 b1 a2 b2 ..."
// Scannt nur die Abschnitte innerhalb der Kontur. Der Nullpunkt bleibt erhalten (Koordinaten wie die Kontur).
static void handleScanMask() {
  if (taskRunning())   return err(409, "Scan laeuft bereits");
  if (Motion::busy())  return err(409, "Achsen in Bewegung");
  ScanParams p;
  if (!readScanParams(p)) return err(400, "Ungueltige Parameter");
  const uint32_t nx = (uint32_t)argF("nx", 0), ny = (uint32_t)argF("ny", 0);
  const float ox = argF("ox", 0), oy = argF("oy", 0);
  if (nx < 2 || ny < 1 || nx > 65000) return err(400, "Ungueltiges Raster");
  const String& body = server.arg("plain");
  const char* c = body.c_str();
  // 1. Durchlauf: Abschnitte zählen und prüfen
  uint32_t nIv = 0, total = 0;
  { const char* q = c; char* e;
    for (uint32_t j = 0; j < ny; ++j) {
      const long cnt = strtol(q, &e, 10);
      if (e == q || cnt < 0) return err(400, "Maske unvollstaendig");
      q = e;
      for (long k = 0; k < cnt; ++k) {
        const long a = strtol(q, &e, 10);
        q = e;
        const long b = strtol(q, &e, 10);
        if (e == q || a < 0 || b < a || b >= (long)nx) return err(400, "Maske fehlerhaft");
        q = e;
        total += b - a + 1;
      }
      nIv += cnt;
    } }
  free(maskIdx); free(maskIv); maskIdx = nullptr; maskIv = nullptr;
  maskIdx = (uint32_t*)malloc((ny + 1) * sizeof(uint32_t));
  maskIv  = (uint16_t*)malloc((nIv ? nIv : 1) * 2 * sizeof(uint16_t));
  if (!maskIdx || !maskIv) return err(507, "Nicht genug RAM fuer Maske");
  // 2. Durchlauf: speichern
  { const char* q = c; char* e; uint32_t k = 0;
    for (uint32_t j = 0; j < ny; ++j) {
      maskIdx[j] = k;
      const long cnt = strtol(q, &e, 10); q = e;
      for (long t = 0; t < cnt; ++t) {
        maskIv[2 * k] = (uint16_t)strtol(q, &e, 10); q = e;
        maskIv[2 * k + 1] = (uint16_t)strtol(q, &e, 10); q = e;
        ++k;
      }
    }
    maskIdx[ny] = k; }
  if (!total) return err(400, "Maske leer");
  maskOn = true;
  const char* e = startScanCommon(p, nx, ny, ox, oy, false, total);
  if (e) { maskOn = false; return err(e[0] == 'N' ? 507 : 400, e); }
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

// /api/contour?mode=0|1&step=&thr=&max=&dir=&avg=&settle=&ref=&f=
static void handleContour() {
  if (taskRunning() || Motion::busy()) return err(409, "Beschaeftigt");
  ContourParams p;
  p.step     = argF("step", 0.5f);
  p.thr      = argF("thr", 0.5f);
  p.maxExt   = argF("max", 300);
  p.feed     = argF("f", 30);
  p.avg      = (uint16_t)constrain((int)argF("avg", 16), 1, 256);
  p.settleMs = (uint32_t)max(0.0f, argF("settle", 30));
  p.dir      = (uint8_t)constrain((int)argF("dir", 0), 0, 3);
  p.refine   = (uint8_t)constrain((int)argF("ref", 2), 0, 6);
  if (!(p.step >= 2.0f / STEPS_PER_MM) || !(p.thr > 0) || !(p.maxExt > p.step) || !(p.feed > 0))
    return err(400, "Ungueltige Parameter");
  if (p.maxExt / p.step > 30000) return err(400, "Ausdehnung/Schrittweite zu gross");
  if (!cPts) {                                         // Puffer einmalig, so groß wie möglich
    for (cCap = CONTOUR_MAX_PTS; cCap >= 1000 && !(cPts = (float*)bigAlloc(cCap * 2 * sizeof(float))); cCap /= 2) {}
    if (!cPts) { cCap = 0; return err(507, "Nicht genug RAM fuer Konturpunkte"); }
  }
  if (!hKey) {
    for (hCap = CONTOUR_HASH; hCap >= 2048; hCap /= 2) {
      hKey = (uint32_t*)bigAlloc(hCap * sizeof(uint32_t));
      hVal = (int8_t*)bigAlloc(hCap);
      if (hKey && hVal) break;
      free(hKey); free(hVal); hKey = nullptr; hVal = nullptr;
    }
    if (!hKey) { hCap = 0; return err(507, "Nicht genug RAM fuer Mess-Cache"); }
  }
  cp = p;
  cMode = argF("mode", 0) != 0;
  abortReq = false; pauseReq = false;
  Motion::enable(true);
  if (xTaskCreatePinnedToCore(contourTask, "contour", 6144, nullptr, 2, &contourTaskHandle, 1) != pdPASS)
    return err(500, "Task-Start fehlgeschlagen");
  ok();
}

// /api/cdata -> Konturpunkte der letzten/aktuellen Fahrt als float32 x,y
static void handleCData() {
  const uint32_t n = cN;
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Contour-Id", String(cId));
  server.sendHeader("X-Closed", cClosed ? "1" : "0");
  server.sendHeader("X-Mode", String(cMode));
  const size_t len = (size_t)n * 2 * sizeof(float);
  server.setContentLength(len);
  server.send(200, "application/octet-stream", "");
  const char* p = (const char*)cPts;
  for (size_t o = 0; o < len; o += 4096) server.sendContent(p + o, min((size_t)4096, len - o));
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

// Dauerfahrt solange gedrückt: Ziel weit entfernt, Browser sendet alle 200 ms ein Lebenszeichen.
// Bleibt es aus (Taste losgelassen, WLAN weg, Browser zu), bremst der ESP32 selbst ab.
static volatile bool     jogCont = false;
static volatile uint32_t jogKA = 0;
static void handleJogC() {
  if (taskRunning() || Motion::busy()) return err(409, "Beschaeftigt");
  const float dx = argF("dx", 0), dy = argF("dy", 0), n = hypotf(dx, dy);
  if (n < 1e-6f) return err(400, "Richtung fehlt");
  Motion::moveRel(dx / n * JOG_MAX_MM, dy / n * JOG_MAX_MM, argF("f", DEFAULT_TRAVEL_SPEED));
  jogKA = millis(); jogCont = true;
  ok();
}
static void handleJogKA()   { jogKA = millis(); ok(); }
static void handleJogStop() { if (jogCont) { Motion::stop(); jogCont = false; } ok(); }

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
  if (!scanRunning() && !contourTaskHandle) return err(409, "Kein Scan aktiv");
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

// ------------------------------------------------------------ Scan-Datei vom PC (.cxs) und Konturen
// .cxs: 64-Byte-Kopf (little endian) | nx*ny int16 Rohabstand 0.01 mm | Konturen-JSON (cLen Bytes)
//  0 "CXS1"  4 nx  8 ny  12 res(f32)  16 ox(f32)  20 oy(f32)  24 total  28 e0  32 e1  36 flags(bit0 Teilscan)  40 cLen  44..63 frei
static constexpr uint32_t CXS_MAGIC = 0x31535843;   // "CXS1"
static uint64_t fileSize(const char* n) { File f = LittleFS.open(n, FILE_READ); uint64_t s = f ? f.size() : 0; if (f) f.close(); return s; }
static uint64_t fsUsable(uint64_t reclaim) {         // nutzbarer Platz, wenn 'reclaim' Bytes vorher gelöscht werden
  const uint64_t fr = (uint64_t)(LittleFS.totalBytes() - LittleFS.usedBytes()) + reclaim;
  return fr * 95 / 100 > 16384 ? fr * 95 / 100 - 16384 : 0;
}
static struct {
  uint8_t  hdr[64]; uint32_t hn;
  uint32_t nx, ny, total, e0, e1, flags, cLen; float res, ox, oy;
  uint64_t dataLen, got;
  File     f;
  bool     fail; int code; char msg[80];
} rst;

static void restoreFail(int code, const char* m) {
  if (rst.f) rst.f.close();
  if (!rst.fail) { rst.fail = true; rst.code = code; strlcpy(rst.msg, m, sizeof rst.msg); }
}

static void restoreHeader() {
  auto u32 = [](const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; };
  auto f32 = [](const uint8_t* p) { float v; memcpy(&v, p, 4); return v; };
  const uint8_t* h = rst.hdr;
  if (u32(h) != CXS_MAGIC) return restoreFail(400, "Keine Scan-Datei (.cxs)");
  rst.nx = u32(h + 4); rst.ny = u32(h + 8); rst.res = f32(h + 12); rst.ox = f32(h + 16); rst.oy = f32(h + 20);
  rst.total = u32(h + 24); rst.e0 = u32(h + 28); rst.e1 = u32(h + 32); rst.flags = u32(h + 36); rst.cLen = u32(h + 40);
  if (!rst.nx || !rst.ny || !(rst.res > 0) || isnan(rst.ox) || isnan(rst.oy) || rst.cLen > 4000000UL)
    return restoreFail(400, "Scan-Datei beschaedigt");
  if ((uint64_t)rst.nx * rst.ny > MAX_POINTS) return restoreFail(400, "Zu viele Punkte");
  rst.dataLen = (uint64_t)rst.nx * rst.ny * sizeof(int16_t);
  const uint64_t need = rst.dataLen + rst.cLen + 4096;
  const uint64_t have = fsUsable(fileSize(DATA_FILE) + fileSize(CONT_FILE));
  if (need > have) {
    snprintf(rst.msg, sizeof rst.msg, "Nicht genug Flash: %u kB noetig, %u kB frei", (unsigned)(need >> 10), (unsigned)(have >> 10));
    rst.fail = true; rst.code = 507; return;
  }
  // alten Scan verwerfen, neue Rohdaten schreiben
  gridNx = gridNy = 0; rowsWritten = 0; pointsDone = 0; totalPts = 0; maskOn = false; state = ST_IDLE;
  LittleFS.remove(DATA_FILE); LittleFS.remove(META_FILE); LittleFS.remove(CONT_FILE);
  rst.f = LittleFS.open(DATA_FILE, FILE_WRITE);
  if (!rst.f) restoreFail(500, "Datei-Schreibfehler");
}

static void handleRestoreUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (rst.f) rst.f.close();
    rst.hn = 0; rst.got = 0; rst.dataLen = 0; rst.cLen = 0; rst.fail = false; rst.code = 200; rst.msg[0] = 0;
    if (taskRunning() || Motion::busy()) restoreFail(409, "Beschaeftigt");
    Serial.printf("Scan-Datei: %s\n", up.filename.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    const uint8_t* p = up.buf; size_t n = up.currentSize;
    while (n && !rst.fail) {
      if (rst.hn < sizeof rst.hdr) {                       // Kopf sammeln
        const size_t k = min(n, (size_t)(sizeof rst.hdr - rst.hn));
        memcpy(rst.hdr + rst.hn, p, k); rst.hn += k; p += k; n -= k;
        if (rst.hn == sizeof rst.hdr) restoreHeader();
        continue;
      }
      const uint64_t total = rst.dataLen + rst.cLen;
      if (rst.got >= total) { restoreFail(400, "Scan-Datei zu lang"); break; }
      if (rst.got == rst.dataLen) {                        // Wechsel Rohdaten -> Konturen
        rst.f.close();
        rst.f = LittleFS.open(CONT_FILE, FILE_WRITE);
        if (!rst.f) { restoreFail(500, "Datei-Schreibfehler"); break; }
      }
      const uint64_t lim = rst.got < rst.dataLen ? rst.dataLen : total;
      const size_t k = (size_t)min((uint64_t)n, lim - rst.got);
      if (rst.f.write(p, k) != k) { restoreFail(507, "Flash voll"); break; }
      rst.got += k; p += k; n -= k;
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (rst.f) rst.f.close();
    if (!rst.fail && (rst.hn < sizeof rst.hdr || rst.got != rst.dataLen + rst.cLen)) restoreFail(400, "Scan-Datei unvollstaendig");
    if (!rst.fail) {
      gridNx = rst.nx; gridRes = rst.res; gridOx = rst.ox; gridOy = rst.oy;
      totalPts = rst.total ? rst.total : rst.nx * rst.ny;
      scanE0 = rst.e0; scanE1 = rst.e1; scanT0 = scanT1 = 0; pausedMs = 0;
      writeMeta(rst.ny, true, rst.flags & 1);
      loadMeta();
      Serial.printf("Scan-Datei geladen: %u x %u, %u Bytes Konturen\n", (unsigned)rst.nx, (unsigned)rst.ny, (unsigned)rst.cLen);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    restoreFail(400, "Uebertragung abgebrochen");
  }
  if (rst.fail && rst.hn >= sizeof rst.hdr && rst.code != 409 && !gridNx) {   // halbe Dateien nicht stehen lassen
    LittleFS.remove(DATA_FILE); LittleFS.remove(META_FILE); LittleFS.remove(CONT_FILE);
  }
}

static void handleRestoreDone() {
  if (rst.fail) return err(rst.code, rst.msg);
  ok();
}

// Konturen: GET liefert gespeicherte Konturen (404 = keine), POST (Datei-Upload) speichert sie
static void handleCLoopsGet() {
  File f = LittleFS.open(CONT_FILE, FILE_READ);
  if (!f) return err(404, "Keine Konturen");
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(f, "application/json");
  f.close();
}

static File  cUpF;
static bool  cUpFail = false;
static void handleCLoopsUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    cUpFail = scanRunning() || Motion::busy();             // Flash-Zugriff nur bei stehenden Achsen
    if (!cUpFail) { cUpF = LittleFS.open("/contour.tmp", FILE_WRITE); cUpFail = !cUpF; }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!cUpFail && cUpF.write(up.buf, up.currentSize) != up.currentSize) cUpFail = true;
  } else if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (cUpF) cUpF.close();
    if (up.status == UPLOAD_FILE_ABORTED) cUpFail = true;
    if (!cUpFail) { LittleFS.remove(CONT_FILE); cUpFail = !LittleFS.rename("/contour.tmp", CONT_FILE); }
    else LittleFS.remove("/contour.tmp");
  }
}
static void handleCLoopsDone() { if (cUpFail) return err(409, "Konturen nicht gespeichert (beschaeftigt?)"); ok(); }

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
  server.on("/api/scanmask",HTTP_POST, handleScanMask);
  server.on("/api/stop",    HTTP_POST, handleStop);
  server.on("/api/pause",   HTTP_POST, handlePause);
  server.on("/api/jog",     HTTP_POST, handleJog);
  server.on("/api/jogc",    HTTP_POST, handleJogC);
  server.on("/api/jogka",   HTTP_POST, handleJogKA);
  server.on("/api/jogstop", HTTP_POST, handleJogStop);
  server.on("/api/frame",   HTTP_POST, handleFrame);
  server.on("/api/contour", HTTP_POST, handleContour);
  server.on("/api/cdata",   HTTP_GET,  handleCData);
  server.on("/api/format",  HTTP_POST, handleFormat);
  server.on("/api/restore", HTTP_POST, handleRestoreDone, handleRestoreUpload);
  server.on("/api/cloops",  HTTP_GET,  handleCLoopsGet);
  server.on("/api/cloops",  HTTP_POST, handleCLoopsDone, handleCLoopsUpload);
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

  // Dauerfahrt: ohne Lebenszeichen vom Browser abbremsen
  if (jogCont) {
    if (!Motion::busy()) jogCont = false;
    else if (millis() - jogKA > JOG_KEEPALIVE_MS) { Motion::stop(); jogCont = false; Serial.println("Dauerfahrt: kein Lebenszeichen -> Stopp"); }
  }

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
