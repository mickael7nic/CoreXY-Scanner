#pragma once
#include <Arduino.h>

namespace Sensor {
  bool  begin();                                         // false = ADS1115 nicht gefunden
  bool  isAds();

  // Abstand in mm (NaN = ungültig). raw = mA bzw. V am Sensorausgang
  float readMm(uint16_t samples, float* raw = nullptr);         // kalibriert
  float readNominalMm(uint16_t samples, float* raw = nullptr);  // ohne Kalibrierung

  // Fliegende Messung (nur ADS): Werte zwischen zwei Rasterpunkten aufsummieren
  void     accReset();
  void     accPoll();                                   // nimmt neuen Wandlerwert auf, falls verfügbar
  uint16_t accCount();
  float    accTakeMm(float* raw = nullptr);             // Mittelwert (kalibriert) + Reset

  // Zwei-Punkt-Kalibrierung: d = d_nom * scale + offset  (im NVS gespeichert)
  void  setCal(float scale, float offset);
  float calScale();
  float calOffset();
}
