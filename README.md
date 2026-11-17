![Doku](scanner-system.svg)
# CoreXY Laser-Oberflächenscanner

**Ein Laser tastet ein Teil Punkt für Punkt ab – heraus kommt ein fertiges 3D-Modell für CAD, 3D-Druck oder Laser, bedient komplett aus dem Browser.**

![Systemübersicht](scanner-system.svg)
---

## Das Wichtigste in Kürze

| | |
|---|---|
| **Messprinzip** | Laser-Triangulationssensor **Panasonic HG-C1100**, Messbereich 65–135 mm, berührungslos |
| **Mechanik** | **CoreXY-Portal** mit zwei Closed-Loop-Schrittmotoren und GT2-Riemen, 80 Schritte/mm |
| **Steuerung** | ein **ESP32**: Motoren, Messung, Speicher und Webserver auf einem Board |
| **Höhenauflösung** | rechnerisch ca. **0,003 mm** mit 16-Bit-Wandler ADS1115 (vorbereitet) |
| **Ergebnis** | **CSV, 3MF, STL, STEP** und Umriss als **DXF / SVG** – direkt aus dem Browser |
| **Bedienung** | jeder Browser im Heimnetz: PC, Tablet oder Handy – keine Installation, keine App |

---

## Vorteile im Einzelnen

### Präzise Bewegung trotz WLAN
Die Motorschritte erzeugt ein Hardware-Timer mit 40 kHz – unabhängig von Webserver und Funk. Beide Motoren werden synchron mit gemeinsamer Beschleunigungsrampe geführt (Bresenham + Trapezprofil). Die Position wird in jedem Schritt gezählt und stimmt auch nach einem Abbruch. Closed-Loop-Motoren verhindern Schrittverluste.

### Zwei Messarten
- **Anhalten an jedem Punkt** – Punkt anfahren, beruhigen, mehrfach mitteln: höchste Genauigkeit.
- **Fliegende Messung** – die Zeile wird ohne Anhalten durchfahren, gemessen wird an der Ist-Position der Motoren. Ein Teil von 100 × 100 mm bei 1 mm Raster dauert so ca. **9 Minuten** statt einer halben Stunde.

### Kein Datenverlust
Jede fertige Zeile wird sofort im Flash gespeichert – geschrieben wird nur, wenn der Kopf steht, damit nichts ruckelt. Ein Scan übersteht Stromausfall, Neustart und Firmware-Update. Ein abgebrochener Scan wird als fertiger Teilscan übernommen. Platz für rund **640 000 Messpunkte** (z. B. 90 × 280 mm bei 0,25 mm Raster).

### Nur das Teil, nicht die Auflage
Die Auflagefläche wird automatisch als Ebene erkannt – auch wenn sie leicht schräg liegt – und als Z = 0 abgezogen. Exportiert wird nur das Teil selbst, als geschlossener Volumenkörper mit Oberfläche, Wänden und Boden.

### Saubere Daten ohne Nacharbeit
- **Fehlstellen schließen:** Messausfälle im Teil werden glatt aus der Umgebung ergänzt, echte Bohrungen bleiben offen.
- **Ausreißer entfernen:** einzelne Spitzen fliegen raus, Kanten bleiben erhalten.
- **Kantenerhaltendes Glätten:** Welligkeit verschwindet, Stufen bleiben scharf.
- **Ebene Flächen begradigen:** ebene Bereiche werden exakt eben, gekrümmte bleiben unverändert.
- **Umriss als geschlossene Linie:** Kontur zwischen den Rasterpunkten interpoliert, geglättet, mit Außenkontur und Löchern – direkt für CAD-Skizze, Laser oder CNC.

### Alles im Browser
Die Weboberfläche zeigt live Position, Höhe, ein Höhenbild des Scans und den Fortschritt – mit großer Prozentanzeige, Start- und Endzeit (NTP) und Restzeit. Die Auswertung und alle Exporte rechnet der Browser, der ESP32 bleibt frei für die Messung. Firmware-Updates per Dateiauswahl über WLAN.

---

## Was der Scanner kann

| Funktion | Beschreibung |
|---|---|
| Scanbereich | X × Y ab der aktuellen Position, Rasterweite frei wählbar |
| Rahmen abfahren | fährt den Scanbereich einmal ab – Lichtpunkt zeigt, ob das Teil vollständig drinliegt |
| Pause / Weiter | hält an einer sicheren Stelle an, Position bleibt erhalten |
| Stopp | sofort, bisherige Daten bleiben als Teilscan erhalten |
| Live-Vorschau | Höhenbild während des Scans, optional mit Umriss |
| Höhenkalibrierung | zwei Punkte mit Endmaß, dauerhaft gespeichert |
| Zeitanzeige | Start, voraussichtliches Ende, Gesamtdauer, Restzeit – Pausen werden herausgerechnet |
| Energiesparen | Motoren schalten nach 1 Minute ohne Bewegung ab |
| Speicheranzeige | freier Platz und maximale Punktzahl, Speicher formatierbar |
| OTA-Update | neue Firmware per Dateiauswahl mit Fortschrittsanzeige |

---

## Technische Daten

| | |
|---|---|
| Controller | ESP32 Dev Module (ESP32-D0WD-V3), 4 MB Flash |
| Antrieb | 2 × Closed-Loop-Schrittmotor, GT2-Riemen, 20-Zahn-Pulley, 16 Mikroschritte |
| Schrittauflösung | 0,0125 mm, max. 20 kHz Schrittfrequenz pro Motor |
| Sensor | Panasonic HG-C1100, 100 ± 35 mm, Analogausgang 4–20 mA |
| Wandler | intern 12 Bit oder ADS1115 16 Bit (I²C), umschaltbar |
| Sinnvolles Raster | 0,25–1 mm |
| Speicher | LittleFS im Flash, ca. 640 000 Punkte |
| Netzwerk | WLAN 2,4 GHz, DHCP oder feste IP, NTP-Zeit |
| Exportformate | CSV, 3MF, STL, STEP (AP214), DXF, SVG |
| Software | Arduino IDE, Arduino-ESP32 ab 3.0, keine externen Bibliotheken |

---

## Bedienung in vier Schritten

1. **Teil auflegen** und den Kopf mit den Pfeiltasten auf die linke vordere Ecke fahren.
2. **Größe und Raster eintragen** und mit **„Rahmen abfahren“** prüfen, ob das Teil vollständig im Bereich liegt.
3. **„Scan starten“** – den Fortschritt live verfolgen, bei Bedarf pausieren.
4. **Exportieren:** CSV, 3MF, STL, STEP oder den Umriss als DXF/SVG herunterladen – fertig für CAD, Slicer oder Laser.
