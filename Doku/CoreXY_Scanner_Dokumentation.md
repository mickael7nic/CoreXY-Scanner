---
title: "CoreXY Laser-Oberflächenscanner"
subtitle: "Dokumentation – Hardware, Firmware, Bedienung"
date: "Stand: 16.11.2026"
lang: de
---

# Überblick

Der Scanner tastet eine Oberfläche mit einem Laser-Abstandssensor **Panasonic HG-C1100** rasterförmig ab und erzeugt daraus ein 3D-Modell. Der Sensor sitzt auf einem **CoreXY-Portal** mit zwei Closed-Loop-Schrittmotoren (GT2-Riemen, 20-Zahn-Pulleys). Gesteuert wird alles von einem **ESP32 Dev Module**, bedient über eine Weboberfläche im Browser.

**Funktionen**

- Scanbereich X × Y ab der aktuellen Position, Rasterweite frei wählbar
- zwei Messarten: *Anhalten an jedem Punkt* (höchste Genauigkeit) oder *fliegende Messung* (schnell)
- Scan pausieren und fortsetzen
- „Rahmen abfahren“ zur Kontrolle, ob das Teil vollständig im Scanbereich liegt
- Live-Vorschau als Höhenbild, Fortschritt mit Start-/Endzeit (NTP) und Restzeit
- Scandaten im Flash – bleiben bei Neustart, Stromausfall und Update erhalten; abgebrochene Scans werden als Teilscan übernommen
- Export als **CSV**, **3MF**, **STL** und **STEP**; die Auflagefläche (Z = 0) wird automatisch erkannt und weggelassen
- 16-Bit-Wandler ADS1115 mit Zwei-Punkt-Höhenkalibrierung
- Firmware-Update (OTA) über die Weboberfläche
- automatische Motor-Abschaltung nach 1 min ohne Bewegung

# Hardware

## Komponenten

| Komponente | Typ / Wert |
|---|---|
| Controller | ESP32 Dev Module (ESP32-D0WD-V3, WROOM-Modul, 4 MB Flash) |
| Motoren | 2 × Closed-Loop-Schrittmotor, 200 Vollschritte/U, Mikroschritt 1–64 (eingestellt: 16) |
| Antrieb | GT2-Riemen (2 mm), Pulley 20 Zähne → 40 mm/U → **80 Schritte/mm** bei 16 Mikroschritten |
| Sensor | Panasonic HG-C1100, Messbereich 100 ± 35 mm, Analogausgang 4–20 mA bzw. 0–5 V |
| A/D-Wandler | ADS1115, 16 Bit, I²C (alternativ interner ESP32-ADC) |
| Shunt | 100 Ω, 0,1 %, ≤ 25 ppm/K (bei ESP32-ADC: 120 Ω) |

## Pinbelegung ESP32

| Signal | GPIO | Aderfarbe |
|---|---|---|
| Motor A – STEP | 16 | Grün |
| Motor A – DIR | 4 | Gelb |
| Motor A – EN | 17 | Blau |
| Motor B – STEP | 19 | Grün |
| Motor B – DIR | 18 | Gelb |
| Motor B – EN | 21 | Blau |
| ADS1115 – SDA | 22 | |
| ADS1115 – SCL | 23 | |
| Sensor analog (nur ohne ADS1115) | 34 | Grau |

Hinweise:

- GPIO 6–11 sind am klassischen ESP32 mit dem Flash verbunden und dürfen nicht verwendet werden (führt zu Watchdog-Reset).
- GPIO 16/17 sind nur bei WROOM-Modulen frei; bei WROVER-Modulen belegt sie der PSRAM.
- STEP-Pins müssen unter GPIO 32 liegen (schneller Registerzugriff in der Step-Routine).
- Closed-Loop-Treiber erwarten oft 5-V-Signale. Schalten die Eingänge mit 3,3 V nicht zuverlässig, einen Pegelwandler (z. B. 74AHCT125 an 5 V) dazwischen setzen.
- Der Enable-Eingang vieler Treiber schaltet den Motor bei aktivem Signal **ab**. Der Pegel ist in `config.h` über `EN_LEVEL_ACTIVE` einstellbar.

## Sensor-Anschluss mit ADS1115 (empfohlen)

```
                       +12…24 V ── HG-C1100 (+V)
                                  HG-C1100 (0 V) ───────────────┬── GND Netzteil
                                                                │
HG-C1100 Analogausgang (4–20 mA) ──┬──[ 1 kΩ ]──┬── AIN0        │
                                   │            │   ADS1115     │
                              R_shunt 100 Ω   100 nF            │
                              0,1 %, ≤25 ppm    │               │
                                   │            │               │
GND ───────────────────────────────┴────────────┴── AIN1 / GND ─┴── GND ESP32

ADS1115:  VDD → 3,3 V (ESP32)    GND  → GND
          SDA → GPIO 22          SCL  → GPIO 23
          ADDR → GND (Adresse 0x48)
```

| Teil | Wert | Zweck |
|---|---|---|
| Shunt | 100 Ω, 0,1 % | 4–20 mA → 0,4–2,0 V, passt in den ±2,048-V-Bereich |
| Messbereich ADS | ±2,048 V | 62,5 µV pro Stufe → ca. 0,003 mm Auflösung |
| RC-Filter | 1 kΩ + 100 nF | Störspitzen unterdrücken, Grenzfrequenz ca. 1,6 kHz |
| Messung | differenziell AIN0 – AIN1 | Masseversatz fällt heraus; AIN1 direkt am Shunt-Fußpunkt |

**Wichtig:** Den ADS1115 nur mit **3,3 V** versorgen. Übliche Platinen ziehen SDA/SCL auf ihre Versorgungsspannung; mit 5 V lägen 5 V an den ESP32-Pins. Sensor-0V, Netzteil-GND und ESP32-GND müssen verbunden sein.

**Variante 0–5 V:** Spannungsteiler 15 kΩ (oben) / 10 kΩ (unten) → 2,0 V bei 5 V, dazu dasselbe RC-Filter. Störanfälliger als der Stromausgang.

## Sensor-Anschluss ohne ADS1115

Der Analogausgang wird über einen **120-Ω-Shunt** (0,48–2,4 V) direkt an GPIO 34 geführt. Der interne ADC des klassischen ESP32 ist nur bis ca. 2,45 V linear und löst über den vollen Messbereich nur ca. 0,02–0,03 mm pro Stufe auf. In `config.h` dafür `SENSOR_ADC_ADS1115` auf `0` setzen.

# Software

## Dateien

| Datei | Inhalt |
|---|---|
| `CoreXY_Scanner.ino` | Hauptprogramm: WLAN, Webserver, Scanablauf, Speicher, OTA, NTP |
| `config.h` | Pins, Mechanik, Bewegung, Sensor, Wandler |
| `secrets.h` | WLAN-Zugangsdaten, Netzwerk |
| `motion.h / .cpp` | CoreXY-Bewegung per Timer-Interrupt |
| `sensor.h / .cpp` | Sensor-Auswertung (ADS1115 oder ESP32-ADC), Kalibrierung |
| `webpage.h` | Weboberfläche inkl. Export (CSV/3MF/STL/STEP) |
| `partitions.csv` | optional, eigene Partitionstabelle (siehe unten) |

Alle Dateien liegen im Sketch-Ordner `CoreXY_Scanner`. Externe Bibliotheken werden nicht benötigt.

## Arduino-IDE-Einstellungen

| Einstellung | Wert |
|---|---|
| Board-Paket | Arduino-ESP32 (Espressif) **ab Version 3.0** |
| Board | ESP32 Dev Module |
| Flash Size | 4MB |
| Partition Scheme | **Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)** |

Mit dem Default-Schema stehen ca. 1,3 MB für Scandaten zur Verfügung (ca. 640 000 Punkte). Passt der Sketch nicht in 1,25 MB, `partitions.csv` in den Sketch-Ordner legen: 2 × 1,44 MB Programm (OTA weiter möglich) und 1 MB Datenspeicher (ca. 480 000 Punkte). Die IDE verwendet die Datei dann automatisch.

**Nach einer Änderung des Partitionsschemas muss einmal per USB geflasht werden** – ein OTA-Update überträgt die Partitionstabelle nicht.

## secrets.h

| Eintrag | Bedeutung |
|---|---|
| `WIFI_SSID`, `WIFI_PASS` | WLAN-Zugangsdaten (nur 2,4 GHz) |
| `USE_STATIC_IP` | `0` = DHCP mit Reservierung im Router (empfohlen), `1` = feste IP im ESP32 |
| `LOCAL_IP`, `GATEWAY_IP`, `SUBNET_MASK`, `DNS_IP` | nur bei fester IP, mit Kommas statt Punkten |
| `HOSTNAME` | Gerätename im Netzwerk |

## config.h – wichtige Parameter

| Parameter | Standard | Bedeutung |
|---|---|---|
| `MICROSTEPS` | 16 | muss zur DIP-Einstellung der Treiber passen |
| `DIR_INVERT_A/B` | false | Drehrichtung der Motoren (siehe Inbetriebnahme) |
| `EN_LEVEL_ACTIVE` | LOW | Pegel für „Motor bestromt“ |
| `ACCEL_MM_S2` | 300 | Beschleunigung; 1000–2000 meist möglich, bestimmt die Scanzeit bei kleinem Raster |
| `IDLE_DISABLE_MS` | 60000 | Motoren aus nach dieser Zeit ohne Bewegung |
| `TICK_HZ` | 40000 | Takt der Step-Routine, max. Stepfrequenz = halber Takt |
| `SENSOR_ADC_ADS1115` | 1 | 1 = ADS1115, 0 = interner ADC |
| `SENSOR_MODE_CURRENT` | 1 | 1 = 4–20 mA, 0 = 0–5 V |
| `SHUNT_OHM` | 100 | Shunt-Widerstand |
| `DIST_AT_MIN_MM` / `DIST_AT_MAX_MM` | 135 / 65 | Abstand bei 4 mA bzw. 20 mA (bei Skalierung des Sensorausgangs anpassen) |
| `NTP_SERVER1/2`, `TZ_INFO` | fritz.box, pool.ntp.org, MEZ/MESZ | Zeitquelle und Zeitzone |

# Inbetriebnahme

1. **Netzwerk:** Nach dem Flashen zeigt der serielle Monitor (115200 Baud) die **MAC-Adresse**. Damit im Router eine feste IP reservieren. Bei Verbindungsproblemen gibt der Monitor den Trennungsgrund aus (z. B. Grund 15/204 = falsches Passwort, 201 = SSID nicht gefunden).
2. **Weboberfläche** im Browser öffnen: `http://<IP-Adresse>/`.
3. **Fahrrichtungen prüfen** mit den Jog-Tasten:

| Fehlerbild | Abhilfe in `config.h` |
|---|---|
| X und Y vertauscht | einen der beiden `DIR_INVERT` umschalten |
| beide Achsen falsch herum | beide `DIR_INVERT` umschalten |
| nur Y falsch herum | Pins von Motor A und B tauschen |

4. **Sensorrichtung prüfen:** Ein Objekt näher an den Sensor halten – der angezeigte Abstand muss kleiner werden. Sonst `DIST_AT_MIN_MM` und `DIST_AT_MAX_MM` tauschen.
5. **ADS1115 prüfen:** Unter „Wandler / Kal.“ muss *ADS1115* stehen. Wird er nicht gefunden, meldet der serielle Monitor `ADS1115 nicht gefunden`.
6. **Höhen-Kalibrierung** durchführen (siehe unten).
7. **Rauschen ermitteln:** Kopf über die Auflage stellen und den Live-Abstand ca. 30 s beobachten. Die Schwankung ist das Rauschen; danach richten sich die Toleranzen im Export.

## Höhen-Kalibrierung (2 Punkte)

Im Bereich *Manuell verfahren*:

1. Kopf über eine freie Stelle der Auflage fahren → **„1: Auflage messen“**.
2. Endmaß bekannter Höhe (ideal 20–50 mm) darunterlegen, Höhe eintragen → **„2: Endmaß messen“**.
3. Gemessene und Soll-Höhe sowie der neue Faktor werden angezeigt → **„Übernehmen“**.

Die Werte werden dauerhaft im ESP32 gespeichert (auch über OTA). „Zurücksetzen“ stellt Faktor 1 / Offset 0 wieder her. Unplausible Werte (Faktor außerhalb 0,8–1,25) werden abgelehnt.

# Bedienung

## Aufbau der Weboberfläche

| Bereich | Inhalt |
|---|---|
| Kopfleiste (immer sichtbar) | Zustand, X/Y/Z-Position, **PAUSE/WEITER**, **STOP** |
| Zeile 1 | Status · Scan-Einstellungen · Export-Einstellungen |
| Zeile 2 | Manuell verfahren (inkl. Kalibrierung) · Firmware-Update |
| Zeile 3 | Vorschau |

Die Einstellungsbereiche lassen sich per Klick auf die Überschrift einklappen; der Zustand wird im Browser gespeichert. Unter 900 px Breite werden alle Bereiche untereinander angezeigt.

Z in der Kopfleiste ist die Live-Höhe über der Auflage. Sie richtet sich nach der Z-Null-Einstellung im Export; „Auto“ und „Startpunkt“ benötigen einen vorhandenen Scan.

## Ablauf eines Scans

1. Kopf mit den Jog-Tasten auf die **linke vordere Ecke** des Scanbereichs fahren.
2. Bereich X/Y und Auflösung eintragen.
3. **„Rahmen abfahren“** – der Kopf fährt den Bereich einmal als Rechteck ab und endet wieder am Startpunkt. Am Lichtpunkt des Sensors ist zu erkennen, ob das Teil vollständig im Rahmen liegt.
4. **„Scan starten“** – die aktuelle Position wird zum Nullpunkt. Nach dem Scan fährt der Kopf zum Startpunkt zurück.
5. Nach Abschluss in den Export-Einstellungen die gewünschten Dateien erzeugen.

**PAUSE** hält den Scan an einer sicheren Stelle an: beim Anhalten-Modus nach dem aktuellen Punkt, bei fliegender Messung am Ende der aktuellen Zeile. Die Motoren bleiben bestromt, die Position bleibt erhalten. **WEITER** setzt den Scan fort. Pausenzeiten werden bei Laufzeit und Restzeit nicht mitgezählt; Jog-Fahrten sind während der Pause gesperrt.

**STOP** bricht den Scan ab (auch aus der Pause). Bereits gescannte Zeilen werden als fertiger Teilscan übernommen („DONE (abgebrochen)“) und können exportiert werden.

## Scan-Einstellungen

| Feld | Bedeutung |
|---|---|
| Bereich X / Y | Größe des Scanbereichs ab aktueller Position in mm |
| Auflösung XY | Rasterweite in mm |
| Scan-Geschw. | Geschwindigkeit zwischen den Punkten bzw. Zeilengeschwindigkeit (fliegend) |
| Eilgang | Geschwindigkeit für Positionierfahrten |
| Beruhigung | Wartezeit nach dem Anfahren eines Punkts (nur Anhalten) |
| Mittelung | Anzahl Messwerte pro Punkt (nur Anhalten; ADS1115: 8–16, je ca. 1,2 ms) |
| Serpentine | Zeilen abwechselnd hin und zurück (schneller); aus = jede Zeile aus derselben Richtung (genauer) |
| Fliegende Messung | Zeile ohne Anhalten durchfahren, Messung beim Überfahren jedes Rasterpunkts |
| Sensor-Verzögerung | Ausgleich der Sensor-Laufzeit bei fliegender Messung (Standard 2 ms) |

Unter den Feldern wird die Rastergröße und die geschätzte Dauer angezeigt, bei fliegender Messung ggf. eine Warnung, wenn zu wenig Zeit pro Punkt bleibt.

### Messarten

**Anhalten an jedem Punkt:** Punkt anfahren, Beruhigungszeit abwarten, gemittelt messen. Höchste Genauigkeit. Bei 1 mm Raster begrenzt die Beschleunigung die Geschwindigkeit (bei 300 mm/s² werden auf 1 mm nur ca. 17 mm/s erreicht).

**Fliegende Messung:** Jede Zeile wird mit konstanter Geschwindigkeit durchfahren. Gemessen wird an der Ist-Position der Motoren. Mit ADS1115 wird über alle Wandlerwerte zwischen zwei Rasterpunkten gemittelt; das Mittelungsfenster ist automatisch auf den Punkt zentriert.

- Vor und hinter jeder Zeile fährt der Kopf ein Stück über den Scanbereich hinaus, um auf Geschwindigkeit zu kommen (bei 50 mm/s und 300 mm/s² ca. 5 mm). **Dieser Weg muss mechanisch frei sein.**
- **Sensor-Verzögerung einstellen:** Teil mit scharfer Kante quer zur X-Richtung mit Serpentine scannen. Ist die Kante gezackt, den Wert ändern, bis sie gerade ist.

**Beispiel 100 × 100 mm, 1 mm Raster (10 201 Punkte):**

| Einstellung | Zeit pro Punkt | Dauer |
|---|---|---|
| Anhalten, 300 mm/s², Beruhigung 60 ms | ca. 0,18 s | ca. 31 min |
| Anhalten, 1500 mm/s², Beruhigung 20 ms | ca. 0,075 s | ca. 13 min |
| fliegend, 50 mm/s | ca. 0,02 s | ca. 9 min |

## Status und Fortschritt

Der Statusbereich zeigt Zustand, Position, Abstand, Sensor-Rohwert (mA bzw. V), Motorzustand, Punkte, Wandler/Kalibrierung, Scan-Speicher (Größe und max. Punktzahl), Uhrzeit des ESP32 und Firmware-Stand.

Unter dem Fortschrittsbalken: Fortschritt in Prozent (groß, rot), **Start**, **Fertig ca. / Fertig um**, **Gesamtdauer**, **Geschafft** und **Restzeit**. Die Prognose wird aus der bisherigen Geschwindigkeit hochgerechnet. Start- und Endzeit kommen per NTP vom ESP32 und bleiben mit dem Scan gespeichert.

## Vorschau

Höhenbild des Scans (blau = tief, rot = hoch, grau = leer), während des Scans laufend aktualisiert. Es werden nur neu fertige Zeilen nachgeladen. Mit „Nur Teil anzeigen“ werden Z-Null, Mindesthöhe und Glättung aus den Export-Einstellungen angewendet.

**Spiegeln:** „↔ Horizontal spiegeln“ tauscht links/rechts (X), „↕ Vertikal spiegeln“ oben/unten (Y). Die Spiegelung wirkt auf Vorschau und alle Exporte; die gespeicherten Messdaten bleiben unverändert. Ein erneuter Klick hebt sie auf, ein neuer Scan setzt sie zurück.

# Export

## Einstellungen

| Feld | Bedeutung | Empfehlung |
|---|---|---|
| Z-Null (Auflagefläche) | *Auto*: Ebene durch die Auflage fitten (gleicht Schräglage aus) · *Startpunkt*: Messwert am Startpunkt · *Manuell*: fester Abstand | Auto |
| Ebenen-Toleranz | Abweichung, bis zu der Punkte zur Auflage zählen | ca. 3 × Rauschen |
| Mindesthöhe Teil | Punkte darunter gelten als Auflage und werden weggelassen | ca. 5 × Rauschen |
| **Fehlstellen** | | |
| Kleine Löcher schließen | Bereiche ohne gültigen Messwert (oder unter Mindesthöhe) innerhalb des Teils werden glatt aus der Umgebung interpoliert | an |
| max. Lochfläche | größere Bereiche gelten als echte Löcher und bleiben offen; Bereiche am Scanrand werden nie gefüllt | knapp unter dem kleinsten echten Loch, z. B. 2–5 mm² |
| Ausreißer entfernen ab | Einzelpunkte, die von allen Nachbarn um mehr als diesen Wert abweichen, werden verworfen und gefüllt; Kanten bleiben erhalten | 0,5–1 mm (0 = aus) |
| **Oberfläche glätten** | | |
| Modus | *Gauß, kantenerhaltend*: gewichtetes Glätten, Nachbarn mit großem Höhenunterschied zählen nicht · *Mittelwert*: einfacher Mittelwert · *Löschen*: nur CSV, entfernt Punkte innerhalb der Toleranz | Gauß |
| Z-Toleranz | 0 = aus; Höhenunterschiede über ca. 3 × Toleranz gelten als Kante und werden nicht geglättet | ca. 3 × Rauschen |
| Gauß-Radius σ | Reichweite der Glättung in mm; größer = glatter, feine Details gehen verloren | 0,3–1 mm |
| Gauß-Durchläufe | Mehrfachanwendung für stärkere Wirkung | 1–3 |
| Mittelwert-Radius | nur Modus Mittelwert: Nachbarschaft in Punkten (1 = 3×3) | 1 |
| Ebene Flächen begradigen | erkennt ebene Bereiche und setzt sie exakt auf eine Ebene; gekrümmte Flächen bleiben unverändert | bei Teilen mit ebenen Flächen |
| Ebenen-Toleranz | max. Abweichung eines Punkts von der Ebene | 0,03–0,1 mm |
| Ebene min. Fläche | kleinere ebene Bereiche werden nicht begradigt | 10–50 mm² |
| **Umriss** | | |
| Umriss-Glättung σ | Glättung der Außen- und Lochkonturen in mm; 0 = ungeglättet | 0,3–1 mm |
| Konturen kleiner als | kleinere Konturen (Störungen) werden ignoriert | 1–5 mm² |
| Umriss in Vorschau | zeigt Außenkonturen weiß, Löcher türkis über dem Höhenbild | |
| Ausdünnung | nur jeder k-te Punkt für 3MF/STL/STEP | 1 (STEP: 2–4) |
| Dateiname | ohne Endung | |

## Formate

| Format | Inhalt | Verwendung |
|---|---|---|
| **CSV** | `x_mm,y_mm,z_mm`, nur Punkte des Teils | Punktwolke, CloudCompare, Python, Excel |
| **3MF** | geschlossener Körper (Oberfläche, Wände, Boden auf Z = 0) | Slicer, Windows 3D-Builder |
| **STL** | wie 3MF, binär | MeshLab, Blender, Slicer |
| **STEP** | Volumenkörper aus ebenen Flächen (AP214) | CAD (FreeCAD, Fusion 360, SolidWorks …) |
| **Umriss DXF** | geschlossene Polylinien, Ebenen `AUSSEN` und `LOCH`, Einheit mm | CAD-Skizze, Extrusion, Laser/CNC |
| **Umriss SVG** | Konturen als Pfad (Löcher ausgespart), Maße in mm | Inkscape, Laser, Plotter |

**Umriss:** Die Kontur wird dort gelegt, wo die Oberfläche die Mindesthöhe kreuzt, zwischen zwei Rasterpunkten interpoliert (Marching Squares). Danach wird sie geglättet und als geschlossene Linie exportiert. An steilen Kanten liegt die Genauigkeit bei etwa einer halben Rasterweite. Starke Umriss-Glättung verkleinert Rundungen leicht (Löcher werden etwas kleiner, Außenradien etwas kleiner). Für einen Körper mit glattem Rand in CAD: DXF als Skizze importieren, extrudieren, Netz/STEP als Referenz für die Oberfläche verwenden.

Koordinaten beziehen sich auf den Startpunkt des Scans. Bei großen Scans werden die Dateien sehr groß (ca. 400 000 Punkte → STL/3MF 80–100 MB); für STEP dann die Ausdünnung auf 4 oder mehr stellen.

## Nachbearbeitung

**MeshLab** (kostenlos, liest STL): *File → Import Mesh* → „Unify duplicated vertices“ mit **Ja** bestätigen → *Filters → Smoothing, Fairing and Deformation → Taubin Smooth* (10–20 Durchläufe) → *Export Mesh As*.

**CloudCompare:** CSV als Punktwolke laden, *Plugins → Poisson Surface Reconstruction* erzeugt eine glatte Oberfläche.

**Glatte CAD-Flächen:** STEP besteht aus vielen ebenen Facetten. Echte Freiformflächen erfordern Flächenrückführung (z. B. Fusion 360 *Convert Mesh*, QuickSurface, Geomagic Design X).

# Empfohlene Einstellungen für beste Qualität

| Bereich | Empfehlung |
|---|---|
| Sensor | Analogausgang auf den benötigten Höhenbereich skalieren (falls vom Sensor unterstützt), genaueste Ansprechzeit, Teil bei ca. 100 mm Abstand, 10–15 min aufwärmen |
| Oberfläche | glänzende, dunkle oder transparente Flächen mit Scan- oder Kreidespray mattieren |
| Messart | Anhalten an jedem Punkt, Serpentine aus |
| Beruhigung / Mittelung | 80–100 ms / ADS1115: 8–16 |
| Auflösung | 0,25–0,5 mm |
| Beschleunigung | 500–800 mm/s² |
| Fliegend (wenn nötig) | 20–30 mm/s, Serpentine aus, Sensor-Verzögerung an Kante einstellen |

Wird der Sensorausgang skaliert, `DIST_AT_MIN_MM` / `DIST_AT_MAX_MM` in `config.h` anpassen und neu kalibrieren.

# Speicher und Updates

## Scan-Speicher

Scans werden zeilenweise im Flash (LittleFS) abgelegt. Geschrieben wird nur, wenn die Achsen stehen (am Zeilenende), damit die Motoren nicht ruckeln. Der letzte Scan inkl. Start-/Endzeit bleibt bei Neustart, Stromausfall und OTA erhalten; ein unterbrochener Scan wird als Teilscan geladen.

Der Statusbereich zeigt die Speichergröße und die maximal mögliche Punktzahl. Ist ein Scan zu groß, nennt die Fehlermeldung die maximale Punktzahl. **„Scan-Speicher formatieren“** (Bereich Firmware-Update) richtet den Speicher neu ein, z. B. nach einem Wechsel des Partitionsschemas.

## Firmware-Update (OTA)

1. In der Arduino IDE *Sketch → Kompilierte Binärdatei exportieren*.
2. Im Bereich *Firmware-Update* die Datei `CoreXY_Scanner.ino.bin` aus dem Ordner `build/…` wählen (nicht `.merged` oder `.bootloader`).
3. **„Hochladen & flashen“** – Fortschritt in Prozent und kB. Ein laufender Scan wird nach Rückfrage abgebrochen, die Motoren werden abgeschaltet.
4. Nach erfolgreichem Update startet der ESP32 neu, die Seite lädt nach 10 s neu. Das Erstelldatum der Firmware steht im Status.

Das Partitionsschema muss OTA unterstützen (nicht „Huge APP (no OTA)“).

# Fehlersuche

| Problem | Ursache / Abhilfe |
|---|---|
| Boot-Schleife mit `TG1WDT_SYS_RESET` | Pins GPIO 6–11 verwendet (Flash) → andere Pins wählen |
| MAC-Adresse 00:00:00:00:00:00 | ältere Firmware; aktuelle liest die MAC direkt aus dem Chip |
| WLAN verbindet nicht | Trennungsgrund im seriellen Monitor; nur 2,4 GHz; Router auf WPA2+WPA3; Sonderzeichen `"` `\` im Passwort mit `\` maskieren |
| „Nicht genug Flash: max. ca. 50176 Punkte“ | zu kleine Datenpartition → Partitionsschema „Default 4MB with spiffs“, **per USB** flashen, ggf. Speicher formatieren |
| Abstand „ungültig / kein Objekt“ | Objekt außerhalb 65–135 mm, Sensorsignal außerhalb 4–20 mA, ADS1115 nicht gefunden |
| Hohes erscheint tief | `DIST_AT_MIN_MM` / `DIST_AT_MAX_MM` tauschen |
| Achse fährt falsch | siehe Tabelle unter *Inbetriebnahme* |
| Gezackte Kanten bei fliegender Messung | Sensor-Verzögerung anpassen oder Serpentine aus |
| Kleine Löcher / einzelne Spitzen im Teil | Messausfälle (Glanz, steile Flanken) → *Kleine Löcher schließen*, *Ausreißer entfernen*; Oberfläche ggf. mattieren |
| Stufen („Treppen“) in der Oberfläche | ADC-Auflösung → ADS1115, Glättung im Export oder in MeshLab |
| Motoren schalten ab | gewollt nach 1 min ohne Bewegung (`IDLE_DISABLE_MS`); stromlos kann das Portal verschoben werden, Nullpunkt dann neu setzen |
| Kopf stockt kurz während des Scans | Flash-Lesezugriff durch die Vorschau; Vorschau zuklappen oder Browser schließen |

# Technische Details

## CoreXY-Kinematik

Motor A = X + Y, Motor B = X − Y (jeweils in Schritten). Daraus X = (A + B) / 2, Y = (A − B) / 2. Reine X- oder Y-Fahrten bewegen beide Motoren gleich weit.

## Bewegungssteuerung

Ein Hardware-Timer erzeugt mit 40 kHz einen Interrupt. Darin laufen ein Trapez-Geschwindigkeitsprofil (Festkomma-Arithmetik, kein Float in der ISR) und ein Bresenham-Algorithmus, der beide Motoren synchron führt. Die maximale Stepfrequenz beträgt 20 kHz pro Motor (bei 80 Schritten/mm: 250 mm/s). Die Positionen werden im Interrupt gezählt und sind damit auch nach einem Abbruch korrekt.

Bibliotheken wie AccelStepper wurden bewusst nicht verwendet: Sie erzeugen Schritte durch Polling, was durch WLAN und Webserver zu Ruckeln führt, und beschleunigen die Motoren nicht koordiniert.

## Datenformat

- Abstände werden als `int16` in 0,01 mm gespeichert, ungültige Werte als −32768.
- `/scan.bin`: Zeilen hintereinander, je Zeile `nx` Werte in aufsteigender X-Richtung.
- `/scan.meta`: Rastergröße, Auflösung, Status (vollständig/Teilscan), Start-/Endzeit.
- Höhe Z = Abstand der Auflage (Ebene) − gemessener Abstand. Die Auswertung erfolgt im Browser.

## Web-API

| Pfad | Methode | Parameter | Funktion |
|---|---|---|---|
| `/` | GET | | Weboberfläche |
| `/api/status` | GET | | Status als JSON |
| `/api/data` | GET | `from` | Scanzeilen ab Zeile `from` (binär, Header `X-Row-From`/`X-Row-To`) |
| `/api/scan` | POST | `w h res feed travel settle avg bidir fly lat` | Scan starten |
| `/api/frame` | POST | `w h f` | Rahmen abfahren |
| `/api/pause` | POST | `on` (optional) | Pause ein/aus, ohne Parameter umschalten |
| `/api/stop` | POST | | Stopp / Abbruch |
| `/api/jog` | POST | `dx dy f` | relativ verfahren |
| `/api/home` | POST | `f` | zum Nullpunkt |
| `/api/zero` | POST | | Nullpunkt setzen |
| `/api/enable` | POST | `on` | Motoren an/aus |
| `/api/measure` | POST | `n` | Messung ohne Kalibrierung (JSON) |
| `/api/cal` | POST | `s o` bzw. `reset` | Kalibrierung setzen/zurücksetzen |
| `/api/format` | POST | | Scan-Speicher formatieren |
| `/update` | POST | Datei | Firmware-Update |
