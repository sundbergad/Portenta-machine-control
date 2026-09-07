# Cloud Logging Setup Guide

Your Orangeri system can log data to the cloud for:
- Historical analysis and trends
- Remote monitoring from anywhere
- Alerts and notifications
- Long-term climate pattern analysis

## Option 1: ThingSpeak (Recommended for Beginners)

### Setup Steps:

1. **Create Account**: Go to https://thingspeak.com and sign up
2. **Create Channel**:
   - Click "Create" → "New Channel"
   - Name: "Orangeri Climate"
   - Enable 8 fields:
     - Field 1: Temperature Inside
     - Field 2: Temperature Outside
     - Field 3: Humidity Inside
     - Field 4: Humidity Outside
     - Field 5: Wind Speed
     - Field 6: Wind Direction
     - Field 7: Frost Risk (0/1)
     - Field 8: Winter Mode (0/1)
   - Save
3. **Get API Key**: View the channel, find "API Keys" section
4. **Add to Arduino Code**:

```cpp
#include <WiFi.h>

// After logToCloud() function:
const char* thingspeakServer = "api.thingspeak.com";
const char* thingspeakApiKey = "YOUR_API_KEY_HERE";

void logToCloud() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  WiFiClient client;
  if (!client.connect(thingspeakServer, 80)) {
    Serial.println("ThingSpeak connection failed");
    return;
  }
  
  String url = "/update?api_key=";
  url += thingspeakApiKey;
  url += "&field1=" + String(current_stats.temp_inside);
  url += "&field2=" + String(current_stats.temp_outside);
  url += "&field3=" + String(current_stats.humidity_inside);
  url += "&field4=" + String(current_stats.humidity_outside);
  url += "&field5=" + String(current_stats.wind_speed);
  url += "&field6=" + String(current_stats.wind_direction);
  url += "&field7=" + String(current_stats.in_frost_mode ? 1 : 0);
  url += "&field8=" + String(current_stats.in_winter_mode ? 1 : 0);
  
  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: api.thingspeak.com\r\n");
  client.print("Connection: close\r\n\r\n");
  
  delay(500);
  client.stop();
  Serial.println("Data sent to ThingSpeak");
}
```

### ThingSpeak Advantages:
- ✓ Free tier (3 million messages/year)
- ✓ Built-in charts and analytics
- ✓ Mobile app available
- ✓ Very easy setup
- ✗ Limited customization

---

## Option 2: Blynk (Best for Mobile Apps)

### Setup Steps:

1. **Install Blynk**: https://blynk.io
2. **Create Project**:
   - New Project → Portenta H7 → WiFi
   - Copy the Auth Token
3. **Add Library**:
   - Arduino IDE → Sketch → Include Library → Manage Libraries
   - Search "Blynk"
   - Install by Volodymyr Shymanskyy
4. **Code Example**:

```cpp
#define BLYNK_TEMPLATE_ID "TMxxxxxx"
#define BLYNK_TEMPLATE_NAME "Orangeri"
#define BLYNK_AUTH_TOKEN "xxxxxxxxxxxxx"

#include <WiFi.h>
#include <BlynkSimpleWiFi.h>

char auth[] = "YOUR_AUTH_TOKEN";
char ssid[] = "YOUR_SSID";
char pass[] = "YOUR_PASSWORD";

void setup() {
  Blynk.begin(auth, ssid, pass);
}

void loop() {
  Blynk.run();
  
  // Send data to Blynk virtual pins
  Blynk.virtualWrite(V0, current_stats.temp_inside);
  Blynk.virtualWrite(V1, current_stats.humidity_inside);
  Blynk.virtualWrite(V2, current_stats.temp_outside);
  // etc.
}

// Receive commands from Blynk app
BLYNK_WRITE(V10) {
  if (param.asInt() == 1) {
    openHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
  }
}
```

### Blynk Advantages:
- ✓ Beautiful mobile app
- ✓ Drag-and-drop dashboard
- ✓ Real-time push notifications
- ✓ Free tier available
- ✗ Limited storage (free tier)

---

## Option 3: Firebase (Best for Scalability)

Firebase offers real-time database and hosting.

### Setup Steps:

1. **Create Firebase Project**: https://firebase.google.com
2. **Enable Realtime Database** (Firestore)
3. **Arduino Setup**:

```cpp
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "your-project.firebaseio.com"
#define USER_EMAIL "your-email@example.com"
#define USER_PASSWORD "your-password"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  Firebase.begin(&config, &auth);
}

void logToCloud() {
  if (!Firebase.ready()) return;
  
  FirebaseJson json;
  json.set("timestamp", millis());
  json.set("temp_inside", current_stats.temp_inside);
  json.set("temp_outside", current_stats.temp_outside);
  json.set("humidity_inside", current_stats.humidity_inside);
  json.set("humidity_outside", current_stats.humidity_outside);
  json.set("wind_speed", current_stats.wind_speed);
  json.set("frost_risk", current_stats.in_frost_mode);
  
  Firebase.database(&fbdo).set("/logs/" + String(millis()), json);
}
```

### Firebase Advantages:
- ✓ Excellent scalability
- ✓ Real-time updates
- ✓ Free tier (good for testing)
- ✓ Built-in authentication
- ✗ More complex setup

---

## Option 4: Azure IoT Hub (Professional Grade)

Best for industrial applications.

```cpp
#include <az_core.h>
#include <az_iot.h>
#include <WiFi.h>

// Setup Azure connection
// See: https://github.com/Azure/azure-iot-sdk-c

void logToCloud() {
  // Send MQTT message to Azure
  static char telemetry_topic[256];
  static char telemetry_payload[1024];
  
  snprintf(telemetry_payload, sizeof(telemetry_payload),
    "{"
    "\"temp_in\":%.1f,"
    "\"temp_out\":%.1f,"
    "\"humidity_in\":%d,"
    "\"humidity_out\":%d,"
    "\"wind_speed\":%.1f,"
    "\"frost_risk\":%d"
    "}",
    current_stats.temp_inside,
    current_stats.temp_outside,
    current_stats.humidity_inside,
    current_stats.humidity_outside,
    current_stats.wind_speed,
    current_stats.in_frost_mode ? 1 : 0
  );
  
  // Publish to Azure
  // mqtt_client.publish(telemetry_topic, telemetry_payload);
}
```

---

## Option 5: Local InfluxDB + Grafana (Maximum Control)

Run your own server for complete control:

### Architecture:
```
Orangeri (Arduino)
    ↓ (HTTP POST)
InfluxDB (Time-series database)
    ↓
Grafana (Beautiful dashboards)
```

### Arduino Code to Send to InfluxDB:

```cpp
void logToCloud() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  WiFiClient client;
  const char* influxServer = "192.168.1.100"; // Your InfluxDB IP
  const int influxPort = 8086;
  const char* influxDB = "orangeri";
  
  if (!client.connect(influxServer, influxPort)) {
    Serial.println("InfluxDB connection failed");
    return;
  }
  
  // InfluxDB Line Protocol format
  String lineData = "sensors,location=orangeri ";
  lineData += "temp_in=" + String(current_stats.temp_inside) + ",";
  lineData += "temp_out=" + String(current_stats.temp_outside) + ",";
  lineData += "humidity_in=" + String(current_stats.humidity_inside) + ",";
  lineData += "humidity_out=" + String(current_stats.humidity_outside) + ",";
  lineData += "wind_speed=" + String(current_stats.wind_speed) + ",";
  lineData += "frost_risk=" + String(current_stats.in_frost_mode ? 1 : 0);
  
  String request = "POST /write?db=" + String(influxDB) + " HTTP/1.1\r\n";
  request += "Host: " + String(influxServer) + ":" + String(influxPort) + "\r\n";
  request += "Content-Length: " + String(lineData.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += lineData;
  
  client.print(request);
  delay(500);
  client.stop();
  
  Serial.println("Data logged to InfluxDB");
}
```

### Grafana Setup:
1. Install Grafana on your server
2. Add InfluxDB as data source
3. Create dashboards with charts
4. Access from browser at http://your-server:3000

### Advantages:
- ✓ Complete control
- ✓ No data limits
- ✓ Beautiful dashboards
- ✓ Professional appearance
- ✗ Requires server/PC always running

---

## My Recommendation for Your Project:

**Start with**: Blynk or ThingSpeak (easy, free)
**Then upgrade to**: Firebase (scalable, good free tier)
**Later consider**: InfluxDB + Grafana (if you want to run your own server)

---

## Data Retention & Privacy:

- **Cloud**: Your data is on external servers
  - Pros: Accessible from anywhere, automatic backups
  - Cons: Privacy concerns, requires internet

- **Local (InfluxDB)**: Data on your own hardware
  - Pros: Complete privacy, full control
  - Cons: Need to maintain the server

**Recommendation for Sweden**: Use local logging for privacy, sync to cloud for remote access when needed.

---

## Testing Your Cloud Connection:

```cpp
void testCloudConnection() {
  Serial.println("Testing cloud connection...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi not connected");
    return;
  }
  
  WiFiClient client;
  if (!client.connect("8.8.8.8", 53)) {
    Serial.println("✗ Internet unreachable");
    return;
  }
  
  Serial.println("✓ Internet connection OK");
  client.stop();
  
  // Test cloud service connection here
}
```

Call this in setup() to verify before running.
