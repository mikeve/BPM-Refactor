#include "WiFiManager.h"
#include "arduino_secrets_mv.h"
#include "WebServer.h"

WIFI_STATE wifiState = WIFI_STARTUP;
//
// Local web server used while in AP mode.
//
static WiFiServer gWebServer(80);

//
// Configuration
//
static NetworkConfig gConfig;

static unsigned long gConnectStartTime = 0;

static unsigned long gDisconnectTime = 0;

static bool gConnectionAttemptActive = false;

static bool gRestartRequested = false;

//
// State machine timers
//
static unsigned long gStateStartTime = 0;
static unsigned long gLastStatusCheck = 0;

//
// Current connection
//
static String gCurrentSSID = "";
static int gCurrentNetworkIndex = 0;
static int gCurrentCycle = 0;

//
// AP mode
//
static bool gAPStarted = false;

//
// Search list
//
static NetworkCredentials gSearchList[20];
static int gSearchCount = 0;

static bool LoadConfig();
static bool SaveConfig();

static void BuildSearchList();

static void BeginNetworkSearch();

static void StartAPMode();
static void StopAPMode();

static void BeginConnection(const String& ssid, const String& password);

static bool ConnectionSucceeded();

static void SaveLastSSID(const String& ssid);

static bool LoadConfig() {
    gConfig.lastSSID = "";
    gConfig.adhocSSID = "";
    gConfig.adhocPassword = "";

    if (!SD.exists(NETWORK_CONFIG_FILE))
    {
        Serial.println(
            "No network config file.");

        return false;
    }
    File file = SD.open(NETWORK_CONFIG_FILE, FILE_READ);

    if (!file)
        return false;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("LAST_SSID=")) {
            gConfig.lastSSID = line.substring(10);
        }
        else if (line.startsWith("ADHOC_SSID=")) {
            gConfig.adhocSSID = line.substring(11);
        }
        else if (line.startsWith("ADHOC_PASSWORD=")) {
            gConfig.adhocPassword = line.substring(15);
        }
    }
    file.close();
    return true;
}

static bool SaveConfig() {
    if (!SD.exists("/config")) {
        SD.mkdir("/config");
    }


    File file = SD.open(NETWORK_CONFIG_FILE, O_WRITE | O_CREAT | O_TRUNC);

    if (!file) {
        return false;
    }
    file.print("LAST_SSID=");
    file.println(gConfig.lastSSID);

    file.print("ADHOC_SSID=");
    file.println(gConfig.adhocSSID);

    file.print("ADHOC_PASSWORD=");
    file.println(gConfig.adhocPassword);

    file.close();

    return true;
}

bool SaveAdhocNetwork(const String& ssid, const String& password) {
    gConfig.adhocSSID = ssid;
    gConfig.adhocPassword = password;

    return SaveConfig();
}

static void SaveLastSSID(const String& ssid) {
    if (gConfig.lastSSID == ssid) {
        return;
    }
    gConfig.lastSSID = ssid;
    SaveConfig();
}

static void BuildSearchList() {
    gSearchCount = 0;
    // Last SSID
    if (gConfig.lastSSID.length()) {
        for (int i = 0; i < intNET_COUNT; i++) {
            if (gConfig.lastSSID == myipData[i].ssid) {
                gSearchList[gSearchCount++] = {
                    myipData[i].ssid,
                    myipData[i].password
                };
            }
        }
        if (gConfig.lastSSID == gConfig.adhocSSID) {
            gSearchList[gSearchCount++] = {
                gConfig.adhocSSID,
                gConfig.adhocPassword
            };
        }
    }

    // Ad Hoc
    if (gConfig.adhocSSID.length()) {
        if (gConfig.adhocSSID != gConfig.lastSSID) {
            gSearchList[gSearchCount++] =
            {
                gConfig.adhocSSID,
                gConfig.adhocPassword
            };
        }
    }

    // Known Networks
    for (int i = 0; i < intNET_COUNT; i++) {
        if (!gConfig.lastSSID.equals(myipData[i].ssid)) {
            gSearchList[gSearchCount++] =
            {
                myipData[i].ssid,
                myipData[i].password
            };
        }
    }
}

static void BeginConnection(const String& ssid, const String& password) {
    Serial.print("Trying: ");
    Serial.println(ssid);

    WiFi.disconnect();

    gCurrentSSID = ssid;

    WiFi.begin(ssid.c_str(), password.c_str());

    gStateStartTime = millis();
}

static bool ConnectionSucceeded()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    IPAddress ip = WiFi.localIP();

    return (ip != IPAddress(0, 0, 0, 0));
}

static void StartAPMode() {
    if (gAPStarted)
        return;

    String apName = "BilgeMonitor-";
    byte mac[6];
    WiFi.macAddress(mac);
    char suffix[5];
    sprintf(suffix, "%02X%02X", mac[4], mac[5]);
    apName += suffix;

    WiFi.config(AP_IP, AP_GATEWAY, AP_SUBNET);

    WiFi.beginAP(apName.c_str(), AP_PASSWORD);

    Serial.println();
    Serial.println("AP MODE STARTED");
    Serial.println(apName);
    Serial.println("192.168.4.1");

    gWebServer.begin();

    gAPStarted = true;
}

static void StopAPMode()
{
    if (!gAPStarted)
        return;

    WiFi.end();

    gAPStarted = false;
}

bool InitWiFiManager()
{
    LoadConfig();

    BuildSearchList();

    gCurrentCycle = 0;
    gCurrentNetworkIndex = 0;


    Serial.print("Search count: ");
    Serial.println(gSearchCount);

    for (int i = 0; i < gSearchCount; i++)
    {
        Serial.print(i);
        Serial.print(": ");
        Serial.println(gSearchList[i].ssid);
    }


    wifiState =
        WIFI_TRY_NETWORK;

    return true;
}

void ServiceWiFiManager() {
    static unsigned long ulLastReport = 0;

    if (millis() - ulLastReport > 5000) {
        ulLastReport = millis();

        Serial.print("WiFi State: ");
        Serial.println(GetWiFiStateString());
    }

    switch (wifiState) {
        case WIFI_STARTUP:
            break;

        case WIFI_TRY_NETWORK:
            // Start next connection attempt
            if (!gConnectionAttemptActive) {
                if (gCurrentNetworkIndex >= gSearchCount) {
                    gCurrentCycle++;
                    if (gCurrentCycle >= 3) {
                        Serial.println("No networks found.");
                        wifiState = WIFI_AP_MODE;
                        break;
                    }
                    gCurrentNetworkIndex = 0;
                }

                BeginConnection(
                    gSearchList[gCurrentNetworkIndex].ssid,
                    gSearchList[gCurrentNetworkIndex].password);
                gConnectionAttemptActive = true;
                gConnectStartTime = millis();
            }
            //
            // Connected?
            if (ConnectionSucceeded()) {
                Serial.print("Connected to ");
                Serial.println(gCurrentSSID);

                Serial.print("IP: ");
                Serial.println(WiFi.localIP());

                SaveLastSSID(gCurrentSSID);

                gConnectionAttemptActive = false;
                wifiState = WIFI_CONNECTED;
                break;
            }
            //
            // Timeout?
            if ((millis() - gConnectStartTime) > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.print("Failed: ");
                Serial.println(gCurrentSSID);

                gConnectionAttemptActive = false;

                gCurrentNetworkIndex++;
            }
            break;

        case WIFI_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("Connection lost.");

                gDisconnectTime = millis();

                wifiState = WIFI_RECOVERING;
            }
            break;
            
        case WIFI_RECOVERING:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Connection restored.");
                wifiState = WIFI_CONNECTED;
                break;
            }

            if ((millis() - gDisconnectTime) > WIFI_RECONNECT_WINDOW_MS) {
                Serial.println("Starting network search.");
                BuildSearchList();
                gCurrentCycle = 0;
                gCurrentNetworkIndex = 0;
                gConnectionAttemptActive = false;

                wifiState = WIFI_TRY_NETWORK;
            }
            break;
    
        case WIFI_AP_MODE:

            if (!gAPStarted)
            {
                StartAPMode();
            }

            WiFiClient client = gWebServer.available();

            if (client)
            {
                WebServer::ProcessClient(client);
            }

            if (gRestartRequested)
            {
                gRestartRequested = false;

                WiFi.end();

                gAPStarted = false;

                BuildSearchList();

                gCurrentCycle = 0;
                gCurrentNetworkIndex = 0;
                gConnectionAttemptActive = false;

                wifiState = WIFI_TRY_NETWORK;
            }

            break;
    }
}

bool IsWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String GetCurrentSSID() {
    return WiFi.SSID();
}

String GetCurrentIPAddress() {
    if (!IsWiFiConnected())
        return "";

    return WiFi.localIP().toString();
}

bool IsAPMode() {
    return wifiState == WIFI_AP_MODE;
}

/******************************************************************************
 *
 *  Restart Connection
 *
 ******************************************************************************/
 
void RestartConnection()
{
    Serial.println("*** RestartConnection() called ***");
    gRestartRequested = true;
}

/******************************************************************************
 *
 *  Configuration Accessors
 *
 ******************************************************************************/

String GetLastSSID()
{
    return gConfig.lastSSID;
}

String GetAdhocSSID()
{
    return gConfig.adhocSSID;
}

String GetWiFiStateString()
{
    switch (wifiState)
    {
        case WIFI_STARTUP:
            return "STARTUP";

        case WIFI_TRY_NETWORK:
            return "TRY_NETWORK";

        case WIFI_CONNECTED:
            return "CONNECTED";

        case WIFI_RECOVERING:
            return "RECOVERING";

        case WIFI_AP_MODE:
            return "AP_MODE";

        default:
            return "UNKNOWN";
    }
}