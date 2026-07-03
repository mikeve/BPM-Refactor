#pragma once

#include <Arduino.h>
#include <WiFiS3.h>
#include <SD.h>

//
// Connection timing
//
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS   = 10000;
constexpr unsigned long WIFI_RECONNECT_WINDOW_MS  = 120000;
constexpr unsigned long WIFI_STATUS_CHECK_MS      = 5000;

//
// SD Card Configuration
//
#define NETWORK_CONFIG_FILE "/config/network.cfg"

//
// AP Configuration
//
#define AP_PASSWORD "bilgepump"
#define AP_IP       IPAddress(192,168,4,1)
#define AP_GATEWAY  IPAddress(192,168,4,1)
#define AP_SUBNET   IPAddress(255,255,255,0)

//
// WiFi States
//
enum WIFI_STATE
{
    WIFI_STARTUP,
    WIFI_TRY_NETWORK,
    WIFI_CONNECTED,
    WIFI_RECOVERING,
    WIFI_AP_MODE
};

//
// Network Credentials
//
struct NetworkCredentials
{
    String ssid;
    String password;
};

//
// Configuration loaded from SD card
//
struct NetworkConfig
{
    String lastSSID;
    String adhocSSID;
    String adhocPassword;
};

//
// Public Globals
//
extern WIFI_STATE wifiState;

//
// Public Interface
//
bool InitWiFiManager();

void ServiceWiFiManager();

bool IsWiFiConnected();

bool IsAPMode();

String GetCurrentSSID();

String GetCurrentIPAddress();

String GetLastSSID();

String GetAdhocSSID();

//
// Called by WebServer after a successful POST.
//
void RestartConnection();

//
// Save (or replace) the stored ad hoc network.
//
bool SaveAdhocNetwork(
    const String& ssid,
    const String& password);

//
// Utility
//
String GetWiFiStateString();
