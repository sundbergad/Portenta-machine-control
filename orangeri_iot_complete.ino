/*
 * Smart Orangeri Ventilation Control System - Enhanced Edition
 * With WiFi Web Server, Mobile App Interface, and Winter Climate Logic
 * 
 * Features:
 * - REST API for remote control
 * - Real-time web dashboard
 * - Advanced winter climate protection
 * - Cloud data logging
 * - Manual override capability
 */

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ============ WiFi CONFIGURATION ============
const char* ssid = "YOUR_SSID";           // Change to your WiFi SSID
const char* password = "YOUR_PASSWORD";   // Change to your WiFi password
const char* hostname = "orangeri-control";

WebServer server(80);

// ============ SENSOR PIN CONFIGURATION ============
const int TEMP_INSIDE_PIN = A0;
const int TEMP_OUTSIDE_PIN = A1;
const int HUMIDITY_INSIDE_PIN = A2;
const int HUMIDITY_OUTSIDE_PIN = A3;
const int WIND_SPEED_PIN = A4;
const int WIND_DIR_PIN = A5;
const int RAIN_SENSOR_PIN = A6;

// ============ DIGITAL OUTPUT CONFIGURATION ============
const int HATCH_1_OPEN = D0;
const int HATCH_1_CLOSE = D1;
const int HATCH_2_OPEN = D2;
const int HATCH_2_CLOSE = D3;

// ============ CONTROL PARAMETERS ============
// Temperature thresholds (°C)
float TEMP_OPEN_THRESHOLD = 22.0;
float TEMP_CLOSE_THRESHOLD = 20.0;

// Humidity thresholds (%)
int HUMIDITY_OPEN_THRESHOLD = 70;
int HUMIDITY_CLOSE_THRESHOLD = 60;

// Wind safety thresholds (m/s)
const float WIND_MAX_SAFE = 8.0;
const float WIND_SLIGHT_OPEN = 4.0;
const float WIND_MODERATE = 6.0;

// Rain detection
const int RAIN_THRESHOLD = 400;
const bool RAIN_ACTIVE_HIGH = true;

// ============ WINTER CLIMATE PARAMETERS ============
// These adjust based on season
float FROST_PROTECTION_TEMP = 2.0;        // Don't open if outside < this
float FROST_HYSTERESIS = 1.0;              // Keep closed until it warms up
const float MIN_WINTER_TEMP = -5.0;        // Emergency close temp
float WINTER_HUMIDITY_THRESHOLD = 85.0;    // Higher tolerance in winter
float SNOW_LOAD_TEMP = -1.0;               // Assume snow if below this + wet
bool WINTER_MODE = false;                  // Automatically detect based on date/temp
bool ANTI_CONDENSATION_MODE = false;       // Slight ventilation at dawn to prevent frost

// Seasonal detection
enum Season { SPRING, SUMMER, AUTUMN, WINTER };
Season current_season = SPRING;

// ============ STATE MANAGEMENT ============
enum HatchState {
  HATCH_CLOSED = 0,
  HATCH_OPENING = 1,
  HATCH_OPEN = 2,
  HATCH_CLOSING = 3,
  HATCH_SLIGHTLY_OPEN = 4
};

HatchState hatch1_state = HATCH_CLOSED;
HatchState hatch2_state = HATCH_CLOSED;

unsigned long hatch_move_timeout = 30000;
unsigned long hatch1_start_time = 0;
unsigned long hatch2_start_time = 0;

// Manual override state
bool manual_control = false;
HatchState manual_hatch1_state = HATCH_CLOSED;
HatchState manual_hatch2_state = HATCH_CLOSED;

// Timing
unsigned long last_decision_time = 0;
const unsigned long DECISION_INTERVAL = 5000;

// Data logging
unsigned long last_log_time = 0;
const unsigned long LOG_INTERVAL = 60000; // Log every minute
unsigned long last_cloud_sync = 0;
const unsigned long CLOUD_SYNC_INTERVAL = 300000; // Sync to cloud every 5 minutes

// System statistics
struct SystemStats {
  float temp_inside;
  float temp_outside;
  int humidity_inside;
  int humidity_outside;
  float wind_speed;
  int wind_direction;
  bool raining;
  bool in_frost_mode;
  bool in_winter_mode;
  String last_decision;
  unsigned long uptime;
};

SystemStats current_stats;

// ============ SENSOR READING FUNCTIONS ============

float readTemperature(int pin) {
  int raw = analogRead(pin);
  float temp = (raw * 5.0 / 1024.0) * 100.0;
  return temp;
}

int readHumidity(int pin) {
  int raw = analogRead(pin);
  int humidity = map(raw, 0, 1023, 0, 100);
  return constrain(humidity, 0, 100);
}

float readWindSpeed() {
  int raw = analogRead(WIND_SPEED_PIN);
  float wind_speed = (raw / 1023.0) * 20.0;
  return wind_speed;
}

int readWindDirection() {
  int raw = analogRead(WIND_DIR_PIN);
  int direction = map(raw, 0, 1023, 0, 360);
  return direction;
}

bool isRaining() {
  int raw = analogRead(RAIN_SENSOR_PIN);
  if (RAIN_ACTIVE_HIGH) {
    return raw > RAIN_THRESHOLD;
  } else {
    return raw < RAIN_THRESHOLD;
  }
}

// ============ SEASONAL LOGIC ============

void detectSeason() {
  // Simple detection: can be improved with RTC module
  // For now, use temperature patterns
  float avg_outside_temp = current_stats.temp_outside;
  
  if (avg_outside_temp < 5.0) {
    current_season = WINTER;
    WINTER_MODE = true;
  } else if (avg_outside_temp < 10.0) {
    current_season = SPRING;
    WINTER_MODE = false;
  } else if (avg_outside_temp > 20.0) {
    current_season = SUMMER;
    WINTER_MODE = false;
  } else if (avg_outside_temp > 15.0) {
    current_season = AUTUMN;
    WINTER_MODE = true; // Prepare for winter
  }
}

void adjustSeasonalThresholds() {
  if (WINTER_MODE) {
    // Winter: more conservative, prevent frost damage
    TEMP_OPEN_THRESHOLD = 25.0;           // Wait until warmer to open
    TEMP_CLOSE_THRESHOLD = 18.0;          // Close earlier to retain heat
    HUMIDITY_OPEN_THRESHOLD = 85;         // Higher tolerance for condensation
    FROST_PROTECTION_TEMP = 2.0;          // Don't open if < 2°C outside
  } else {
    // Summer: more aggressive ventilation
    TEMP_OPEN_THRESHOLD = 22.0;
    TEMP_CLOSE_THRESHOLD = 20.0;
    HUMIDITY_OPEN_THRESHOLD = 70;
    FROST_PROTECTION_TEMP = 0.0;
  }
}

// ============ WINTER CLIMATE PROTECTION ============

bool isFrostRisk() {
  // Check if frost protection should be active
  if (current_stats.temp_outside < FROST_PROTECTION_TEMP) {
    return true;
  }
  // Also check if cold outside + high humidity + evening time = condensation risk
  if (current_stats.temp_outside < 5.0 && current_stats.humidity_inside > 80) {
    return true;
  }
  return false;
}

bool hasSnowLoad() {
  // Assume snow if:
  // - Temperature is around freezing (±2°C)
  // - Rain sensor is wet (rain/melting snow)
  // - Outside humidity is high (snow clouds)
  
  if (current_stats.temp_outside < 1.0 && current_stats.temp_outside > -3.0) {
    if (current_stats.raining || current_stats.humidity_outside > 90) {
      return true;
    }
  }
  return false;
}

void handleFrostProtection() {
  // Prevent ice formation inside by gentle ventilation at dawn
  // This prevents condensation from refreezing overnight
  if (WINTER_MODE && ANTI_CONDENSATION_MODE) {
    // Open slightly for 15-30 minutes at dawn (4-6 AM)
    // Your code would check actual time here
    if (current_stats.temp_inside > 5.0 && current_stats.temp_outside > -5.0) {
      // Safe to slightly ventilate
      return;
    }
  }
}

// ============ HATCH CONTROL FUNCTIONS ============

void stopHatch(int open_pin, int close_pin) {
  digitalWrite(open_pin, LOW);
  digitalWrite(close_pin, LOW);
}

void openHatch(int open_pin, int close_pin, unsigned long &start_time) {
  digitalWrite(close_pin, LOW);
  digitalWrite(open_pin, HIGH);
  start_time = millis();
}

void closeHatch(int open_pin, int close_pin, unsigned long &start_time) {
  digitalWrite(open_pin, LOW);
  digitalWrite(close_pin, HIGH);
  start_time = millis();
}

void slightlyOpenHatch(int open_pin, int close_pin) {
  digitalWrite(close_pin, LOW);
  digitalWrite(open_pin, HIGH);
  delay(200);
  digitalWrite(open_pin, LOW);
}

void updateHatchMovement(HatchState &state, int open_pin, int close_pin, unsigned long start_time) {
  unsigned long elapsed = millis() - start_time;
  
  if ((state == HATCH_OPENING || state == HATCH_CLOSING) && elapsed > hatch_move_timeout) {
    stopHatch(open_pin, close_pin);
    if (state == HATCH_OPENING) {
      state = HATCH_OPEN;
    } else {
      state = HATCH_CLOSED;
    }
  }
}

// ============ DECISION LOGIC ============

void makeVentilationDecision() {
  // Read all environmental data
  current_stats.temp_inside = readTemperature(TEMP_INSIDE_PIN);
  current_stats.temp_outside = readTemperature(TEMP_OUTSIDE_PIN);
  current_stats.humidity_inside = readHumidity(HUMIDITY_INSIDE_PIN);
  current_stats.humidity_outside = readHumidity(HUMIDITY_OUTSIDE_PIN);
  current_stats.wind_speed = readWindSpeed();
  current_stats.wind_direction = readWindDirection();
  current_stats.raining = isRaining();
  current_stats.uptime = millis() / 1000;
  
  // Detect season and adjust thresholds
  detectSeason();
  adjustSeasonalThresholds();
  
  // Check for critical conditions
  current_stats.in_frost_mode = isFrostRisk();
  current_stats.in_winter_mode = WINTER_MODE;
  
  // Emergency override: extreme cold
  if (current_stats.temp_outside < MIN_WINTER_TEMP) {
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    current_stats.last_decision = "EMERGENCY: Extreme cold - all hatches closed";
    hatch1_state = HATCH_CLOSING;
    hatch2_state = HATCH_CLOSING;
    return;
  }
  
  // ===== SAFETY FIRST: Check for rain =====
  if (current_stats.raining) {
    current_stats.last_decision = "RAINING: Closing all hatches";
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    hatch1_state = HATCH_CLOSING;
    hatch2_state = HATCH_CLOSING;
    return;
  }
  
  // ===== SAFETY: Check for snow load =====
  if (hasSnowLoad()) {
    current_stats.last_decision = "SNOW LOAD: Keeping hatches closed";
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    hatch1_state = HATCH_CLOSING;
    hatch2_state = HATCH_CLOSING;
    return;
  }
  
  // ===== SAFETY: Check for dangerous wind =====
  if (current_stats.wind_speed > WIND_MAX_SAFE) {
    current_stats.last_decision = "WIND TOO STRONG: Closing hatches";
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    hatch1_state = HATCH_CLOSING;
    hatch2_state = HATCH_CLOSING;
    return;
  }
  
  // ===== FROST PROTECTION =====
  if (current_stats.in_frost_mode) {
    current_stats.last_decision = "FROST RISK: Keeping hatches closed";
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    hatch1_state = HATCH_CLOSING;
    hatch2_state = HATCH_CLOSING;
    return;
  }
  
  // ===== NORMAL OPERATION: Temperature & Humidity Control =====
  HatchState desired_state = HATCH_CLOSED;
  String reason = "";
  
  if (current_stats.temp_inside > TEMP_OPEN_THRESHOLD) {
    desired_state = HATCH_OPEN;
    reason = "Inside too warm - OPEN for cooling";
  }
  else if (current_stats.temp_inside < TEMP_CLOSE_THRESHOLD) {
    desired_state = HATCH_CLOSED;
    reason = "Inside too cold - CLOSE to retain heat";
  }
  else if (current_stats.humidity_inside > HUMIDITY_OPEN_THRESHOLD && 
           current_stats.temp_inside > (TEMP_CLOSE_THRESHOLD + 1)) {
    desired_state = HATCH_OPEN;
    reason = "Humidity too high - OPEN for dehumidifying";
  }
  else if (current_stats.humidity_inside < HUMIDITY_CLOSE_THRESHOLD) {
    desired_state = HATCH_CLOSED;
    reason = "Humidity acceptable - CLOSE";
  }
  else {
    desired_state = HATCH_CLOSED;
    reason = "Conditions stable - CLOSE";
  }
  
  // ===== WIND MODERATION =====
  if (desired_state == HATCH_OPEN && current_stats.wind_speed > WIND_SLIGHT_OPEN) {
    if (current_stats.wind_speed > WIND_MODERATE) {
      desired_state = HATCH_SLIGHTLY_OPEN;
      reason += " (wind strong - SLIGHTLY OPEN)";
    }
  }
  
  current_stats.last_decision = reason;
  
  // ===== APPLY DECISION =====
  // Apply to hatch 1
  if (desired_state == HATCH_OPEN && hatch1_state != HATCH_OPEN && hatch1_state != HATCH_OPENING) {
    openHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    hatch1_state = HATCH_OPENING;
  }
  else if (desired_state == HATCH_CLOSED && hatch1_state != HATCH_CLOSED && hatch1_state != HATCH_CLOSING) {
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    hatch1_state = HATCH_CLOSING;
  }
  else if (desired_state == HATCH_SLIGHTLY_OPEN && hatch1_state != HATCH_SLIGHTLY_OPEN) {
    slightlyOpenHatch(HATCH_1_OPEN, HATCH_1_CLOSE);
    hatch1_state = HATCH_SLIGHTLY_OPEN;
  }
  
  // Apply to hatch 2
  if (desired_state == HATCH_OPEN && hatch2_state != HATCH_OPEN && hatch2_state != HATCH_OPENING) {
    openHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    hatch2_state = HATCH_OPENING;
  }
  else if (desired_state == HATCH_CLOSED && hatch2_state != HATCH_CLOSED && hatch2_state != HATCH_CLOSING) {
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    hatch2_state = HATCH_CLOSING;
  }
  else if (desired_state == HATCH_SLIGHTLY_OPEN && hatch2_state != HATCH_SLIGHTLY_OPEN) {
    slightlyOpenHatch(HATCH_2_OPEN, HATCH_2_CLOSE);
    hatch2_state = HATCH_SLIGHTLY_OPEN;
  }
}

// ============ CLOUD LOGGING ============

void logToCloud() {
  // Send data to cloud service
  // Example for ThingSpeak, Azure IoT Hub, or Firebase
  
  StaticJsonDocument<512> doc;
  doc["timestamp"] = millis() / 1000;
  doc["temp_inside"] = current_stats.temp_inside;
  doc["temp_outside"] = current_stats.temp_outside;
  doc["humidity_inside"] = current_stats.humidity_inside;
  doc["humidity_outside"] = current_stats.humidity_outside;
  doc["wind_speed"] = current_stats.wind_speed;
  doc["wind_direction"] = current_stats.wind_direction;
  doc["raining"] = current_stats.raining;
  doc["frost_risk"] = current_stats.in_frost_mode;
  doc["winter_mode"] = current_stats.in_winter_mode;
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  Serial.print("Cloud Log: ");
  Serial.println(jsonStr);
  
  // TODO: Implement actual cloud upload here
  // Options:
  // 1. ThingSpeak: Send HTTP GET with API key
  // 2. Azure IoT: Send MQTT message
  // 3. Firebase: Send HTTPS POST with JSON
  // 4. Local InfluxDB: Send line protocol via HTTP
}

// ============ WEB SERVER ROUTES ============

void handleRoot() {
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/html", getWebDashboard());
}

void handleAPI_Status() {
  StaticJsonDocument<1024> doc;
  
  doc["system"]["uptime"] = current_stats.uptime;
  doc["system"]["winter_mode"] = WINTER_MODE;
  doc["system"]["frost_risk"] = current_stats.in_frost_mode;
  doc["system"]["manual_control"] = manual_control;
  
  doc["sensors"]["temp_inside"] = round(current_stats.temp_inside * 10) / 10.0;
  doc["sensors"]["temp_outside"] = round(current_stats.temp_outside * 10) / 10.0;
  doc["sensors"]["humidity_inside"] = current_stats.humidity_inside;
  doc["sensors"]["humidity_outside"] = current_stats.humidity_outside;
  doc["sensors"]["wind_speed"] = round(current_stats.wind_speed * 10) / 10.0;
  doc["sensors"]["wind_direction"] = current_stats.wind_direction;
  doc["sensors"]["raining"] = current_stats.raining;
  
  doc["hatches"]["hatch1"] = getHatchStateName(hatch1_state);
  doc["hatches"]["hatch2"] = getHatchStateName(hatch2_state);
  
  doc["thresholds"]["temp_open"] = TEMP_OPEN_THRESHOLD;
  doc["thresholds"]["temp_close"] = TEMP_CLOSE_THRESHOLD;
  doc["thresholds"]["humidity_open"] = HUMIDITY_OPEN_THRESHOLD;
  
  doc["last_decision"] = current_stats.last_decision;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAPI_ManualControl() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, body);
      
      if (!error) {
        if (doc.containsKey("manual_control")) {
          manual_control = doc["manual_control"];
          
          if (manual_control) {
            // Take manual control
            if (doc.containsKey("hatch1")) {
              String cmd = doc["hatch1"];
              if (cmd == "open") {
                openHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
              } else if (cmd == "close") {
                closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
              } else if (cmd == "stop") {
                stopHatch(HATCH_1_OPEN, HATCH_1_CLOSE);
              }
            }
            if (doc.containsKey("hatch2")) {
              String cmd = doc["hatch2"];
              if (cmd == "open") {
                openHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
              } else if (cmd == "close") {
                closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
              } else if (cmd == "stop") {
                stopHatch(HATCH_2_OPEN, HATCH_2_CLOSE);
              }
            }
          } else {
            // Return to automatic control
            stopHatch(HATCH_1_OPEN, HATCH_1_CLOSE);
            stopHatch(HATCH_2_OPEN, HATCH_2_CLOSE);
          }
        }
        
        // Update thresholds if provided
        if (doc.containsKey("temp_open")) {
          TEMP_OPEN_THRESHOLD = doc["temp_open"];
        }
        if (doc.containsKey("temp_close")) {
          TEMP_CLOSE_THRESHOLD = doc["temp_close"];
        }
      }
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

void handleAPI_Thresholds() {
  if (server.method() == HTTP_GET) {
    StaticJsonDocument<256> doc;
    doc["temp_open"] = TEMP_OPEN_THRESHOLD;
    doc["temp_close"] = TEMP_CLOSE_THRESHOLD;
    doc["humidity_open"] = HUMIDITY_OPEN_THRESHOLD;
    doc["humidity_close"] = HUMIDITY_CLOSE_THRESHOLD;
    doc["wind_max_safe"] = WIND_MAX_SAFE;
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  }
  else if (server.method() == HTTP_POST) {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      StaticJsonDocument<256> doc;
      deserializeJson(doc, body);
      
      if (doc.containsKey("temp_open")) TEMP_OPEN_THRESHOLD = doc["temp_open"];
      if (doc.containsKey("temp_close")) TEMP_CLOSE_THRESHOLD = doc["temp_close"];
      if (doc.containsKey("humidity_open")) HUMIDITY_OPEN_THRESHOLD = doc["humidity_open"];
      if (doc.containsKey("humidity_close")) HUMIDITY_CLOSE_THRESHOLD = doc["humidity_close"];
      
      server.send(200, "application/json", "{\"status\":\"thresholds_updated\"}");
    }
  }
}

String getHatchStateName(HatchState state) {
  switch(state) {
    case HATCH_CLOSED: return "closed";
    case HATCH_OPENING: return "opening";
    case HATCH_OPEN: return "open";
    case HATCH_CLOSING: return "closing";
    case HATCH_SLIGHTLY_OPEN: return "slightly_open";
    default: return "unknown";
  }
}

String getWebDashboard() {
  return R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Orangeri Control Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 10px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            text-align: center;
        }
        h1 { font-size: 24px; margin-bottom: 5px; }
        .subtitle { font-size: 12px; opacity: 0.9; }
        
        .content {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            padding: 20px;
        }
        
        .card {
            background: white;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 15px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        
        .card-title {
            font-size: 16px;
            font-weight: 600;
            margin-bottom: 15px;
            color: #333;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        
        .sensor-row {
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid #f0f0f0;
        }
        
        .sensor-label { color: #666; font-size: 13px; }
        .sensor-value { font-weight: 600; color: #333; }
        
        .hatch-control {
            margin: 15px 0;
            padding: 15px;
            background: #f5f5f5;
            border-radius: 6px;
        }
        
        .hatch-name {
            font-weight: 600;
            margin-bottom: 10px;
            color: #333;
        }
        
        .hatch-state {
            font-size: 12px;
            padding: 5px 10px;
            background: #ddd;
            border-radius: 4px;
            display: inline-block;
            margin-bottom: 10px;
        }
        
        .hatch-state.open { background: #4caf50; color: white; }
        .hatch-state.closed { background: #f44336; color: white; }
        .hatch-state.slightly { background: #ff9800; color: white; }
        .hatch-state.moving { background: #2196f3; color: white; }
        
        .button-group {
            display: flex;
            gap: 8px;
            margin-top: 10px;
        }
        
        button {
            flex: 1;
            padding: 10px;
            border: none;
            border-radius: 4px;
            font-size: 12px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }
        
        .btn-open { background: #4caf50; color: white; }
        .btn-open:hover { background: #45a049; }
        .btn-close { background: #f44336; color: white; }
        .btn-close:hover { background: #da190b; }
        .btn-stop { background: #999; color: white; }
        .btn-stop:hover { background: #777; }
        
        .toggle-manual {
            width: 100%;
            padding: 12px;
            margin: 10px 0;
            background: #667eea;
            color: white;
            border: none;
            border-radius: 4px;
            font-weight: 600;
            cursor: pointer;
        }
        
        .toggle-manual.active { background: #ff6b6b; }
        
        .status-badge {
            display: inline-block;
            padding: 4px 8px;
            border-radius: 3px;
            font-size: 11px;
            font-weight: 600;
            margin-left: 10px;
        }
        
        .status-winter { background: #e3f2fd; color: #1976d2; }
        .status-frost { background: #f3e5f5; color: #7b1fa2; }
        .status-manual { background: #fff3e0; color: #e65100; }
        
        .full-width { grid-column: 1 / -1; }
        
        .decision-box {
            background: #e8f5e9;
            border-left: 4px solid #4caf50;
            padding: 12px;
            border-radius: 4px;
            margin-top: 10px;
            font-size: 13px;
            color: #2e7d32;
        }
        
        .refresh-time {
            text-align: center;
            font-size: 11px;
            color: #999;
            margin-top: 20px;
            padding-top: 20px;
            border-top: 1px solid #e0e0e0;
        }
        
        @media (max-width: 600px) {
            .content {
                grid-template-columns: 1fr;
            }
            h1 { font-size: 18px; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🌱 Orangeri Control Dashboard</h1>
            <div class="subtitle">Smart Ventilation System</div>
        </header>
        
        <div class="content">
            <!-- System Status Card -->
            <div class="card">
                <div class="card-title">
                    System Status
                    <span id="winterBadge" class="status-badge" style="display:none;">Winter Mode</span>
                    <span id="frostBadge" class="status-badge" style="display:none;">Frost Risk</span>
                    <span id="manualBadge" class="status-badge" style="display:none;">Manual</span>
                </div>
                <div class="sensor-row">
                    <span class="sensor-label">Uptime</span>
                    <span class="sensor-value" id="uptime">-</span>
                </div>
                <div class="sensor-row">
                    <span class="sensor-label">Raining</span>
                    <span class="sensor-value" id="raining">-</span>
                </div>
                <button class="toggle-manual" id="manualToggle" onclick="toggleManualControl()">Enable Manual Control</button>
                <div id="decisionBox" class="decision-box" style="display:none;"></div>
            </div>
            
            <!-- Inside Environment Card -->
            <div class="card">
                <div class="card-title">Inside Environment</div>
                <div class="sensor-row">
                    <span class="sensor-label">Temperature</span>
                    <span class="sensor-value" id="tempInside">-</span>
                </div>
                <div class="sensor-row">
                    <span class="sensor-label">Humidity</span>
                    <span class="sensor-value" id="humidityInside">-</span>
                </div>
            </div>
            
            <!-- Outside Environment Card -->
            <div class="card">
                <div class="card-title">Outside Environment</div>
                <div class="sensor-row">
                    <span class="sensor-label">Temperature</span>
                    <span class="sensor-value" id="tempOutside">-</span>
                </div>
                <div class="sensor-row">
                    <span class="sensor-label">Humidity</span>
                    <span class="sensor-value" id="humidityOutside">-</span>
                </div>
                <div class="sensor-row">
                    <span class="sensor-label">Wind Speed</span>
                    <span class="sensor-value" id="windSpeed">-</span>
                </div>
                <div class="sensor-row">
                    <span class="sensor-label">Wind Direction</span>
                    <span class="sensor-value" id="windDir">-</span>
                </div>
            </div>
            
            <!-- Hatch Control Card -->
            <div class="card full-width">
                <div class="card-title">Hatch Controls</div>
                
                <div class="hatch-control">
                    <div class="hatch-name">Hatch 1</div>
                    <span class="hatch-state" id="hatch1State">-</span>
                    <div class="button-group" id="hatch1Controls">
                        <button class="btn-open" onclick="controlHatch(1, 'open')">Open</button>
                        <button class="btn-close" onclick="controlHatch(1, 'close')">Close</button>
                        <button class="btn-stop" onclick="controlHatch(1, 'stop')">Stop</button>
                    </div>
                </div>
                
                <div class="hatch-control">
                    <div class="hatch-name">Hatch 2</div>
                    <span class="hatch-state" id="hatch2State">-</span>
                    <div class="button-group" id="hatch2Controls">
                        <button class="btn-open" onclick="controlHatch(2, 'open')">Open</button>
                        <button class="btn-close" onclick="controlHatch(2, 'close')">Close</button>
                        <button class="btn-stop" onclick="controlHatch(2, 'stop')">Stop</button>
                    </div>
                </div>
            </div>
            
            <!-- Settings Card -->
            <div class="card full-width">
                <div class="card-title">Thresholds & Settings</div>
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px;">
                    <div>
                        <label>Open Temp (°C)</label>
                        <input type="number" step="0.5" id="tempOpen" onchange="updateThresholds()">
                    </div>
                    <div>
                        <label>Close Temp (°C)</label>
                        <input type="number" step="0.5" id="tempClose" onchange="updateThresholds()">
                    </div>
                    <div>
                        <label>Open Humidity (%)</label>
                        <input type="number" id="humidityOpen" onchange="updateThresholds()">
                    </div>
                </div>
            </div>
            
            <div class="refresh-time">
                Last updated: <span id="lastUpdate">-</span> | 
                <a href="#" onclick="location.reload(); return false;">Refresh</a>
            </div>
        </div>
    </div>

    <script>
        let manualControlActive = false;
        
        async function updateDashboard() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                
                // Update sensors
                document.getElementById('tempInside').textContent = data.sensors.temp_inside + '°C';
                document.getElementById('humidityInside').textContent = data.sensors.humidity_inside + '%';
                document.getElementById('tempOutside').textContent = data.sensors.temp_outside + '°C';
                document.getElementById('humidityOutside').textContent = data.sensors.humidity_outside + '%';
                document.getElementById('windSpeed').textContent = data.sensors.wind_speed + ' m/s';
                document.getElementById('windDir').textContent = data.sensors.wind_direction + '°';
                document.getElementById('raining').textContent = data.sensors.raining ? 'Yes' : 'No';
                
                // Update hatches
                updateHatchDisplay('hatch1State', data.hatches.hatch1);
                updateHatchDisplay('hatch2State', data.hatches.hatch2);
                
                // Update badges
                document.getElementById('winterBadge').style.display = data.system.winter_mode ? 'inline' : 'none';
                document.getElementById('frostBadge').style.display = data.system.frost_risk ? 'inline' : 'none';
                document.getElementById('manualBadge').style.display = data.system.manual_control ? 'inline' : 'none';
                
                // Update decision
                const decisionBox = document.getElementById('decisionBox');
                if (data.last_decision) {
                    decisionBox.textContent = data.last_decision;
                    decisionBox.style.display = 'block';
                }
                
                // Update uptime
                const hours = Math.floor(data.system.uptime / 3600);
                const minutes = Math.floor((data.system.uptime % 3600) / 60);
                document.getElementById('uptime').textContent = hours + 'h ' + minutes + 'm';
                
                document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
                
                // Load thresholds
                const thresholds = await fetch('/api/thresholds').then(r => r.json());
                document.getElementById('tempOpen').value = thresholds.temp_open;
                document.getElementById('tempClose').value = thresholds.temp_close;
                document.getElementById('humidityOpen').value = thresholds.humidity_open;
                
            } catch (error) {
                console.error('Error fetching data:', error);
            }
        }
        
        function updateHatchDisplay(elementId, state) {
            const element = document.getElementById(elementId);
            element.textContent = state.toUpperCase();
            element.className = 'hatch-state';
            
            if (state.includes('open')) element.classList.add('open');
            if (state.includes('closed')) element.classList.add('closed');
            if (state.includes('slightly')) element.classList.add('slightly');
            if (state.includes('ing')) element.classList.add('moving');
        }
        
        async function controlHatch(hatchNum, command) {
            if (!manualControlActive) {
                alert('Enable Manual Control first');
                return;
            }
            
            const payload = {};
            payload['hatch' + hatchNum] = command;
            
            try {
                await fetch('/api/manual', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload)
                });
                updateDashboard();
            } catch (error) {
                console.error('Error controlling hatch:', error);
            }
        }
        
        async function toggleManualControl() {
            manualControlActive = !manualControlActive;
            const btn = document.getElementById('manualToggle');
            btn.textContent = manualControlActive ? 'Disable Manual Control' : 'Enable Manual Control';
            btn.classList.toggle('active');
            
            try {
                await fetch('/api/manual', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ manual_control: manualControlActive })
                });
                updateDashboard();
            } catch (error) {
                console.error('Error toggling manual control:', error);
            }
        }
        
        async function updateThresholds() {
            const payload = {
                temp_open: parseFloat(document.getElementById('tempOpen').value),
                temp_close: parseFloat(document.getElementById('tempClose').value),
                humidity_open: parseInt(document.getElementById('humidityOpen').value)
            };
            
            try {
                await fetch('/api/thresholds', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload)
                });
            } catch (error) {
                console.error('Error updating thresholds:', error);
            }
        }
        
        // Update every 5 seconds
        updateDashboard();
        setInterval(updateDashboard, 5000);
    </script>
</body>
</html>
  )";
}

// ============ SETUP & MAIN LOOP ============

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║  Orangeri IoT Ventilation Control System      ║");
  Serial.println("║  With WiFi & Winter Climate Protection        ║");
  Serial.println("╚════════════════════════════════════════════════╝\n");
  
  // Initialize hatch control pins
  pinMode(HATCH_1_OPEN, OUTPUT);
  pinMode(HATCH_1_CLOSE, OUTPUT);
  pinMode(HATCH_2_OPEN, OUTPUT);
  pinMode(HATCH_2_CLOSE, OUTPUT);
  
  // Initialize sensor pins
  pinMode(TEMP_INSIDE_PIN, INPUT);
  pinMode(TEMP_OUTSIDE_PIN, INPUT);
  pinMode(HUMIDITY_INSIDE_PIN, INPUT);
  pinMode(HUMIDITY_OUTSIDE_PIN, INPUT);
  pinMode(WIND_SPEED_PIN, INPUT);
  pinMode(WIND_DIR_PIN, INPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  
  // All relays off initially
  stopHatch(HATCH_1_OPEN, HATCH_1_CLOSE);
  stopHatch(HATCH_2_OPEN, HATCH_2_CLOSE);
  
  // WiFi Setup
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ WiFi Failed - Running in local mode");
  }
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/api/status", handleAPI_Status);
  server.on("/api/manual", handleAPI_ManualControl);
  server.on("/api/thresholds", handleAPI_Thresholds);
  server.onNotFound([] {
    server.send(404, "text/plain", "Not found");
  });
  
  server.begin();
  Serial.println("✓ Web Server started");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Access dashboard at: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
  }
  
  last_decision_time = millis();
}

void loop() {
  server.handleClient();
  
  // Update hatch movement
  updateHatchMovement(hatch1_state, HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
  updateHatchMovement(hatch2_state, HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
  
  // Make decisions at regular intervals (automatic mode only)
  if (!manual_control && millis() - last_decision_time >= DECISION_INTERVAL) {
    makeVentilationDecision();
    last_decision_time = millis();
  }
  
  // Cloud logging
  if (millis() - last_cloud_sync >= CLOUD_SYNC_INTERVAL) {
    logToCloud();
    last_cloud_sync = millis();
  }
  
  delay(100);
}
