#pragma once

#include <Arduino.h>
#include <WiFiS3.h>

namespace WebServer
{
    //
    // Process one connected browser.
    //
    void ProcessClient(WiFiClient& client);
}