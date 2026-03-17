
#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_SW_I2C
u8g2(U8G2_R0, /*clock=*/12, /*data=*/14, U8X8_PIN_NONE);

#define DHTPIN 2      // GPIO2 = D4 on NodeMCU/Wemos
#define DHTTYPE DHT11 // Use DHT22 here if needed

DHT dht(DHTPIN, DHTTYPE);

const char *WIFI_SSID = "WildNetz";
const char *WIFI_PASS = "owgtb123";

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
// Last sensor values for API responses (updated each loop)
float last_vcc_v = 0.0f;
float last_t = 0.0f;
float last_h = 0.0f;
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
// --- Smoothing & calibration ---
const size_t SAMPLES = 20; // number of samples for moving average
const uint16_t SAMPLE_DELAY_MS = 25;
float calibration_gain = 1.000f;   // multiply to adjust scale
int16_t calibration_offset_mv = 0; // add/subtract to adjust offset
ADC_MODE(ADC_VCC);                 // Configure ADC to measure Vcc

uint32_t readVccRawMv()
{
  // ESP.getVcc() returns millivolts (requires ADC_MODE(ADC_VCC))
  return ESP.getVcc();
}

uint32_t readVccSmoothedMv()
{
  uint32_t acc = 0;
  for (size_t i = 0; i < SAMPLES; i++)
  {
    acc += readVccRawMv();
    delay(SAMPLE_DELAY_MS);
  }
  uint32_t avg = acc / SAMPLES;
  // Apply calibration (gain then offset)
  float scaled = (float)avg * calibration_gain + (float)calibration_offset_mv;
  return (uint32_t)roundf(scaled);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting u8g2");
  u8g2.begin();
  Serial.println("Starting DHT");
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Turn off LED (ESP8266 built-in LED is inverted)

  // Optional: set calibration if you have a reference meter
  calibration_gain = 1.000f;
  calibration_offset_mv = 302;

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("Connecting to WiFi...");
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
  u8g2.drawStr(0, 22, "http://");
  u8g2.drawStr(42, 22, (ipChar));
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.sendBuffer();

  // Register HTTP API routes once
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument json;
    json["status"] = "ok";
    json["ip"] = WiFi.localIP().toString();
    json["vcc_voltage"] = last_vcc_v;
    json["temperature"] = last_t;
    json["humidity"] = last_h;
    sendJson(request, json); });
  server.begin();
}

void loop()
{
  // Read Vcc
  // Read smoothed Vcc
  uint32_t vcc_mv = readVccSmoothedMv();
  float vcc_v = vcc_mv / 1000.0f;
  String msg = String(int(100 * (vcc_v / 3.3f))) + "%";
  u8g2.drawStr(100, 10, msg.c_str());

  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Celsius
  t = t - 2.7;                     // calibration offset
  if (isnan(h) || isnan(t))
  {
    Serial.println("Failed to read from DHT11 sensor!");
    return;
  }
  else
  {
    u8g2.drawStr(0, 40, "Temp:");
    u8g2.drawStr(36, 40, String(t).c_str());
    u8g2.drawStr(80, 40, "°C");
    u8g2.drawStr(0, 55, "Hum:");
    u8g2.drawStr(36, 55, String(h).c_str());
    u8g2.drawStr(80, 55, "%");
    u8g2.sendBuffer();
  }
  // Print to serial
  Serial.printf("Vcc: %u mV (%.3f V)\n", vcc_mv, vcc_v);
  Serial.printf("Temperature: %.1f °C | Humidity: %.1f %%\n", t, h);
  // update latest sensor values for API handler
  last_vcc_v = vcc_v;
  last_t = t;
  last_h = h;
  delay(1500);
}
