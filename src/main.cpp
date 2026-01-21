
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_SW_I2C
u8g2(U8G2_R0, /*clock=*/12, /*data=*/14, U8X8_PIN_NONE);

const char *WIFI_SSID = "WildNetz";
const char *WIFI_PASS = "owgtb123";
bool toggle = true;
// Forward declaration
void clearTextLine(u8g2_uint_t x, u8g2_uint_t baselineY, u8g2_uint_t width);

void clearTextLine(u8g2_uint_t x, u8g2_uint_t baselineY, u8g2_uint_t width)
{
  int16_t ascent = u8g2.getAscent();   // positive
  int16_t descent = u8g2.getDescent(); // negative or zero

  // The font's pixel height
  uint16_t h = ascent - descent;

  // Top-left corner of the line box
  u8g2_uint_t boxY = baselineY - ascent;

  // Draw background (0) to clear the region
  u8g2.setDrawColor(0); // 0 = background (off) for monochrome
  u8g2.drawBox(x, boxY, width, h);

  // Restore normal draw color for subsequent text
  u8g2.setDrawColor(1);
}

AsyncWebServer server(80);
const int LED_PIN = LED_BUILTIN; // D4 on Wemos D1 mini

// Send JSON helper
void sendJson(AsyncWebServerRequest *request, JsonDocument &doc, int code = 200)
{
  String out;
  serializeJson(doc, out);
  AsyncWebServerResponse *res = request->beginResponse(code, "application/json", out);
  res->addHeader("Access-Control-Allow-Origin", "*"); // CORS for browser apps
  res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res->addHeader("Access-Control-Allow-Headers", "Content-Type");
  request->send(res);
}

void setup()
{
  Serial.begin(115200);
  u8g2.begin();
  pinMode(LED_PIN, OUTPUT);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  IPAddress ip = WiFi.localIP();
  char ipChar[16];                                            // enough for "255.255.255.255"
  sprintf(ipChar, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); // load ipchar with information
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(0, 10, "WildArt!");
  u8g2.sendBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 25, "http://");
  u8g2.drawStr(42, 25, (ipChar));
  u8g2.sendBuffer();
  // CORS preflight
  server.on("/api/led", HTTP_OPTIONS, [](AsyncWebServerRequest *req)
            { req->send(204); });

  // GET /api/status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    StaticJsonDocument<256> json;
    json["status"] = "ok";
    json["ip"] = WiFi.localIP().toString();
    json["led_on"] = digitalRead(LED_PIN) == LOW; // ESP8266 LED is inverted
    sendJson(request, json); });

  // POST /api/led  { "on": true }
  server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              // Handler for when request completes
            },
            nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    StaticJsonDocument<128> body;
    DeserializationError err = deserializeJson(body, data, len);

    StaticJsonDocument<128> res;
    if (err)
    {
      res["error"] = err.c_str();
      sendJson(request, res, 400);
      return;
    }

    bool on = body["on"] | false;
    // LED on ESP8266 is inverted: LOW = ON
    digitalWrite(LED_PIN, on ? LOW : HIGH);

    res["ok"] = true;
    res["led_on"] = on;
    sendJson(request, res); });

  server.begin();
}

void loop()
{
  if (toggle)
  {
    u8g2.drawStr(120, 10, "*");
    toggle = false;
  }
  else
  {
    clearTextLine(120, 10, 10);
    toggle = true;
  }
  u8g2.sendBuffer();
}
