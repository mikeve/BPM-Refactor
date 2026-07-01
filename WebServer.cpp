/******************************************************************************
 *
 *  WebServer.cpp
 *
 *  Bilge Pump Monitor
 *
 *  Handles the browser provisioning interface while operating in
 *  Access Point mode.
 *
 ******************************************************************************/

#include "WebServer.h"
#include "WiFiManager.h"

namespace
{
    //----------------------------------------------------------------------
    // Internal Functions
    //----------------------------------------------------------------------

    static void ProcessClient(WiFiClient& client);

    static void ProcessSaveRequest(WiFiClient& client);

    static void SendSetupPage(WiFiClient& client);

    static void SendSavedPage(WiFiClient& client);

    static bool ParseProvisioningData(
        const String& body,
        String& ssid,
        String& password);

    static String URLDecode(const String& text);
}

/******************************************************************************
 *
 *  Public Interface
 *
 ******************************************************************************/

namespace WebServer
{

void ProcessClient(WiFiClient& client)
{
    String request = client.readStringUntil('\r');

    Serial.println();
    Serial.print("REQUEST: ");
    Serial.println(request);

    if (request.startsWith("GET / "))
    {
        SendSetupPage(client);
    }
    else if (request.startsWith("POST /save"))
    {
        ProcessSaveRequest(client);
    }
    else
    {
        client.println("HTTP/1.1 404 Not Found");
        client.println();
    }

    delay(10);
    client.stop();
}

}

/******************************************************************************
 *
 *  Handle Browser Request
 *
 ******************************************************************************/

namespace
{

static void ProcessClient(WiFiClient& client)
{
    String request = client.readStringUntil('\r');

    Serial.println();
    Serial.print("REQUEST: ");
    Serial.println(request);

    //--------------------------------------------------------------
    // GET /
    //--------------------------------------------------------------

    if (request.startsWith("GET / "))
    {
        SendSetupPage(client);

        delay(10);

        client.stop();

        return;
    }

    //--------------------------------------------------------------
    // POST /save
    //--------------------------------------------------------------

    if (request.startsWith("POST /save"))
    {
        ProcessSaveRequest(client);

        delay(10);

        client.stop();

        return;
    }

    //--------------------------------------------------------------
    // Unknown request
    //--------------------------------------------------------------

    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();

    client.println("<html>");
    client.println("<body>");
    client.println("<h2>404 - Page Not Found</h2>");
    client.println("</body>");
    client.println("</html>");

    delay(10);

    client.stop();
}

/******************************************************************************
 *
 *  Handle POST /save
 *
 ******************************************************************************/

static void ProcessSaveRequest(WiFiClient& client)
{
    int contentLength = 0;

    //----------------------------------------------------------------------
    // Read HTTP headers
    //----------------------------------------------------------------------

    while (client.connected())
    {
        String line = client.readStringUntil('\n');

        line.trim();

        if (line.startsWith("Content-Length:"))
        {
            contentLength = line.substring(15).toInt();
        }

        //
        // Blank line = end of headers
        //
        if (line.length() == 0)
        {
            break;
        }
    }

    //----------------------------------------------------------------------
    // Read POST body
    //----------------------------------------------------------------------

    String body;

    unsigned long start = millis();

    while ((int)body.length() < contentLength &&
           (millis() - start) < 5000)
    {
        while (client.available())
        {
            body += (char)client.read();

            start = millis();
        }
    }

    Serial.println();
    Serial.print("POST BODY: ");
    Serial.println(body);

    //----------------------------------------------------------------------
    // Parse
    //----------------------------------------------------------------------

    String ssid;
    String password;

    if (!ParseProvisioningData(
            body,
            ssid,
            password))
    {
        SendSetupPage(client);

        return;
    }

    Serial.print("SSID: ");
    Serial.println(ssid);

    Serial.print("Password: ");
    Serial.println(password);

    //----------------------------------------------------------------------
    // Save configuration
    //----------------------------------------------------------------------

    if (!SaveAdHocNetwork(
            ssid,
            password))
    {
        SendSetupPage(client);

        return;
    }

    //----------------------------------------------------------------------
    // Tell WiFiManager to reconnect
    //----------------------------------------------------------------------

    RestartConnection();

    //----------------------------------------------------------------------
    // Inform browser
    //----------------------------------------------------------------------

    SendSavedPage(client);
}

/******************************************************************************
 *
 *  Parse Provisioning Data
 *
 *  POST body format:
 *
 *      ssid=MyNetwork&password=Secret123
 *
 ******************************************************************************/

static bool ParseProvisioningData(
    const String& body,
    String& ssid,
    String& password)
{
    int ssidStart = body.indexOf("ssid=");
    int passStart = body.indexOf("&password=");

    if (ssidStart < 0 || passStart < 0)
    {
        return false;
    }

    ssid =
        body.substring(
            ssidStart + 5,
            passStart);

    password =
        body.substring(
            passStart + 10);

    ssid = URLDecode(ssid);

    //
    // Windows browsers occasionally submit smart apostrophes
    // as Windows-1252. Convert them to UTF-8.
    //
    ssid.replace(
        "\x92",
        "\xE2\x80\x99");

    password = URLDecode(password);

    return true;
}

/******************************************************************************
 *
 *  URL Decode
 *
 ******************************************************************************/

static String URLDecode(const String& text)
{
    String decoded;

    for (int i = 0; i < text.length(); i++)
    {
        if (text[i] == '+')
        {
            decoded += ' ';
            continue;
        }

        if (text[i] == '%' &&
            i + 2 < text.length())
        {
            char hex[3];

            hex[0] = text[i + 1];
            hex[1] = text[i + 2];
            hex[2] = '\0';

            decoded +=
                (char)strtol(
                    hex,
                    nullptr,
                    16);

            i += 2;

            continue;
        }

        decoded += text[i];
    }

    return decoded;
}

/******************************************************************************
 *
 *  Send Provisioning Page
 *
 ******************************************************************************/

static void SendSetupPage(WiFiClient& client)
{
    String html;

    //
    // Prevent heap fragmentation while building the page.
    //
    html.reserve(2048);

    html += F("<!DOCTYPE html>");
    html += F("<html>");
    html += F("<head>");
    html += F("<meta charset='UTF-8'>");
    html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>Bilge Pump Monitor Setup</title>");

    html += F("<style>");
    html += F("body{");
    html += F("font-family:Arial,Helvetica,sans-serif;");
    html += F("max-width:500px;");
    html += F("margin:40px auto;");
    html += F("padding:20px;");
    html += F("}");
    html += F("input{");
    html += F("width:100%;");
    html += F("padding:10px;");
    html += F("margin-top:4px;");
    html += F("margin-bottom:16px;");
    html += F("box-sizing:border-box;");
    html += F("}");
    html += F("input[type=submit]{");
    html += F("width:auto;");
    html += F("padding:10px 24px;");
    html += F("}");
    html += F(".info{");
    html += F("background:#f4f4f4;");
    html += F("padding:12px;");
    html += F("border-radius:6px;");
    html += F("margin-bottom:20px;");
    html += F("}");
    html += F("</style>");

    html += F("</head>");
    html += F("<body>");

    html += F("<h2>Bilge Pump Monitor Setup</h2>");

    html += F("<div class='info'>");

    html += F("<b>Last Connected SSID</b><br>");
    html += GetLastSSID();          // New WiFiManager accessor

    html += F("<br><br>");

    html += F("<b>Current Ad Hoc SSID</b><br>");
    html += GetAdhocSSID();         // New WiFiManager accessor

    html += F("</div>");

    html += F("<form method='POST' action='/save'>");

    html += F("<label for='ssid'>SSID</label>");
    html += F("<input "
              "id='ssid' "
              "name='ssid' "
              "type='text' "
              "autocomplete='off'>");

    html += F("<label for='password'>Password</label>");
    html += F("<input "
              "id='password' "
              "name='password' "
              "type='password'>");

    html += F("<input type='submit' value='Save'>");

    html += F("</form>");

    html += F("<p>");
    html += F("Saving replaces the currently configured ad hoc network.");
    html += F("</p>");

    html += F("</body>");
    html += F("</html>");

    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));
    client.println();

    client.print(html);
}

/******************************************************************************
 *
 *  Network Saved Page
 *
 ******************************************************************************/

static void SendSavedPage(WiFiClient& client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();

    client.println("<!DOCTYPE html>");

    client.println("<html>");

    client.println("<head>");

    client.println("<meta charset='UTF-8'>");

    client.println("<title>");
    client.println("Bilge Pump Monitor");
    client.println("</title>");

    client.println("</head>");

    client.println("<body>");

    client.println("<h2>");
    client.println("Network Saved");
    client.println("</h2>");

    client.println("<p>");
    client.println("Attempting connection...");
    client.println("</p>");

    client.println("</body>");

    client.println("</html>");
}

}       // anonymous namespace