#pragma once
// ===== Zugangsdaten & Netzwerk =====
#define WIFI_SSID   "MyRouter"
#define WIFI_PASS   "12345674"
 
// 0 = DHCP (IP-Reservierung in der Fritzbox), 1 = feste IP im ESP32
#define USE_STATIC_IP  0
 
// Statische IP (Kommas statt Punkte!)
#define LOCAL_IP     192, 168, 55, 113
#define GATEWAY_IP   192, 168, 55, 254
#define SUBNET_MASK  255, 255, 255, 0
#define DNS_IP       192, 168, 55, 254

#define HOSTNAME     "corexy-scanner"
