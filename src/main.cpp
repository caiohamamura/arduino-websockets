#include <SocketIOClient.h>
#include <Regexp.h>
#include <ArduinoJson.h>
#include <string.h>
#include <NTPClient.h>
#include <WiFiUdp.h> //Socket UDP
#include <math.h>

// Constants
const char *WIFI_SSID = "Glaucia";
const char *WIFI_PASSWORD = "2tatubolinha";
const int TIME_ZONE = -3;
const char *NTP_SERVER_URL = "0.br.pool.ntp.org";
const int NTP_INTERVAL = 60000;
const char *MESSAGE_PATTERN = ".*gpio([0-9]+) (-?[0-9]+)";
const char *PATTERN_LAST_TWO = "([^ ]+) ([0-9]+)$";

const int LED_PIN = D8;
// Variables
bool autoLED = true;
int hourBegin = 12;
int hourEnd = 18;
int ledPwm = -1;
WiFiUDP udp;
NTPClient ntpClient(udp, NTP_SERVER_URL, TIME_ZONE * 3600, NTP_INTERVAL);
WebSocketsClient client;

void processMessage(char *message)
{
  MatchState ms(message);
  ms.Match(MESSAGE_PATTERN);
  char cap[10];

  if (ms.level > 1)
  {
    ms.GetCapture(cap, 0);
    int matchedGpioNumber = atoi(cap);
    Serial.printf("Matched gpio: %d\n", matchedGpioNumber);

    ms.GetCapture(cap, 1);
    int value = atoi(cap);
    Serial.printf("Matched value: %d\n", value);

    if (value >= 0)
    {
      if (matchedGpioNumber == LED_PIN)
      {
        autoLED = false;
        ledPwm = value;
      }
      analogWrite(matchedGpioNumber, value);
    }
    else if (matchedGpioNumber == LED_PIN)
    {
      autoLED = true;
    }
  }
  else
  {
    ms.Match(PATTERN_LAST_TWO);
    if (ms.level > 1)
    {
      Serial.println("Outro padrao: " + String(message));
      ms.GetCapture(cap, 0);
      if (strcmp(cap, "hourBegin") == 0)
      {
        ms.GetCapture(cap, 1);
        int hour = atoi(cap);
        if (hour >= 0 && hour < 24)
        {
          hourBegin = hour;
          Serial.printf("New hour begin: %d\n", hourBegin);
        }
      }
      else if (strcmp(cap, "hourEnd") == 0)
      {
        ms.GetCapture(cap, 1);
        int hour = atoi(cap);
        if (hour > 0 && hour <= 24)
        {
          hourEnd = hour;
          Serial.printf("New hour end: %d\n", hourEnd);
        }
      }
    }
  }
}

void websocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
  {
    Serial.printf("[WSc] Disconnected!\n");
  }
  break;
  case WStype_PING:
  {
    Serial.printf("[WSc] Ping\n");
  }
  break;
  case WStype_PONG:
  {
    Serial.printf("[WSc] Pong\n");
  }
  break;
  case WStype_TEXT:
  {
    Serial.printf("Message: %s\n", payload);
    processMessage((char *)payload);
  }
  break;
  case WStype_CONNECTED:
  {
    Serial.printf("[WSc] Connected to url: %s\n", payload);
    const char *output = "name=grande";
    client.sendTXT(output);
    Serial.println(output);
  }
  break;
  default:
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  analogWriteRange(1023);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  client.setExtraHeaders("Cookie:key=d12860a0-1a82-40ef-bbe2-1de82ee60875");
  client.onEvent(websocketEvent); // Listen for messages
  client.begin("192.168.0.119", 80, "/");
  client.enableHeartbeat(15000, 3000, 2);

  // Inicializa o client NTP
  ntpClient.begin();

  // Espera pelo primeiro update online
  Serial.println("Waiting for first update");
  while (!ntpClient.update())
  {
    Serial.print(".");
    ntpClient.forceUpdate();
    delay(500);
  }

  Serial.println();
  Serial.println("First Update Complete");
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
    }
  }

  client.loop();

  if (autoLED)
  {
    int value = ledPwm;
    if (ntpClient.getHours() > hourBegin && ntpClient.getHours() < (hourEnd - 1))
    {
      value = 0;
    }
    else if (ntpClient.getHours() == hourBegin)
    {
      value = (int)round(1023 - (1023 * ((ntpClient.getMinutes() * 60) + ntpClient.getSeconds()) / 3600.0));
    }
    else if (ntpClient.getHours() == (hourEnd - 1))
    {
      value = (int)round(1023 * ((ntpClient.getMinutes() * 60) + ntpClient.getSeconds()) / 3600.0);
    }
    else
    {
      value = 1023;
    }
    if (ledPwm != value)
    {
      Serial.printf("Changing value from: %d => %d\n", ledPwm, value);
      ledPwm = value;
      analogWrite(LED_PIN, ledPwm);
    }
  }

  delay(1);
}
