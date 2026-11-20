#pragma once
#include <Arduino.h>

// ============================================================
//  Board-Auswahl: automatisch über das in der Arduino IDE gewählte Board
//    "ESP32S3 Dev Module"  -> ESP32-S3-Pins
//    "ESP32 Dev Module"    -> klassische ESP32-Pins
//  Erzwingen: eine der beiden Zeilen aktivieren
// ============================================================
// #define BOARD_ESP32S3
// #define BOARD_ESP32
#if !defined(BOARD_ESP32S3) && !defined(BOARD_ESP32)
  #if CONFIG_IDF_TARGET_ESP32S3
    #define BOARD_ESP32S3
  #else
    #define BOARD_ESP32
  #endif
#endif

// STEP-Pins müssen < GPIO32 sein (schneller Registerzugriff in der ISR)
#ifdef BOARD_ESP32S3
  // ---------- ESP32-S3 ----------
  // Tabu: 19/20 (USB), 26–32 (Flash), 33–37 (PSRAM bei Octal-Modulen N8R8/N16R8),
  //       Strapping 0, 3, 45, 46 meiden. Analog nur ADC1 = GPIO 1–10 (ADC2 stört WLAN).
  #define PIN_DIR_A    10
  #define PIN_STEP_A   11
  #define PIN_EN_A     12
  #define PIN_DIR_B    16
  #define PIN_STEP_B   17
  #define PIN_EN_B     18
  #define PIN_SENSOR    4     // ADC1_CH3 (nur bei SENSOR_ADC_ADS1115 0 benutzt)
  #define PIN_I2C_SDA  13     // ADS1115 (nur bei SENSOR_ADC_ADS1115 1)
  #define PIN_I2C_SCL  14
#else
  // ---------- ESP32 Dev Module (klassisch) ----------
  // Tabu: GPIO 6–11 (Flash), 1/3 (UART), Strapping 0/2/5/12/15 meiden.
  #define PIN_STEP_A   16     // Grün   GPIO16/17 nur bei WROOM-Modulen frei (bei WROVER = PSRAM)
  #define PIN_DIR_A     4     // Gelb
  #define PIN_EN_A     17     // Blau
  #define PIN_STEP_B   19     // Grün
  #define PIN_DIR_B    18     // Gelb
  #define PIN_EN_B     21     // Blau
  #define PIN_SENSOR   34     // Grau   ADC1_CH6, nur Eingang (nur bei SENSOR_ADC_ADS1115 0 benutzt)
  #define PIN_I2C_SDA  22     // ADS1115 (nur bei SENSOR_ADC_ADS1115 1)
  #define PIN_I2C_SCL  23
#endif

// ============================================================
//  Uhrzeit (NTP)
// ============================================================
#define NTP_SERVER1  "fritz.box"
#define NTP_SERVER2  "pool.ntp.org"
#define TZ_INFO      "CET-1CEST,M3.5.0,M10.5.0/3"   // Mitteleuropa inkl. Sommerzeit

// ============================================================
//  Mechanik
// ============================================================
#define MOTOR_FULLSTEPS  200     // 1.8° Motor
#define MICROSTEPS       16      // = DIP-Einstellung am Treiber (1..64)
#define BELT_PITCH_MM    2.0f    // GT2
#define PULLEY_TEETH     20
#define STEPS_PER_MM     ((float)MOTOR_FULLSTEPS * MICROSTEPS / (BELT_PITCH_MM * PULLEY_TEETH))

// CoreXY: A = X + Y, B = X - Y
// Jog X+ fährt in X-  -> beide invertieren
// Jog X+ fährt entlang Y -> genau einen Motor invertieren
#define DIR_INVERT_A   false
#define DIR_INVERT_B   false

// Enable-Pegel für "Motor bestromt". Viele Closed-Loop-Treiber (z.B. CL57T)
// schalten bei bestromtem ENA-Optokoppler AB -> dann LOW = aktiv (bei ENA+ an Pin, ENA- an GND)
#define EN_LEVEL_ACTIVE   LOW
#define ENABLE_DELAY_MS   200     // Treiber-Bereitschaft nach Enable
#define DIR_SETUP_US      20

// ============================================================
//  Bewegung
// ============================================================
#define TICK_HZ               40000   // Step-ISR-Takt -> max. 20 kHz Stepfrequenz pro Motor
#define ACCEL_MM_S2           300.0f
#define START_SPEED_MM_S      2.0f
#define DEFAULT_SCAN_SPEED    20.0f
#define DEFAULT_TRAVEL_SPEED  40.0f
#define IDLE_DISABLE_MS       60000UL // Motoren aus nach 1 min ohne Bewegung
#define JOG_MAX_MM            500.0f  // Dauerfahrt (Taste gedrückt halten): max. Weg pro Druck
#define JOG_KEEPALIVE_MS      600     // Dauerfahrt stoppt, wenn so lange kein Lebenszeichen vom Browser kommt
#define PREAPPROACH_MM        1.0f    // nur unidirektional: Vorlauf in -X für gleiche Anfahrrichtung

// ============================================================
//  Sensor HG-C1100 (Messbereich 100 ±35 mm)
// ============================================================
// Wandler: 1 = ADS1115 (16 Bit, I2C), 0 = interner ESP32-ADC
#define SENSOR_ADC_ADS1115    0
// 1 = 4–20 mA über Shunt (empfohlen), 0 = 0–5 V über Spannungsteiler
#define SENSOR_MODE_CURRENT   1

#if SENSOR_ADC_ADS1115
  #define ADS_ADDR            0x48    // ADDR an GND
  #define ADS_PGA             2       // 0=±6.144 1=±4.096 2=±2.048 3=±1.024 V
  #define ADS_DIFFERENTIAL    1       // 1 = AIN0-AIN1 (AIN1 am Shunt-Fußpunkt), 0 = AIN0-GND
  #define SHUNT_OHM           100.0f  // 4..20 mA -> 0.4..2.0 V (0.1 %, <=25 ppm/K)
  #define VDIV_RATIO          ((15.0f + 10.0f) / 10.0f) // R_oben 15k, R_unten 10k: 5 V -> 2.0 V
#elif defined(BOARD_ESP32S3)
  // ESP32-S3-ADC bei 12 dB linear bis ~2.9 V. 100 Ω 0,1 % wie beim ADS1115 -> später kein Umbau
  #define SHUNT_OHM           100.0f  // 4..20 mA -> 0.4..2.0 V (0.1 % Widerstand)
  #define VDIV_RATIO          ((22.0f + 24.0f) / 24.0f) // R_oben 22k, R_unten 24k: 5 V -> 2.6 V
#else
  // Klassischer ESP32: ADC bei 11 dB nur bis ~2.45 V linear -> Bereich auf max. 2.4 V legen
  #define SHUNT_OHM           120.0f  // 4..20 mA -> 0.48..2.4 V (0.1 % Widerstand!)
  #define VDIV_RATIO          ((22.0f + 20.0f) / 20.0f) // R_oben 22k, R_unten 20k: 5 V -> 2.38 V
#endif
// Abstand bei 4 mA / 0 V und bei 20 mA / 5 V  -> mit Live-Anzeige prüfen, ggf. tauschen
#define DIST_AT_MIN_MM        135.0f   // getauscht: Messung war invertiert
#define DIST_AT_MAX_MM        65.0f
#define SENSOR_VALID_MARGIN   0.01f   // Werte >1 % außerhalb Skala = ungültig (kein Objekt / Alarm)

#define DEFAULT_AVG           32      // ADS1115: 8–16 reichen (1.2 ms pro Wert)
#define DEFAULT_SETTLE_MS     60
#define MAX_POINTS            2000000UL // zusätzlich begrenzt durch freien Flash (LittleFS, ca. 700k Punkte)

// Kontur abfahren: max. Kantenpunkte, Größe Mess-Cache (Zweierpotenz), Schrittgrenze
#ifdef BOARD_ESP32S3
  #define CONTOUR_MAX_PTS     40000UL   // PSRAM wird genutzt, falls aktiviert
  #define CONTOUR_HASH        65536UL
#else
  #define CONTOUR_MAX_PTS     6000UL    // ca. 48 KB
  #define CONTOUR_HASH        8192UL    // ca. 40 KB
#endif
#define CONTOUR_MAX_STEPS     400000UL
