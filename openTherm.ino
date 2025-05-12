#include <Arduino.h>
#include <OpenTherm.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Gas Boiler"; // TODO: chose your ssid name
const char* password = "12345678"; // TODO: change default

const int inPin = 12;
const int outPin = 13;
const int switchPin = 25; // start
const int ledPin = 26;      // LED для аварії
const int activityLed = 27; // LED активності

OpenTherm ot(inPin, outPin);
WebServer server(80);

float boilerSetTemp = 64;
float minBoilerTemperature = 5;
float maxBoilerTemperature = 75;
bool centralHeating = false;
bool flameOn = false;
bool faultDetected = false;
float temperature = 0.0;

void IRAM_ATTR handleInterrupt() {
  ot.handleInterrupt();
}

void updateBoilerStatus() {
  bool enableCentralHeating = digitalRead(switchPin) == LOW;
  unsigned long response = ot.setBoilerStatus(enableCentralHeating, false, false);

  if (ot.isValidResponse(response)) {
    digitalWrite(activityLed, HIGH);
    delay(50);
    digitalWrite(activityLed, LOW);
  }

  ot.setBoilerTemperature(boilerSetTemp);

  centralHeating = ot.isCentralHeatingActive(response);
  flameOn = ot.isFlameOn(response);
  faultDetected = ot.isFault(response);

  temperature = ot.getBoilerTemperature();

  digitalWrite(ledPin, faultDetected ? HIGH : LOW);

// Виводимо дані
    Serial.println("Boiler temperature: " + String(temperature) + " °C");
}

void setup() {
  Serial.begin(115200);
  pinMode(switchPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(activityLed, OUTPUT);

  ot.begin(handleInterrupt);

  WiFi.softAP(ssid, password);
  Serial.println("Access Point started...");

  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Boiler Monitor</title>
    <style>
        body { font-family: sans-serif; padding: 8px; }
        .temperature { font-size: 4em; font-weight: bold; text-align: center; }
        .label { font-weight: bold; }
        .link {
            position: absolute;
            bottom: 8px;
            left: 8px;
        }
        .input {
            width: auto;
            height: 24px;
            font-size: 1em;
            margin-left: 10px;
            border-radius: 4px;
            border-width: 1px;
            border-color: #eeecec;
        }
        .button { width: 100%; height: 40px; font-size: 1em; font-weight: bold; border-radius: 8px; margin-top: 20px}
        .statusArea { background: #eeecec; padding: 2px 8px; border-radius: 8px;}
    </style>
</head>
<body>
<h2>Gas Boiler</h2>
<div id="currentTemp"></div>
<div class="statusArea">
    <h2>Status:</h2>
    <div id="status"></div>
</div>
<h3>Set Desired Temperature:</h3>
<label for="tempInput">Set to:</label>
<input class="input" type="number" id="tempInput" value="64" min="10" max="90">
<button class="button" onclick="setTemperature()">Set</button>
<a class="link" href="https://github.com/Panibrat/Gas-boiler-OpenTherm">Github Link</a>

<script>
    function fetchData() {
        fetch('/data')
            .then(response => response.json())
            .then(data => {
                document.getElementById("currentTemp").innerHTML = `
                <h1 class="temperature" id="currentTemp">${Number(data.temp).toFixed(1)} &#8451</h1>
                `;
                document.getElementById("status").innerHTML = `
                  <p><span class="label">Heating:</span> ${data.ch}</p>
                  <p><span class="label">Flame:</span> ${data.flame}</p>
                  <p><span class="label">Fault:</span> ${data.fault}</p>
                  <p><span class="label">Boiler Set Temp:</span> ${data.boilerSetTemp}  &#8451</p>
                `;
            });
    }

    function setTemperature() {
        const val = document.getElementById("tempInput").value;
        fetch(`/setTemp?value=${val}`);
    }

    setInterval(fetchData, 2000);
    fetchData();
</script>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", html);
  });

  server.on("/setTemp", HTTP_GET, []() {
    if (server.hasArg("value")) {
    float requestedTemp = server.arg("value").toFloat();

    if (requestedTemp < minBoilerTemperature) requestedTemp = minBoilerTemperature;
    if (requestedTemp > maxBoilerTemperature) requestedTemp = maxBoilerTemperature;

    boilerSetTemp = requestedTemp;
  }
    Serial.println("setTemp: " + String(boilerSetTemp) + " C");
    server.send(200, "text/plain", "OK");
  });

  server.on("/data", HTTP_GET, []() {
    String json = "{";
    json += "\"ch\":\"" + String(centralHeating ? "on" : "off") + "\",";
    json += "\"flame\":\"" + String(flameOn ? "on" : "off") + "\",";
    json += "\"temp\":" + String(temperature) + ",";
    json += "\"boilerSetTemp\":" + String(boilerSetTemp) + ",";
    json += "\"fault\":\"" + String(faultDetected ? "YES" : "NO") + "\"";
    json += "}";

    server.send(200, "application/json", json);
  });

  server.begin();
}

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 1000;

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    updateBoilerStatus();
    lastUpdate = now;
  }
}
