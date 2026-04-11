#include <ArduinoOTA.h>

/**
 * Configures and starts the ArduinoOTA service.
 * Sets the OTA hostname to HOST_NAME and registers callbacks for:
 *   - onStart:    clears the display and shows the update type
 *   - onEnd:      shows "Restarting..." before the reboot
 *   - onProgress: draws a progress bar on the OLED
 *   - onError:    displays the error reason on the OLED
 * Must be called once in setup() after WiFi is connected.
 */
void OTAsetup()
{
  ArduinoOTA.setHostname(HOST_NAME.c_str());

  ArduinoOTA.onStart([]()
                     {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, "OTA Update");
    display.display();
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else
      type = "filesystem";
    Serial.println("Start updating " + type);
    watchDogCount = 0; });

  ArduinoOTA.onEnd([]()
                   {
    Serial.println("\nEnd");
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Restarting...");
    display.display();
    watchDogCount = 0; });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    display.drawProgressBar(4, 32, 120, 8, progress / (total / 100));
    display.display();
    watchDogCount = 0; });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);

    if (error == OTA_AUTH_ERROR) display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Auth Failed");
    else if (error == OTA_BEGIN_ERROR) display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Begin Failed");
    else if (error == OTA_CONNECT_ERROR) display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Connection Failed");
    else if (error == OTA_RECEIVE_ERROR) display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Receive Failed");
    else if (error == OTA_END_ERROR) display.drawString(display.getWidth() / 2, display.getHeight() / 2, "End Failed");
    display.display(); });

  ArduinoOTA.begin();
}
