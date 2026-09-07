# Complete Setup & Configuration Guide

## Quick Start Checklist

- [ ] **Hardware**: Portenta Machine Control received and powered
- [ ] **Sensors**: All environmental sensors purchased and tested
- [ ] **WiFi**: Network SSID and password available
- [ ] **Arduino IDE**: Installed with Portenta support
- [ ] **Libraries**: ArduinoJson and WiFi libraries installed
- [ ] **Code**: Downloaded the `orangeri_iot_complete.ino` sketch
- [ ] **Configuration**: WiFi credentials updated in code
- [ ] **Testing**: System tested in safe environment
- [ ] **Deployment**: Installed in orangeri with proper wiring

---

## Step 1: Hardware Assembly

### Components Needed:

**Controller**:
- Portenta Machine Control
- USB-C power supply (24V recommended)
- Ethernet cable or WiFi antenna

**Environmental Sensors**:
- Temperature sensor (inside & outside): LM35 or DS18B20 (×2)
- Humidity sensor (inside & outside): DHT22 or HIH7000 (×2)
- Wind speed sensor: Cup anemometer (analog output)
- Wind direction sensor: Wind vane (analog output)
- Rain sensor: Rain drop detector module

**Hatch Control**:
- 2× DC motors (or stepper motors)
- 2× 24V relays for each hatch (open & close)
- 24V power supply for motors
- Emergency manual override button (optional)

**Housing & Cabling**:
- Waterproof enclosure for controller
- Shielded cables for sensor inputs (analog noise prevention)
- Connector blocks for field wiring
- Cable glands for weatherproofing

### Wiring Diagram:

```
┌─────────────────────────────────────────────────────────────┐
│  Portenta Machine Control (in waterproof enclosure)         │
│                                                              │
│  Analog Inputs (A0-A6):                                      │
│    A0 ─── Temp Inside ──────────────────────────────┐      │
│    A1 ─── Temp Outside ─────────────────────────────┤      │
│    A2 ─── Humidity Inside ──────────────────────────┤      │
│    A3 ─── Humidity Outside ─────────────────────────┤      │
│    A4 ─── Wind Speed ───────────────────────────────┤      │
│    A5 ─── Wind Direction ──────────────────────────┤      │
│    A6 ─── Rain Sensor ──────────────────────────────┤      │
│                                                      │      │
│  Digital Outputs (D0-D3):                           │      │
│    D0 ─── Hatch1 Open Relay                         │      │
│    D1 ─── Hatch1 Close Relay                        │      │
│    D2 ─── Hatch2 Open Relay                         │      │
│    D3 ─── Hatch2 Close Relay                        │      │
│                                                      │      │
│  Power:                                             │      │
│    +24V ─ Motor power supply                        │      │
│    GND  ─ Common ground (tie sensors & motors)      │      │
│                                                      │      │
│  Network:                                           │      │
│    WiFi antenna ──────────────────────────────────┘      │
│                                                              │
└─────────────────────────────────────────────────────────────┘

                          ↓
                    
┌─────────────────────────────────────────────────────────────┐
│  Field Wiring to Sensors & Motors                          │
│                                                              │
│  • Use twisted pair for sensor cables                       │
│  • Shield cables to prevent interference                    │
│  • Run motor power separate from sensor cables             │
│  • Install voltage dividers if using 5V ADC with 24V       │
│                                                              │
│  Temperature Sensors (LM35):                               │
│    Pin 1 → +5V                                             │
│    Pin 2 → Analog Input (A0 or A1)                         │
│    Pin 3 → GND                                             │
│                                                              │
│  Motor Relays:                                             │
│    Relay Coil → Digital Output (D0-D3)                     │
│    Relay Contacts → 24V Motor Circuit                      │
│    Add flyback diode on coil for protection               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Step 2: Software Installation

### Install Arduino IDE:

1. Download from https://www.arduino.cc/en/software
2. Install on your computer
3. Launch Arduino IDE

### Install Portenta Support:

1. Tools → Board Manager
2. Search "portenta"
3. Install "Arduino Portenta H7"
4. Tools → Board → Select "Portenta H7"

### Install Required Libraries:

1. Sketch → Include Library → Manage Libraries
2. Search and install:
   - **ArduinoJson** (by Benoit Blanchon)
   - **WiFi** (built-in, verify it's available)
   - **WebServer** (built-in)

### Download Your Code:

1. Copy the entire `orangeri_iot_complete.ino` file
2. Create new sketch in Arduino IDE
3. Paste all code
4. Save as "orangeri_ventilation"

---

## Step 3: Configuration

### A. WiFi Setup

Edit these lines at the top of the code:

```cpp
const char* ssid = "YOUR_SSID";           // Your WiFi network name
const char* password = "YOUR_PASSWORD";   // Your WiFi password
const char* hostname = "orangeri-control"; // Device name on network
```

**Example**:
```cpp
const char* ssid = "GardenWiFi";
const char* password = "MySecurePassword123";
const char* hostname = "orangeri-sundsvall";
```

### B. Sensor Pin Configuration

If using different pins, update:

```cpp
const int TEMP_INSIDE_PIN = A0;      // Change if needed
const int TEMP_OUTSIDE_PIN = A1;
const int HUMIDITY_INSIDE_PIN = A2;
// ... etc
```

### C. Temperature Thresholds

Adjust for your crops and climate:

```cpp
// Summer settings (default)
float TEMP_OPEN_THRESHOLD = 22.0;      // °C
float TEMP_CLOSE_THRESHOLD = 20.0;     // °C

// Humidity settings
int HUMIDITY_OPEN_THRESHOLD = 70;      // %
int HUMIDITY_CLOSE_THRESHOLD = 60;     // %

// Wind safety
const float WIND_MAX_SAFE = 8.0;       // m/s (close if > this)
const float WIND_MODERATE = 6.0;       // m/s (slightly open)
```

**For Swedish greenhouse**: Increase thresholds by 2-3°C for better heat retention.

### D. Winter Protection Setup

In `setup()` function, add after `Serial.println("System initialized...")`:

```cpp
// Customize for Sundsvall winters
FROST_PROTECTION_TEMP = 0.0;       // Don't open below 0°C
MIN_WINTER_TEMP = -10.0;           // Emergency close at -10°C
WINTER_HUMIDITY_THRESHOLD = 85;    // Higher tolerance
ANTI_CONDENSATION_MODE = true;     // Enable dawn ventilation
```

### E. Cloud Logging (Optional)

To enable cloud logging, choose one option and add the implementation:

**For ThingSpeak** (easiest):
```cpp
const char* thingspeakApiKey = "YOUR_THINGSPEAK_API_KEY";
// Then uncomment and implement the logToCloud() function
```

See `CLOUD_LOGGING_SETUP.md` for detailed instructions.

---

## Step 4: Motor Configuration

### Calculate Motor Timeout

Your hatch motors need time to fully open/close:

```cpp
// Test procedure:
// 1. Manually open one hatch fully - time it
// 2. Note the seconds (e.g., 45 seconds)
// 3. Add 5-10 seconds safety margin
// 4. Convert to milliseconds (×1000)

// Example: 45 seconds + 5 second margin = 50 seconds = 50000 ms
unsigned long hatch_move_timeout = 50000;  // Update this value
```

### Motor Type: DC with Dual Relays

Most common configuration:

```
Open Relay:
  Triggered → Motor moves UP/OPEN
  Release → Motor stops

Close Relay:
  Triggered → Motor moves DOWN/CLOSE
  Release → Motor stops
```

The code automatically:
- Turns off the close relay before opening (prevents conflict)
- Times the motor movement
- Stops the motor after timeout
- Never activates both relays simultaneously

### If Using Stepper Motors Instead:

Replace relay control with step/direction pins:

```cpp
const int MOTOR1_STEP = D0;
const int MOTOR1_DIR = D1;

void openHatchStepper(int step_pin, int dir_pin, int steps) {
  digitalWrite(dir_pin, HIGH);  // Set direction to open
  for (int i = 0; i < steps; i++) {
    digitalWrite(step_pin, HIGH);
    delayMicroseconds(500);
    digitalWrite(step_pin, LOW);
    delayMicroseconds(500);
  }
}
```

---

## Step 5: Sensor Calibration

### Temperature Sensors

**Calibration method**:
```
1. Submerge sensor in ice water (0°C)
   - Should read close to 0°C
   - Note any offset

2. Submerge in boiling water (100°C)
   - Should read close to 100°C
   - Calculate error range

3. Apply correction in code:
   float readTemperature(int pin) {
     int raw = analogRead(pin);
     float temp = (raw * 5.0 / 1024.0) * 100.0;
     return temp - 2.0;  // Subtract calibration offset
   }
```

### Humidity Sensors

**Calibration method**:
```
1. Place in dry chamber (0% humidity reference)
   - Should read very low
   
2. Place in humid chamber (95% humidity reference)
   - Should read close to 95%
   
3. Adjust map() function accordingly:
   int readHumidity(int pin) {
     int raw = analogRead(pin);
     // Adjust these values based on your sensor
     int humidity = map(raw, 150, 850, 0, 100);
     return constrain(humidity, 0, 100);
   }
```

### Wind Speed Sensor

**Calibration**:
```
1. Use calibration data from manufacturer
2. Most are 0.4 m/s per cup rotation
3. Adjust the formula:
   float readWindSpeed() {
     int raw = analogRead(WIND_SPEED_PIN);
     // Adjust 20.0 to your sensor's max range
     float wind_speed = (raw / 1023.0) * 20.0;
     return wind_speed;
   }
```

### Rain Sensor

**Calibration**:
```
const int RAIN_THRESHOLD = 400;  // Adjust based on testing

// To find the right threshold:
// 1. Print raw sensor value when dry: analogRead(RAIN_SENSOR_PIN)
// 2. Print raw value when wet
// 3. Choose threshold value between them
// 4. Example: dry = 100, wet = 600, set threshold to 350
```

---

## Step 6: Upload & Test

### Upload Code:

1. Connect Portenta to computer via USB-C
2. Arduino IDE → Tools → Port → Select Portenta port
3. Verify code compiles: Sketch → Verify
4. Upload: Sketch → Upload
5. Wait for upload to complete

### Monitor Serial Output:

1. Tools → Serial Monitor
2. Set baud rate: 115200
3. Watch for startup messages:

```
╔════════════════════════════════════════════════╗
║  Orangeri IoT Ventilation Control System      ║
║  With WiFi & Winter Climate Protection        ║
╚════════════════════════════════════════════════╝

Connecting to WiFi: GardenWiFi
.....
✓ WiFi Connected!
IP Address: 192.168.1.123

✓ Web Server started
Access dashboard at: http://192.168.1.123/
```

### Test Each Component:

```
1. Temperature Sensors:
   - Check serial output for readings
   - Should be within ±1°C of actual temperature

2. Humidity Sensors:
   - Check for reasonable values (0-100%)
   - Should respond to humidity changes

3. Hatch Motors:
   - Go to web dashboard
   - Enable Manual Control
   - Click "Open" button for Hatch 1
   - Listen for relay clicking and motor running
   - Verify hatch physically moves
   - Click "Close" - should reverse
   - Click "Stop" - should stop immediately

4. Wind Sensor:
   - Fan or blow near anemometer
   - Check serial output for increasing wind speed values

5. Rain Sensor:
   - Spray water on sensor
   - Should detect precipitation in serial output

6. Web Dashboard:
   - Open browser to http://192.168.1.123/
   - Should display all sensor values
   - Manual controls should be responsive
   - Dashboard should auto-update every 5 seconds
```

---

## Step 7: Deployment

### Installation Location:

1. **Controller**: Mount in weather-proof enclosure near orangeri
2. **Sensors**:
   - Temperature: One inside, one on shaded north wall outside
   - Humidity: Same locations as temperature
   - Wind: Mounted on roof, high point, away from obstructions
   - Rain: Horizontal surface, open to sky, no overhang
3. **Motors**: Mounted to hatches with mechanical linkage
4. **Cables**: Run in conduit for protection

### Weather Protection:

- All connections must be waterproof
- Use silicone sealant on cable entries
- Install drain holes in enclosure (bottom)
- Use stainless steel fasteners (prevents corrosion)
- Paint enclosure light color (reflects heat)

### Power Management:

```
24V Power Supply (main)
    ├─→ Portenta Machine Control (+24V, GND)
    ├─→ Relay Coils (24V)
    ├─→ Motor Power (24V)
    └─→ Sensor Power (5V via buck converter)
```

Ensure proper grounding of all components!

---

## Step 8: Ongoing Monitoring

### Daily Checks:

- [ ] System shows "Connected" status
- [ ] All sensor readings appear reasonable
- [ ] Hatches respond to automatic commands
- [ ] No error messages in serial output
- [ ] Dashboard loads without errors

### Weekly Checks:

- [ ] Sensors aren't covered with dust/pollen
- [ ] No water ingress in enclosure (check condensation)
- [ ] Hatch movement is smooth (no grinding sounds)
- [ ] Wind sensor spins freely
- [ ] Rain sensor isn't clogged

### Monthly Checks:

- [ ] Verify temperature calibration (sunny/cloudy days vary)
- [ ] Check relay contacts for corrosion
- [ ] Test manual override button
- [ ] Review cloud logs if enabled (check for anomalies)
- [ ] Inspect seals around hatches

### Seasonal Transitions:

- **October**: Switch to autumn thresholds, prepare for frost
- **November**: Test winter logic, ensure emergency stops work
- **March**: Gradually increase ventilation, monitor for fungal issues
- **May**: Verify summer thresholds are appropriate

---

## Troubleshooting

### WiFi Connection Failing

**Symptom**: "WiFi Failed - Running in local mode"

**Solutions**:
1. Verify SSID and password are correct (case-sensitive)
2. Check if network is 2.4GHz (Portenta doesn't support 5GHz)
3. Move closer to router to verify signal strength
4. Restart router and try again
5. Check if MAC address is blocked in router settings

### Sensors Reading Zeros

**Symptom**: All sensor values show 0 or -40

**Solutions**:
1. Check wiring from sensor to analog input
2. Verify power is reaching the sensors (multimeter test)
3. Test with simple Arduino sketch (AnalogRead) to isolate issue
4. Replace sensor if confirmed faulty
5. Check for loose connections

### Hatches Won't Move

**Symptom**: Manual control buttons don't open/close hatches

**Solutions**:
1. Verify motors have 24V power (check power supply)
2. Listen for relay clicking when button clicked
   - If no click: relay or digital line issue
   - If click but no motor: wiring or motor fault
3. Check hatch position switches (if equipped)
4. Test relay manually with multimeter
5. Verify motor timeout isn't too short

### Dashboard Won't Load

**Symptom**: Browser times out or shows "connection refused"

**Solutions**:
1. Verify Portenta is connected to WiFi (check serial output)
2. Ping the device: ping 192.168.1.123 (replace with your IP)
3. Try accessing from a different device
4. Check if browser is blocking the page (allow insecure content)
5. Restart Portenta if nothing else works

### Frost Protection Not Activating

**Symptom**: Hatches open during frost conditions

**Solutions**:
1. Check `FROST_PROTECTION_TEMP` setting (should be 0-2°C)
2. Verify outside temperature sensor is working
3. Check if `WINTER_MODE` is enabled
4. Review serial output decision logic
5. Check date/time if using RTC for seasonal detection

---

## Performance Optimization

### Reduce WiFi Lag:

```cpp
// In loop(), reduce server response time:
server.handleClient();  // Call frequently
```

### Improve Sensor Accuracy:

```cpp
// Average multiple readings:
float readTemperature(int pin) {
  float sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  float avg = sum / 10.0;
  float temp = (avg * 5.0 / 1024.0) * 100.0;
  return temp;
}
```

### Reduce Power Consumption:

```cpp
// Increase decision interval to save power:
const unsigned long DECISION_INTERVAL = 10000; // 10 seconds
// (default is 5 seconds)
```

---

## Security Considerations

⚠️ **Important for network-connected devices**:

1. **Change Default Hostname**: Don't use "orangeri-control" on untrusted networks
2. **Use Strong WiFi Password**: At least 16 characters
3. **Disable Remote Access**: If using cloud logging, use API keys, not passwords
4. **Update Regularly**: When new Arduino/library versions are available
5. **Isolate Network**: Keep on separate WiFi network if possible

---

## Next Steps

1. **Test thoroughly** in controlled environment before field deployment
2. **Document your settings** - save a copy of your configuration
3. **Set up cloud logging** - recommend ThingSpeak or Blynk for monitoring
4. **Create backup power** - consider solar + battery for winter reliability
5. **Install manual override** - never rely solely on automation

---

## Support Resources

- Arduino Forum: https://forum.arduino.cc/
- Portenta Documentation: https://docs.arduino.cc/hardware/portenta-machine-control
- GitHub Issues: Create issues if you encounter problems
- Community: Swedish Arduino enthusiasts forum at SweForum

Your system is now ready to keep your orangeri at perfect climate year-round! 🌱🏡
