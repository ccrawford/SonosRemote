#include <Arduino.h>
#include <LittleFS.h>
#include "TFT_eSPI.h"
#include "pin_config.h"
#include <TJpg_Decoder.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <OneButton.h>
#include "Secrets.h"
#include <RestClient.h>
#include <ArduinoJson.h>

#define DISABLE_ALL_LIBRARY_WARNINGS

#define CONFIG_TFT_eSPI_ESPIDF
#undef CONFIG_ENABLE_TOUCH

// Fonts have to be uploaded in the data directory.
#define AA_FONT_SMALL "NotoSansBold15"
#define AA_FONT_SMALL_PLAIN "Calibri-15"

// #define MQTT_SOCKET_TIMEOUT 10

#define DBG_INFO Serial.printf

TFT_eSPI tft = TFT_eSPI();

WiFiClient(espClient);
PubSubClient mqClient(espClient);

OneButton button1(PIN_BUTTON_1, true, true);
OneButton button2(PIN_BUTTON_2, true, true);

RestClient restClient = RestClient(haServer, 8123);

int16_t drawBoxedString(const char *string, int32_t x, int32_t y, int32_t x2, int32_t y2);

char curTrack[101];
char curArtist[101];
char curAlbum[101];
char curCoverArt[256];
char curPlayerState[101];

String Cover_Art_File_Name = "/cover.jpg";

#if defined(LCD_MODULE_CMD_1)
typedef struct {
  uint8_t cmd;
  uint8_t data[14];
  uint8_t len;
} lcd_cmd_t;

lcd_cmd_t lcd_st7789v[] = {
    {0x11, {0}, 0 | 0x80},
    // {0x3A, {0X06}, 1},
    {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
    {0xB7, {0X75}, 1},
    {0xBB, {0X28}, 1},
    {0xC0, {0X2C}, 1},
    {0xC2, {0X01}, 1},
    {0xC3, {0X1F}, 1},
    {0xC6, {0X13}, 1},
    {0xD0, {0XA7}, 1},
    {0xD0, {0XA4, 0XA1}, 2},
    {0xD6, {0XA1}, 1},
    {0xE0, {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32}, 14},
    {0xE1, {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37}, 14},
};
#endif


bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}

void reconnect() {

  // Make sure network is up
  Serial.printf("Wifi Status: %d", WiFi.status());
  /* Value  Constant  Meaning
0 WL_IDLE_STATUS  temporary status assigned when WiFi.begin() is called
1 WL_NO_SSID_AVAIL   when no SSID are available
2 WL_SCAN_COMPLETED scan networks is completed
3 WL_CONNECTED  when connected to a WiFi network
4 WL_CONNECT_FAILED when the connection fails for all the attempts
5 WL_CONNECTION_LOST  when the connection is lost
6 WL_DISCONNECTED when disconnected from a network
*/
  // Loop until we're reconnected to the MQTT broker.


// All of these media_player calls are unique to my environment. 
// "sonos5" is the name of the sonos speaker in my office. Needs to be configurable. 
  
  
  while (!mqClient.connected()) {
    Serial.printf("Attempting MQTT connection...");
    
    if (mqClient.connect("MusicMonitor","hass.mqtt",mqttPassword,0,0,0,0,0)) {
      Serial.printf("MQTT connected\n");
      mqClient.subscribe("media_player/sonos5/track_data");
    } else {
      Serial.printf("failed to connect to MQTT, Try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// void executeCallback() {
//     if (drawingCallback != nullptr) {
//       (*drawingCallback)();
//     }
// }

void displayCover()
{
  Serial.println("Show the cover");
  TJpgDec.setJpgScale(4);
  if(SPIFFS.exists(Cover_Art_File_Name))
  {
    Serial.println("File is there.");      
    TJpgDec.drawFsJpg(0,0, Cover_Art_File_Name);
  }
  else Serial.println("No cover to show.");
  Serial.println("Jpg drawn.");
}

bool downloadFile(String url, String filename)
{
  bool success = false;
  Serial.println("Downloading " + url + " and saving as " + filename);
  HTTPClient httpClient;
  httpClient.begin(url);

  int httpCode = httpClient.GET();
    if(httpCode > 0) {
        //SPIFFS.remove(filename);
        fs::File f = SPIFFS.open(filename, "w+");
        if (!f) {
            Serial.println("file open failed");
            return false;
        }
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) {

            // get lenght of document (is -1 when Server sends no Content-Length header)
            int total = httpClient.getSize();
            int len = total;
            //progressCallback(filename, 0,total, true);
            // create buffer for read
            uint8_t buff[128] = { 0 };

            // get tcp stream
            WiFiClient * stream = httpClient.getStreamPtr();

            // read all data from server
            while(httpClient.connected() && (len > 0 || len == -1)) {
                // get available data size
                size_t size = stream->available();

                if(size) {
                    // read up to 128 byte
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                    // write it to Serial
                    f.write(buff, c);

                    if(len > 0) {
                        len -= c;
                    }
                    //progressCallback(filename, total - len,total, false);
                    // isFirstCall = false;
                    // executeCallback();
                }
                delay(1);
            }

            Serial.println();
            Serial.print("[HTTP] connection closed or file end.\n");
            success = true;
        }
        f.close();
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
        httpClient.end();
        return false;
    }
    
    httpClient.end();

    Serial.println("Done downloading...now show cover");

    displayCover(); //Lazy...should designate a callback in signiture.
    
    return true;
  
}

void updateScreen()
{

  static char lastCoverArt[256];

  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
  tft.fillScreen(TFT_BLACK);
  // tft.drawString(curTrack, 165, 0);
  //Clear right half of screen.
  tft.drawRect(160,0, 320, 170, TFT_BLACK);
  
  int16_t curY = drawBoxedString(curTrack, 165, 0, 320, 60);


  tft.setTextColor(tft.color565(200,200,200),TFT_BLACK, true);
  tft.unloadFont();
  tft.loadFont(AA_FONT_SMALL_PLAIN);
  tft.drawString(curArtist, 165, curY + 25);
  tft.drawString(curAlbum, 165, curY + 40);

  if(!strncmp(lastCoverArt, curCoverArt, 255))
  {
    Serial.println("Same cover...skip update.");
    return;
  }

  strncpy(lastCoverArt, curCoverArt, 255);
  DBG_INFO("Covers are different\n");
  //Clear left half of screen.
  tft.drawRect(0,0, 160,160, tft.color565(20,20,20));

  String url("http://192.168.1.207:8123");
  url += String(curCoverArt);
  if (!downloadFile(url, Cover_Art_File_Name))
  {
    strncpy(lastCoverArt, "FAILED",6);
  }

  Serial.println("Done with download.");
  
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  
  static char lastTrack[255];
  
  Serial.print("MQTT Topic: "); Serial.println(topic); 
  // Serial.print("MQTT Payload: ");  
  // for (int i = 0; i<length; i++) Serial.print((char)payload[i]);
  // Serial.println();
 
  if(!strcmp(topic, "media_player/sonos5/track_data")) {
    
    char trackInfo[length*2];
    static char lastId[512];

    for(int i = 0; i<length; i++) trackInfo[i] = payload[i];
    trackInfo[length] = '\0';

    StaticJsonDocument<96> doc;

    Serial.printf("Media meta payload: %s length: %d\n", trackInfo, length);

    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    const char* id = doc["id"];
    const char* track = doc["track"]; // "IF YOU CAN'T ROCK ME"
    const char* album = doc["album"]; // nullptr
    const char* artist = doc["artist"]; // "ROLLING STONES"
    const char* cover_art = doc["cover_art"];

    if(!strncmp(lastTrack, track, 100))
    { 
      Serial.println("Got track metadata but no change.");
      return;
    }

    strncpy(lastTrack, track, 100);

    strncpy(curTrack, track, 100);
    strncpy(curAlbum, album, 100);
    strncpy(curArtist, artist, 100);
    strncpy(curCoverArt, cover_art, 255);

    Serial.println("Cur track info updated");

    updateScreen();

  }  
  
/*
  if(!strcmp(topic, "homeassistant/media_player/volume")) {
    char bufVol[6];
    strncpy(bufVol, (char *)payload, length);
    bufVol[length] = 0;
    int vol = atof(bufVol)*100;
    DBG_INFO("Volume: %d\n", vol);
    // myNex.writeNum("j1.val", vol);
  }
*/

  if(!strcmp(topic, "homeassistant/media_player/state")) {
    char bufState[20];
    strncpy(bufState, (char *)payload, length);
    bufState[length]=0;
    strcpy(curPlayerState, bufState);
    DBG_INFO("State: %s\n", curPlayerState);
  }
  
}


void mediaControl(char* command) {

  // See: https://www.home-assistant.io/integrations/media_player
  
  char service[100];
  sprintf(service, "/api/services/media_player/%s", command);
  DBG_INFO("Call: %s",service);
  restClient.setHeader(HA_TOKEN);
  restClient.post(service, "{\"entity_id\":\"media_player.sonos_5\"}");
  
}

void togglePause()
{
  char cmd[] = "media_play_pause";
  mediaControl(cmd);  
}
void nextTrack()
{
  char cmd[] = "media_next_track";
  mediaControl(cmd);
}



int16_t drawBoxedString(const char *string, int32_t x, int32_t y, int32_t x2, int32_t y2)
{
  tft.setTextWrap(false, false);
  
  uint16_t cheight = tft.fontHeight();
  
  uint16_t curX = x, curY = y;
  char c[2];

  for(int i=0; i<strlen(string); i++)
  {
    c[0] = string[i];
    c[1] = '\0';
    if (tft.textWidth(c) + curX > x2) 
    {
      curX = x;
      curY += cheight;
    }
    curX += tft.drawString(c, curX, curY);
  }
  return curY;
}


void getNtpTime()
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);  // Chicago time zone via: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  tzset();

  DBG_INFO("Waiting for NTP time sync: ");
  time_t now = time(NULL);
  while (now < 8 * 3600 * 2) {
    delay(500);
    DBG_INFO(".");
    now = time(NULL);
  }
  now = time(NULL);
  struct tm tm;
  getLocalTime(&tm);
  DBG_INFO("Current time %s\n", asctime(&tm));
}


void setup() {

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  Serial.begin(115200);
  Serial.println("Sonos/HA Display for use on Lilygo ESP32 TFT S3");

  tft.begin();

  WiFiManager wm;
  

#if defined(LCD_MODULE_CMD_1)
  for (uint8_t i = 0; i < (sizeof(lcd_st7789v) / sizeof(lcd_cmd_t)); i++) {
    tft.writecommand(lcd_st7789v[i].cmd);
    for (int j = 0; j < lcd_st7789v[i].len & 0x7f; j++) {
      tft.writedata(lcd_st7789v[i].data[j]);
    }

    if (lcd_st7789v[i].len & 0x80) {
      delay(120);
    }
  }
#endif

  button1.attachClick(togglePause);
  button1.attachDoubleClick(nextTrack);

  tft.setRotation(3);
  tft.setSwapBytes(true);

  ledcSetup(0, 2000, 8);
  ledcAttachPin(PIN_LCD_BL, 0);
  ledcWrite(0,255); //Brightness

  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);

  if (!SPIFFS.begin(true)) 
  {
    tft.drawString("SPIFFS initialisation failed!",0,0,1);
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nInitialisation done.");
  tft.drawString("Spiffs Initialized",0,0,1);

  bool font_missing = false;
  font_missing = !SPIFFS.exists("/NotoSansBold15.vlw") ||
                 !SPIFFS.exists("/Calibri-15.vlw");

  if (font_missing)
  {
    tft.setTextColor(TFT_RED,TFT_BLACK);
    tft.drawString("Fonts missing.",0,20,1);
    Serial.println("\r\nFont missing in SPIFFS, did you upload it?");
    while(1) yield();
  }
  else tft.drawString("\r\nFonts found OK.",0,10,1);
  Serial.println("Fonts ok.");
  

  if(!wm.autoConnect("MusicDisplayAP"))
  {
    Serial.println("Could not connect with WM");
  }
  else
  {
    Serial.println("Connected!");
    tft.drawString("Wifi Connected",0,20,1);
  }

// Not using time, but you never know. And if youre doing https you need it.
  getNtpTime();

//  IPAddress mqServer(192, 168, 1, 207); 
  mqClient.setBufferSize(1024);  // Default is 256 and our message is almost 512.
  mqClient.setServer(mqttServer, 1883);
  mqClient.setCallback(mqttCallback);

  TJpgDec.setCallback(tft_output);

  Serial.println("Done with setup. Pause screen a sec");
  
  delay(2000);

  Serial.print("Spiffs size: "); Serial.println(SPIFFS.totalBytes());
  Serial.print("Spiffs used: "); Serial.println(SPIFFS.usedBytes());
   
}

long lastMs = 0;
void loop() {

  if (!mqClient.connected()) {
    DBG_INFO("reconnecting...");
    reconnect();
    DBG_INFO("MQTT Reconnected.\n");
  }
  mqClient.loop();
  button1.tick();
  button2.tick();


  if(millis() > lastMs + 500)  // Used to slow the updates.
  { 
    if(strncmp(curPlayerState, "playing",16))
    {
      ledcWrite(0, 20);
    }
    else
    {
      ledcWrite(0, 255);
    }
 }

}