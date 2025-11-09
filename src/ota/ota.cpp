#include "ota.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <time.h>

namespace comfoair {
  
// Static members for log buffer
char OTA::logBuffer[LOG_BUFFER_SIZE][LOG_MESSAGE_MAX_LEN];
int OTA::logIndex = 0;
int OTA::logCount = 0;

// Add log message to circular buffer
void OTA::addLog(const char* message) {
  // Get current time
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  // Format: HH:MM:SS - message
  // If time is not synced yet (year < 2020), fall back to milliseconds
  if (timeinfo.tm_year > (2020 - 1900)) {
    snprintf(logBuffer[logIndex], LOG_MESSAGE_MAX_LEN, "%02d:%02d:%02d - %s", 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, message);
  } else {
    // Fallback to milliseconds if time not synced yet
    snprintf(logBuffer[logIndex], LOG_MESSAGE_MAX_LEN, "%lums: %s", millis(), message);
  }
  
  logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
  if (logCount < LOG_BUFFER_SIZE) logCount++;
}

/*
 * Server Index Page with Serial Logs
 */

const char* serverIndex =
"<style>"
"body { font-family: Arial; margin: 20px; }"
"#logs { background: #000; color: #0f0; padding: 10px; height: 400px; overflow-y: scroll; font-family: monospace; font-size: 12px; }"
"button { margin: 10px 5px; padding: 10px 20px; font-size: 14px; }"
".restart-btn { background-color: #ff6b6b; color: white; border: none; cursor: pointer; }"
".restart-btn:hover { background-color: #ff5252; }"
"</style>"
"<h1>ComfoAir ESP32 - OTA Update & Debug</h1>"
"<h2>Serial Logs (Last 100 messages)</h2>"
"<div id='logs'>Loading logs...</div>"
"<button onclick='refreshLogs()'>Refresh Logs</button>"
"<button onclick='clearLogs()'>Clear Display</button>"
"<button onclick='autoRefresh()' id='autoBtn'>Auto-Refresh: OFF</button>"
"<button class='restart-btn' onclick='restartDevice()'>Restart Device</button>"
"<h2>Firmware Update</h2>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update'>"
"<input type='submit' value='Update Firmware'>"
"</form>"
"<div id='prg'>progress: 0%</div>"
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<script>"
"var autoRefreshInterval = null;"
"function refreshLogs() {"
"  $.get('/logs', function(data) {"
"    $('#logs').html(data.replace(/\\n/g, '<br>'));"
"    $('#logs').scrollTop($('#logs')[0].scrollHeight);"
"  });"
"}"
"function clearLogs() {"
"  $('#logs').html('');"
"}"
"function autoRefresh() {"
"  if (autoRefreshInterval) {"
"    clearInterval(autoRefreshInterval);"
"    autoRefreshInterval = null;"
"    $('#autoBtn').text('Auto-Refresh: OFF');"
"  } else {"
"    autoRefreshInterval = setInterval(refreshLogs, 1000);"
"    $('#autoBtn').text('Auto-Refresh: ON');"
"    refreshLogs();"
"  }"
"}"
"function restartDevice() {"
"  if (confirm('Are you sure you want to restart the device?')) {"
"    $.post('/restart', function(data) {"
"      alert('Device is restarting... Page will reload in 10 seconds.');"
"      setTimeout(function() { location.reload(); }, 10000);"
"    });"
"  }"
"}"
"refreshLogs();"
"$('form').submit(function(e){"
"  e.preventDefault();"
"  var form = $('#upload_form')[0];"
"  var data = new FormData(form);"
"  $.ajax({"
"    url: '/update',"
"    type: 'POST',"
"    data: data,"
"    contentType: false,"
"    processData:false,"
"    xhr: function() {"
"      var xhr = new window.XMLHttpRequest();"
"      xhr.upload.addEventListener('progress', function(evt) {"
"        if (evt.lengthComputable) {"
"          var per = evt.loaded / evt.total;"
"          $('#prg').html('progress: ' + Math.round(per*100) + '%');"
"        }"
"      }, false);"
"      return xhr;"
"    },"
"    success:function(d, s) {"
"      console.log('success!')"
"    },"
"    error: function (a, b, c) {"
"    }"
"  });"
"});"
"</script>";
  WebServer server(80);

  void OTA::setup() {
    /*use mdns for host name resolution*/
    if (!MDNS.begin("comfoesp32")) { //http://esp32.local
      Serial.println("Error setting up MDNS responder!");
    }
    
    // Main page with logs and OTA
    server.on("/", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });
    
    // Logs endpoint - returns last N log messages
    server.on("/logs", HTTP_GET, []() {
      String response = "";
      response.reserve(LOG_BUFFER_SIZE * 128);  // Pre-allocate approximate size
      int start = (logCount < LOG_BUFFER_SIZE) ? 0 : logIndex;
      for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % LOG_BUFFER_SIZE;
        response += logBuffer[idx];
        response += "\n";
      }
      server.send(200, "text/plain", response);
    });
    
    // Restart endpoint - soft reset the device
    server.on("/restart", HTTP_POST, []() {
      server.send(200, "text/plain", "Restarting...");
      delay(500);  // Give time for response to be sent
      ESP.restart();
    });
    
    /*handling uploading firmware file */
    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
    server.begin();
  }

  void OTA::loop() {
    server.handleClient();
    delay(1);
  }
}