#pragma once

// Starts SPIFFS, SoftAP, and the web server (open AP: "Areca-Classifier").
void webPortalBegin();

// Non-blocking HTTP handler; call from loop().
void webPortalPoll();
