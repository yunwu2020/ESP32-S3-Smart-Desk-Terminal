#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ================= ⚙️ 用户配置区 (User Configuration) =================

// 1. 填入你的 DeepSeek API Key
const char* API_KEY = "sk-xxxxxxxxxxxxxxxxxxxxxxxx"; 

// 2. 填入你所在城市的经纬度 (纯数字，不要带字母或符号！)
const float LATITUDE = 39.90;   // 默认：北京纬度
const float LONGITUDE = 116.40; // 默认：北京经度

// ======================================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
U8g2_for_TFT_eSPI u8f; 
WebServer server(80);
Preferences prefs; 

int todayCode = 0, tomrCode = 0;
float todayMax = 0, todayMin = 0, tomrMax = 0, tomrMin = 0;
String todayDesc = "晴", tomrDesc = "晴";
unsigned long lastWeatherUpdate = 0;

String aiGreeting = "喵呜~ 正在唤醒猫娘记忆..."; 
time_t lastAIUpdateTime = 0;     
time_t lastHeartbeatTime = 0;    
int lastHolidayDate = 0; 
int lastSavedMinute = -1; 

String htmlPage = ""; 

// 🔥 动态生成天气API链接（将浮点数转为字符串，自动处理精度）
String getWeatherUrl() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(LATITUDE, 4); // 保留4位小数，足够精准
  url += "&longitude=";
  url += String(LONGITUDE, 4);
  url += "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=Asia/Shanghai&forecast_days=3";
  return url;
}

// ================= 工具函数 =================
bool isHoliday(int month, int day) {
  if (month == 1 && day == 1) return true;
  if (month == 2 && day == 14) return true;
  if (month == 5 && day == 1) return true;
  if (month == 6 && day == 1) return true;
  if (month == 10 && day == 1) return true;
  if (month == 12 && day == 25) return true;
  return false;
}

String getWeatherDesc(int code) {
  if (code == 0) return "晴"; 
  if (code == 1 || code == 2) return "多云"; 
  if (code == 3) return "阴";
  if (code >= 45 && code <= 48) return "雾"; 
  if (code >= 51 && code <= 57) return "毛毛雨";
  if (code >= 61 && code <= 67) return "雨"; 
  if (code >= 71 && code <= 77) return "雪";
  if (code >= 80 && code <= 82) return "阵雨"; 
  if (code >= 95 && code <= 99) return "雷阵雨";
  return "未知";
}

void drawWeatherIcon(TFT_eSprite &s, int code, int x, int y, uint16_t color) {
  if (code == 0) { 
    s.fillCircle(x, y, 5, color);
    for(int i=0; i<8; i++) s.drawLine(x + cos(i*45*PI/180)*7, y + sin(i*45*PI/180)*7, x + cos(i*45*PI/180)*10, y + sin(i*45*PI/180)*10, color);
  } else if (code >= 1 && code <= 3) { 
    s.fillCircle(x-5, y+2, 4, color); s.fillCircle(x, y-2, 6, color); s.fillCircle(x+5, y+2, 4, color); s.fillRect(x-9, y+2, 18, 4, color);
  } else { 
    s.fillCircle(x-5, y-2, 4, TFT_LIGHTGREY); s.fillCircle(x, y-6, 6, TFT_LIGHTGREY); s.fillCircle(x+5, y-2, 4, TFT_LIGHTGREY);
    s.drawLine(x-4, y+4, x-6, y+9, TFT_BLUE); s.drawLine(x, y+4, x-2, y+9, TFT_BLUE); s.drawLine(x+4, y+4, x+2, y+9, TFT_BLUE);
  }
}

// ================= 网络请求模块 =================
void fetchAIMessage(int month, int day) {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; 
  client.setInsecure(); 
  HTTPClient http;
  http.begin(client, "https://api.deepseek.com/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(API_KEY)); 
  http.setTimeout(15000);

  String prompt = "你是一个软萌的猫娘。今天是公历 " + String(month) + " 月 " + String(day) + " 日。";
  prompt += "第一，如果是中国传统节日或节气，用30字以内结合习俗提醒。";
  prompt += "第二，如果不是特殊节日，用30字以内根据今天天气（" + todayDesc + "，" + String(todayMin) + "-" + String(todayMax) + "度）提醒。";
  prompt += "要求：语气可爱，结尾带个‘喵’。只输出这句话。";

  DynamicJsonDocument reqDoc(1024);
  reqDoc["model"] = "deepseek-chat";
  reqDoc["messages"][0]["role"] = "user";
  reqDoc["messages"][0]["content"] = prompt;
  reqDoc["max_tokens"] = 60; 
  reqDoc["temperature"] = 0.7;

  String payload; serializeJson(reqDoc, payload);
  
  if (http.POST(payload) == HTTP_CODE_OK) {
    String response = http.getString();
    DynamicJsonDocument doc(4096); 
    if (!deserializeJson(doc, response)) {
      aiGreeting = doc["choices"][0]["message"]["content"].as<String>();
      lastAIUpdateTime = time(NULL); 
      
      prefs.begin("clockdata", false);
      prefs.putString("ai_msg", aiGreeting);
      prefs.putUInt("ai_time", lastAIUpdateTime); 
      prefs.end();
    }
  }
  http.end();
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; 
  client.setInsecure(); 
  HTTPClient http; 
  http.begin(client, getWeatherUrl()); 
  http.setTimeout(5000); 
  
  if (http.GET() == HTTP_CODE_OK) {
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, http.getString())) {
      todayCode = doc["daily"]["weather_code"][0]; todayMax = doc["daily"]["temperature_2m_max"][0]; todayMin = doc["daily"]["temperature_2m_min"][0]; todayDesc = getWeatherDesc(todayCode);
      tomrCode = doc["daily"]["weather_code"][1]; tomrMax = doc["daily"]["temperature_2m_max"][1]; tomrMin = doc["daily"]["temperature_2m_min"][1]; tomrDesc = getWeatherDesc(tomrCode);
    }
  }
  http.end(); 
}

// ================= Web 服务器模块 =================
void buildWebPage() {
  char aiTimeStr[30] = "无记录";
  if (lastAIUpdateTime > 0) {
    struct tm * ptm = localtime(&lastAIUpdateTime);
    strftime(aiTimeStr, sizeof(aiTimeStr), "%m-%d %H:%M:%S", ptm);
  }
  char hbTimeStr[30] = "无记录";
  if (lastHeartbeatTime > 0) {
    struct tm * ptm = localtime(&lastHeartbeatTime);
    strftime(hbTimeStr, sizeof(hbTimeStr), "%m-%d %H:%M:%S", ptm);
  }

  htmlPage = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>赛博时钟控制台</title>
    <style>
        body { background-color: #121212; color: white; text-align: center; padding-top: 30px; font-family: sans-serif;}
        .card { background: #1e1e1e; width: 85%; max-width: 400px; margin: 0 auto; padding: 20px; border-radius: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
        h1 { font-size: 22px; color: #00e5ff; }
        .log-box { background: #000; padding: 15px; border-radius: 8px; text-align: left; font-size: 13px; line-height: 1.8; margin-bottom: 20px; border: 1px solid #333;}
        .highlight { color: #FF9800; font-weight: bold; }
        button { background-color: #00e5ff; color: #000; padding: 12px 25px; font-size: 16px; border-radius: 8px; border: none; font-weight: bold; cursor: pointer; }
    </style>
</head>
<body>
    <div class="card">
        <h1>赛博日报控制台</h1>
        <div class="log-box">
            <div>最后心跳: <span class="highlight">)=====" + String(hbTimeStr) + R"=====(</span></div>
            <div>最后AI同步: <span class="highlight">)=====" + String(aiTimeStr) + R"=====(</span></div>
        </div>
        <button onclick="syncTime()">同步手机时间</button>
        <p id="status" style="margin-top:15px; color:#00e5ff;"></p>
    </div>
    <script>
        function syncTime() {
            fetch('/setTime?ts=' + new Date().getTime())
                .then(res => res.text())
                .then(data => { document.getElementById('status').innerText = data; });
        }
    </script>
</body>
</html>
)=====";
}

void handleRoot() { buildWebPage(); server.send(200, "text/html", htmlPage); }

void handleSetTime() {
  if (server.hasArg("ts")) {
    double epoch_ms = server.arg("ts").toDouble();
    if (epoch_ms > 1577836800000.0) { 
      time_t epoch_s = (time_t)(epoch_ms / 1000.0); 
      timeval tv = {0}; tv.tv_sec = epoch_s; tv.tv_usec = 0;
      settimeofday(&tv, NULL); setenv("TZ", "CST-8", 1); tzset();
      server.send(200, "text/plain", "校准成功"); return;
    }
  }
  server.send(400, "text/plain", "参数错误");
}

// ================= 初始化与主循环 =================
void setup() {
  Serial.begin(115200);
  
  prefs.begin("clockdata", true); 
  aiGreeting = prefs.getString("ai_msg", "等待时间同步");
  lastAIUpdateTime = prefs.getUInt("ai_time", 0);
  lastHeartbeatTime = prefs.getUInt("hb_time", 0);
  lastHolidayDate = prefs.getUInt("holiday_date", 0);
  prefs.end();
  
  tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK); 
  sprite.createSprite(160, 128); sprite.setTextDatum(MC_DATUM); 
  u8f.begin(sprite); u8f.setFontMode(1); u8f.setFontDirection(0); u8f.setForegroundColor(TFT_WHITE); u8f.setFont(u8g2_font_wqy12_t_gb2312); 

  tft.setTextColor(TFT_WHITE); tft.drawString("正在连接WiFi...", 80, 64, 2);
  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-S3-Clock")) ESP.restart();

  server.on("/", handleRoot); 
  server.on("/setTime", handleSetTime); 
  server.begin();
  
  timeval tv = {0}; settimeofday(&tv, NULL); 
  configTime(8 * 3600, 0, "ntp.aliyun.com", "time.pool.aliyun.com");
}

int last_sec = -1;
unsigned long ui_start_time = 0; 
bool show_icons = true;          
int scroll_x = 160;

void loop() {
  server.handleClient(); 

  if (millis() - lastWeatherUpdate > 1800000 || lastWeatherUpdate == 0) {
    fetchWeather(); 
    lastWeatherUpdate = millis();
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo) && timeinfo.tm_year > 120) {
    
    time_t now = time(NULL); 
    int todayDateCode = (timeinfo.tm_mon + 1) * 100 + timeinfo.tm_mday; 

    if (timeinfo.tm_min != lastSavedMinute) {
      lastSavedMinute = timeinfo.tm_min;
      lastHeartbeatTime = now;
      prefs.begin("clockdata", false);
      prefs.putUInt("hb_time", lastHeartbeatTime); 
      prefs.end();
    }

    bool needAIRequest = false;
    if (lastAIUpdateTime == 0 || (now - lastAIUpdateTime > 43200)) needAIRequest = true;
    if (isHoliday(timeinfo.tm_mon + 1, timeinfo.tm_mday) && lastHolidayDate != todayDateCode) {
      needAIRequest = true;
      lastHolidayDate = todayDateCode; 
      prefs.begin("clockdata", false);
      prefs.putUInt("holiday_date", lastHolidayDate);
      prefs.end();
    }
    if (needAIRequest) fetchAIMessage(timeinfo.tm_mon + 1, timeinfo.tm_mday);

    if (ui_start_time == 0) ui_start_time = millis(); 
    if (millis() - ui_start_time > 60000) {
      show_icons = !show_icons; scroll_x = 160; ui_start_time = millis(); 
    }

    if (timeinfo.tm_sec != last_sec || !show_icons) {
      last_sec = timeinfo.tm_sec;
      char hhmm[10]; strftime(hhmm, sizeof(hhmm), "%H:%M", &timeinfo);
      char date[20]; strftime(date, sizeof(date), "%Y-%m-%d", &timeinfo);

      sprite.fillRect(100, 0, 60, 128, 0x18E3); 
      sprite.drawFastVLine(100, 0, 128, TFT_DARKGREY);

      u8f.setForegroundColor(TFT_GOLD); u8f.setCursor(106, 12); u8f.print("今日");
      sprite.setTextColor(TFT_WHITE, TFT_BLACK); char tempStr[15];
      sprintf(tempStr, "%.0f-%.0f", todayMin, todayMax); sprite.drawString(tempStr, 130, 28, 2); 

      u8f.setForegroundColor(TFT_CYAN); u8f.setCursor(106, 76); u8f.print("明日");
      sprintf(tempStr, "%.0f-%.0f", tomrMin, tomrMax); sprite.drawString(tempStr, 130, 92, 2);

      if (show_icons) {
        drawWeatherIcon(sprite, todayCode, 130, 50, TFT_GOLD); 
        drawWeatherIcon(sprite, tomrCode, 130, 114, TFT_CYAN); 
      } else {
        u8f.setForegroundColor(TFT_GOLD);
        u8f.setCursor(scroll_x, 52); 
        u8f.print(aiGreeting); 

        scroll_x -= 2; 
        int textWidth = u8f.getUTF8Width(aiGreeting.c_str()); 
        if (scroll_x < -(textWidth)) scroll_x = 160; 
      }

      sprite.fillRect(0, 0, 100, 128, TFT_BLACK); 
      sprite.setTextColor(TFT_WHITE, TFT_BLACK); sprite.drawString(hhmm, 50, 40, 4); 
      sprite.setTextColor(TFT_CYAN, TFT_BLACK); sprite.drawString(date, 50, 90, 2);

      sprite.pushSprite(0, 0); 
    }
  }
  delay(30); 
}
