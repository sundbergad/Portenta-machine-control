# Winter Climate Protection & Advanced Logic Guide

Your orangeri system includes sophisticated winter climate protection to prevent damage from frost, snow, and extreme cold.

---

## Seasonal Modes Explained

### SUMMER Mode (May-September)
**Trigger**: Outside temperature > 20°C

**Thresholds**:
- Open hatches: Inside temp > 22°C
- Close hatches: Inside temp < 20°C
- Humidity threshold: 70% (aggressive ventilation)

**Behavior**: 
- Prioritizes cooling and dehumidification
- Opens hatches readily
- Wind safety: closes if > 8 m/s

**Best for**: Growing heat-loving plants, preventing mold

---

### AUTUMN Mode (September-November)
**Trigger**: Outside temperature 10-20°C

**Thresholds**:
- Open hatches: Inside temp > 22°C
- Close hatches: Inside temp < 20°C
- Humidity threshold: 75% (increased tolerance)

**Behavior**:
- Balances ventilation with heat retention
- Prepares for winter by hardening plants
- Watch for early frost nights

**Best for**: Gradual transition, frost preparation

---

### WINTER Mode (November-March)
**Trigger**: Outside temperature < 5°C OR detected by RTC module

**Critical Features**:

#### 1. **Frost Protection** ❄️
```cpp
FROST_PROTECTION_TEMP = 2.0°C  // Don't open if outside < this

// The system will:
// - Completely seal hatches when frost risk exists
// - Retain heat to prevent internal frost
// - Only open for extreme internal heat/humidity
```

**Why**: 
- Opening in sub-zero weather allows cold air circulation
- Can cause damage to plants and equipment
- Frost can seal hinges and mechanisms

#### 2. **Snow Load Detection** ⛄
```cpp
// Snow is detected when:
// - Temperature is -1°C to +1°C (melting/refreezing range)
// - AND rain sensor is wet (snow/sleet)
// - AND humidity outside > 90%

// When snow load detected:
// - Hatches stay CLOSED
// - Prevents snow from entering
// - Prevents ice accumulation on mechanical parts
```

**Why**:
- Opening creates dangerous drafts in snow
- Snow can block openings
- Ice formation damages seals

#### 3. **Anti-Condensation Ventilation** 🌫️
```cpp
ANTI_CONDENSATION_MODE = true;  // Enable in setup if desired

// In winter:
// - High inside humidity + cold night = frost inside
// - Solution: Gentle ventilation at dawn (4-6 AM)
// - Opens hatches slightly for 15-30 minutes
// - Allows moisture to escape before it freezes
// - Prevents frost accumulation on roof/glass
```

**Recommended Settings for Swedish Winter**:

```cpp
// Copy these to your setup() to override defaults:
FROST_PROTECTION_TEMP = 0.0;      // Start protecting at 0°C
MIN_WINTER_TEMP = -10.0;          // Emergency close at -10°C
WINTER_HUMIDITY_THRESHOLD = 85.0; // Allow more moisture inside
ANTI_CONDENSATION_MODE = true;    // Enable dawn ventilation
TEMP_OPEN_THRESHOLD = 25.0;       // Only open if very hot
TEMP_CLOSE_THRESHOLD = 18.0;      // Close to save heat
```

#### 4. **Night-Time Protection** 🌙
Winter nights are long in Sweden. Add timer-based logic:

```cpp
// Add to your sensor pins:
const int RTC_SDA = 20;  // I2C connections for RTC module
const int RTC_SCL = 21;

// In setup(), add RTC library:
#include <RTClib.h>
RTC_DS3231 rtc;

void setup() {
  if (!rtc.begin()) {
    Serial.println("RTC not found");
  }
  // Set time: rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

// In makeVentilationDecision(), add:
DateTime now = rtc.now();
int hour = now.hour();
int month = now.month();

// Completely close hatches at night (20:00-08:00) in winter
if (month < 4 || month > 10) {  // Nov-Mar
  if (hour >= 20 || hour < 8) {
    // NIGHT IN WINTER: Close hatches completely
    closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
    closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
    current_stats.last_decision = "Winter night: Sealed for heat retention";
    return;
  }
}
```

---

## Snow Detection Logic Explained

### How Snow is Detected:

1. **Temperature Check**
   - If outside temp is -1°C to +1°C → possible snow/sleet
   - If < -5°C → too cold to be liquid precipitation
   - If > +5°C → would melt, not accumulate

2. **Moisture Detection**
   - Rain sensor reads HIGH (wet from precipitation)
   - Outside humidity is very high (> 90%)
   - Both conditions together = snow/sleet likely

3. **Decision**
   - If snow detected: Close hatches
   - Keep closed until temp rises > 5°C

### Code Example:

```cpp
bool hasSnowLoad() {
  // Check if snow/sleet conditions exist
  bool cold_range = (current_stats.temp_outside > -3.0 && 
                     current_stats.temp_outside < 1.0);
  bool moisture = (current_stats.raining || 
                   current_stats.humidity_outside > 90);
  bool not_too_cold = current_stats.temp_outside > -8.0;
  
  return (cold_range && moisture && not_too_cold);
}
```

---

## Emergency Shutdown

If extreme cold is detected:

```cpp
const float MIN_WINTER_TEMP = -5.0;  // Adjust for your location

// In makeVentilationDecision(), at the very top:
if (current_stats.temp_outside < MIN_WINTER_TEMP) {
  // EMERGENCY: Seal everything tight
  closeHatch(HATCH_1_OPEN, HATCH_1_CLOSE, hatch1_start_time);
  closeHatch(HATCH_2_OPEN, HATCH_2_CLOSE, hatch2_start_time);
  stopHatch(HATCH_1_OPEN, HATCH_1_CLOSE);
  stopHatch(HATCH_2_OPEN, HATCH_2_CLOSE);
  
  current_stats.last_decision = "EMERGENCY: Extreme cold - all sealed";
  
  // Optional: Send alert
  sendAlert("EXTREME COLD WARNING: " + String(current_stats.temp_outside) + "°C");
  
  return;  // Skip all other logic
}
```

---

## Regional Calibration for Sundsvall, Sweden

Sundsvall climate (Västernorrland):
- **Coldest months**: December-February (-10° to -20°C)
- **Frost risk**: October-April
- **Peak snow**: January-February
- **Record low**: < -35°C (rare)

### Recommended Values:

```cpp
// Winter settings (Nov-Mar)
const float FROST_PROTECTION_TEMP = 2.0;    // Start protection
const float MIN_WINTER_TEMP = -8.0;         // Emergency close
const float WINTER_HUMIDITY_THRESHOLD = 85; // Higher tolerance

// Season detection
if (month < 4 || month > 10) {  // Nov-Mar
  WINTER_MODE = true;
  TEMP_OPEN_THRESHOLD = 25.0;   // Must be very hot to open
  TEMP_CLOSE_THRESHOLD = 18.0;  // Close earlier to save heat
} else if (month >= 5 && month <= 8) {  // May-Aug
  WINTER_MODE = false;
  TEMP_OPEN_THRESHOLD = 22.0;
  TEMP_CLOSE_THRESHOLD = 20.0;
}

// Frost risk period: Oct-Apr
if ((month >= 10) || (month <= 4)) {
  ANTI_CONDENSATION_MODE = true;
  // Gentle ventilation at dawn prevents indoor frost
}
```

---

## Manual Winter Override

You may need to manually override on special days:

### Via Web Dashboard:
1. Click "Enable Manual Control"
2. Use hatch buttons to open/close
3. Inspect for ice/frost before opening
4. Don't leave open overnight

### Safety Checklist:
- [ ] Outside temp is above 0°C
- [ ] No snow or ice on hatch mechanisms
- [ ] Wind speed < 4 m/s
- [ ] Check weather forecast (no freeze expected)
- [ ] Set reminder to close if forgetting

---

## Maintenance During Winter

### Monthly Checks:
- Inspect hinges for ice/frost buildup
- Check for moisture leaks around seals
- Verify wind sensor is not iced over
- Clear any snow from sensor domes
- Test manual override controls

### If Ice Develops on Hatches:
```cpp
// Don't force the motor - could break it
// Instead:
1. Manually warm hinges (warm water, NOT hot)
2. Let ice melt naturally
3. Check seal integrity
4. Clear ice from sensor domes

// Add to code:
const bool MOTOR_SAFETY_LIMIT = true;
// If motor stalls (current spike), stop immediately
```

---

## Combining Winter Logic with Other Modes

The system uses priority-based decision making:

```
Decision Tree:
├── Is it an emergency (< MIN_WINTER_TEMP)?
│   └── YES → CLOSE all, STOP all motors
├── Is it raining heavily?
│   └── YES → CLOSE all for protection
├── Is there snow load?
│   └── YES → CLOSE all, wait for temperature rise
├── Is wind too strong (> 8 m/s)?
│   └── YES → CLOSE all for safety
├── Is frost risk high?
│   └── YES → CLOSE all to retain heat
└── Normal operation:
    ├── Temperature too high?
    │   └── YES → OPEN (if not windy)
    └── Humidity too high?
        └── YES → OPEN (if warm enough)
```

Each check overrides the previous one, with safety being highest priority.

---

## Testing Winter Logic

### Simulate Winter Conditions:

```cpp
// Add to setup() for testing:
void testWinterLogic() {
  Serial.println("=== WINTER LOGIC TEST ===");
  
  // Simulate cold temperature
  current_stats.temp_outside = -5.0;
  current_stats.humidity_outside = 85;
  current_stats.raining = false;
  
  Serial.println("Test 1: Extreme cold");
  if (current_stats.temp_outside < MIN_WINTER_TEMP) {
    Serial.println("✓ PASS: Emergency shutdown activated");
  }
  
  // Simulate snow conditions
  current_stats.temp_outside = 0.5;
  current_stats.raining = true;
  current_stats.humidity_outside = 92;
  
  Serial.println("Test 2: Snow detection");
  if (hasSnowLoad()) {
    Serial.println("✓ PASS: Snow load detected, hatches should close");
  }
  
  // Simulate frost risk
  current_stats.temp_outside = 1.5;
  current_stats.humidity_inside = 80;
  
  Serial.println("Test 3: Frost protection");
  if (isFrostRisk()) {
    Serial.println("✓ PASS: Frost risk detected");
  }
  
  Serial.println("=== TEST COMPLETE ===");
}

// Call in setup(): testWinterLogic();
```

---

## When Winter Logic Might Fail

### Sensor Ice-Over 🧊
**Problem**: Temperature sensor covered in frost
**Solution**: 
- Mount sensors in protected enclosure
- Add heating element (optional)
- Regular inspection

### Rain Sensor Failure in Freezing Rain 🧊💧
**Problem**: Sensor freezes, doesn't detect ice
**Solution**:
- Add temperature check: if T < 0°C AND raining → assume ice
- Manual ice removal
- Consider heated rain sensor (industrial grade)

### Motor Freezing ❄️
**Problem**: Hatches won't move due to ice
**Solution**:
- Don't force motors (damage risk)
- Manual de-icing with warm water
- Silicone grease on hinges (non-conductive)
- Check motor amperage for safety limits

### Extreme Cold Brittleness 🧊
**Problem**: Plastic seals become brittle at -20°C
**Solution**:
- Use industrial-grade materials
- Check seals quarterly
- Keep hatches closed during extreme cold
- Accept reduced ventilation as safety trade-off

---

## Configuration Worksheet

Print and fill this out for your setup:

```
LOCATION: Sundsvall, Västernorrland
LATITUDE: 62.4°N

WINTER SETTINGS:
- Frost Protection Starts At: _____ °C
- Emergency Close At: _____ °C
- Night-time Closure: YES / NO
- Anti-Condensation Mode: YES / NO

SUMMER SETTINGS:
- Open Temperature: _____ °C
- Close Temperature: _____ °C
- Open Humidity: _____ %

EMERGENCY CONTACTS:
- Local HVAC Service: __________________
- Equipment Supplier: __________________

TESTING DATES:
- System tested: _____ (date)
- Winter tested: _____ (date)
- Snow tested: _____ (date)
```

---

## Further Reading

- [Swedish Greenhouse Standards](https://www.sjv.se) (Statens jordbruksverförvaltning)
- Winter greenhouse management best practices
- Plant frost tolerance data for your crops
- Local weather history for calibration

Your system is now ready for Swedish winters! Start with conservative settings and adjust after observing real conditions.
