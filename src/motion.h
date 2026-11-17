#pragma once
#include <Arduino.h>

// CoreXY-Bewegung über Timer-ISR (Trapezprofil, Bresenham für A/B)
namespace Motion {
  void  begin();
  void  enable(bool on);
  bool  isEnabled();
  bool  busy();
  void  stop();                                   // gebremster Halt
  bool  moveTo(float xmm, float ymm, float speed); // nicht blockierend, Koordinaten relativ zum Nullpunkt
  bool  moveRel(float dx, float dy, float speed);
  void  waitDone();                               // blockiert nur den aufrufenden Task
  bool  setZero();                                // aktuelle Position = 0/0
  void  inhibit(bool on);                         // sperrt alle neuen Bewegungen (z.B. OTA)
  float x();
  float y();
}
