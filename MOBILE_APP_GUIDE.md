# Mobile App Development Guide

Your web dashboard already works on mobile phones! But here are options to create a more app-like experience.

---

## Option 1: Progressive Web App (PWA) - EASIEST ⭐

A PWA is a website that works offline and can be installed like an app.

### What Your System Already Has:

The built-in web dashboard IS a PWA-ready design:
- Responsive layout (works on mobile)
- Touch-friendly buttons
- Real-time updates
- Beautiful UI

### Make It Installable:

1. **Create `manifest.json`** file (save in same folder as your HTML):

```json
{
  "name": "Orangeri Control System",
  "short_name": "Orangeri",
  "description": "Smart ventilation control for your greenhouse",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#ffffff",
  "theme_color": "#667eea",
  "orientation": "portrait-primary",
  "scope": "/",
  "icons": [
    {
      "src": "/images/icon-192.png",
      "sizes": "192x192",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/images/icon-512.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/images/icon-maskable.png",
      "sizes": "192x192",
      "type": "image/png",
      "purpose": "maskable"
    }
  ],
  "screenshots": [
    {
      "src": "/images/screenshot1.png",
      "sizes": "540x720",
      "type": "image/png",
      "form_factor": "narrow"
    },
    {
      "src": "/images/screenshot2.png",
      "sizes": "1280x720",
      "type": "image/png",
      "form_factor": "wide"
    }
  ]
}
```

2. **Update Your HTML Header** (in the dashboard):

Add these lines inside the `<head>` tag:

```html
<meta name="theme-color" content="#667eea">
<meta name="description" content="Smart orangeri ventilation control">
<link rel="manifest" href="/manifest.json">
<link rel="icon" type="image/png" href="/images/icon-192.png">
<link rel="apple-touch-icon" href="/images/icon-192.png">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="default">
<meta name="apple-mobile-web-app-title" content="Orangeri">
```

3. **Create Service Worker** (`service-worker.js`):

```javascript
const CACHE_NAME = 'orangeri-v1';
const URLS_TO_CACHE = [
  '/',
  '/api/status',
  '/api/thresholds'
];

// Install event - cache files
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => {
      return cache.addAll(URLS_TO_CACHE);
    })
  );
});

// Fetch event - serve from cache, fallback to network
self.addEventListener('fetch', event => {
  event.respondWith(
    caches.match(event.request).then(response => {
      if (response) {
        return response;
      }
      return fetch(event.request).then(response => {
        // Cache new responses
        if (event.request.method === 'GET') {
          const cache = caches.open(CACHE_NAME);
          cache.then(c => c.put(event.request, response.clone()));
        }
        return response;
      });
    })
  );
});

// Activate event - cleanup old caches
self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.map(cacheName => {
          if (cacheName !== CACHE_NAME) {
            return caches.delete(cacheName);
          }
        })
      );
    })
  );
});
```

4. **Register Service Worker** (add to end of your dashboard HTML):

```html
<script>
if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('/service-worker.js')
    .then(registration => console.log('SW registered'))
    .catch(err => console.log('SW registration failed'));
}
</script>
```

5. **Installation on iPhone**:
   - Open Safari
   - Go to http://[your-IP]/
   - Tap share button (↗️)
   - Tap "Add to Home Screen"
   - App now appears on homescreen!

6. **Installation on Android**:
   - Open Chrome
   - Go to http://[your-IP]/
   - Chrome will show "Install" prompt
   - Or use menu → "Install app"
   - App now appears in app drawer!

### Advantages:
- ✓ Works offline (cached data)
- ✓ Installs like native app
- ✓ No app store approval needed
- ✓ Easy to update (automatic cache refresh)
- ✓ iOS and Android support
- ✓ NO CODING REQUIRED - use existing dashboard!

---

## Option 2: Native iOS App

### Easiest: Use App Wrapper (Swift)

Create a minimal native wrapper:

```swift
import UIKit
import WebKit

class ViewController: UIViewController, WKNavigationDelegate {
    var webView: WKWebView!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        let webConfiguration = WKWebViewConfiguration()
        webView = WKWebView(frame: .zero, configuration: webConfiguration)
        webView.navigationDelegate = self
        view = webView
        
        // Load your Orangeri dashboard
        let myURL = URL(string: "http://192.168.1.123/")
        let myRequest = URLRequest(url: myURL!)
        webView.load(myRequest)
    }
}
```

### Better: Use SwiftUI + Combine

```swift
import SwiftUI

struct ContentView: View {
    @State private var systemStatus: SystemStatus?
    @State private var manualMode = false
    
    var body: some View {
        NavigationView {
            VStack {
                // Temperature Display
                if let status = systemStatus {
                    HStack {
                        VStack(alignment: .leading) {
                            Text("Inside")
                                .font(.caption)
                                .foregroundColor(.gray)
                            Text("\(String(format: "%.1f", status.tempInside))°C")
                                .font(.title)
                        }
                        
                        Spacer()
                        
                        VStack(alignment: .trailing) {
                            Text("Outside")
                                .font(.caption)
                                .foregroundColor(.gray)
                            Text("\(String(format: "%.1f", status.tempOutside))°C")
                                .font(.title)
                        }
                    }
                    .padding()
                    .background(Color.blue.opacity(0.1))
                    .cornerRadius(8)
                    
                    // Hatch Controls
                    VStack(spacing: 12) {
                        HatchControlView(hatchName: "Hatch 1", 
                                       state: status.hatch1)
                        HatchControlView(hatchName: "Hatch 2", 
                                       state: status.hatch2)
                    }
                    .padding()
                }
                
                Spacer()
            }
            .navigationTitle("🌱 Orangeri")
            .onAppear {
                fetchStatus()
                Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { _ in
                    fetchStatus()
                }
            }
        }
    }
    
    func fetchStatus() {
        guard let url = URL(string: "http://192.168.1.123/api/status") else { return }
        
        URLSession.shared.dataTask(with: url) { data, response, error in
            if let data = data {
                let decoder = JSONDecoder()
                if let status = try? decoder.decode(SystemStatus.self, from: data) {
                    DispatchQueue.main.async {
                        self.systemStatus = status
                    }
                }
            }
        }.resume()
    }
}

struct HatchControlView: View {
    let hatchName: String
    let state: String
    @State private var isManualMode = false
    
    var body: some View {
        VStack {
            Text(hatchName)
                .font(.headline)
            
            Text(state.uppercased())
                .font(.caption)
                .padding(4)
                .background(stateColor())
                .foregroundColor(.white)
                .cornerRadius(4)
            
            if isManualMode {
                HStack(spacing: 8) {
                    Button(action: { controlHatch(command: "open") }) {
                        Text("Open")
                            .frame(maxWidth: .infinity)
                            .padding(8)
                            .background(Color.green)
                            .foregroundColor(.white)
                            .cornerRadius(4)
                    }
                    
                    Button(action: { controlHatch(command: "close") }) {
                        Text("Close")
                            .frame(maxWidth: .infinity)
                            .padding(8)
                            .background(Color.red)
                            .foregroundColor(.white)
                            .cornerRadius(4)
                    }
                    
                    Button(action: { controlHatch(command: "stop") }) {
                        Text("Stop")
                            .frame(maxWidth: .infinity)
                            .padding(8)
                            .background(Color.gray)
                            .foregroundColor(.white)
                            .cornerRadius(4)
                    }
                }
            }
            
            Toggle("Manual Control", isOn: $isManualMode)
        }
        .padding()
        .background(Color.gray.opacity(0.1))
        .cornerRadius(8)
    }
    
    func stateColor() -> Color {
        switch state.lowercased() {
        case "open": return .green
        case "closed": return .red
        case "slightly_open": return .orange
        default: return .blue
        }
    }
    
    func controlHatch(command: String) {
        guard let url = URL(string: "http://192.168.1.123/api/manual") else { return }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        let payload: [String: String] = ["hatch1": command]
        request.httpBody = try? JSONSerialization.data(withJSONObject: payload)
        
        URLSession.shared.dataTask(with: request).resume()
    }
}

struct SystemStatus: Codable {
    let tempInside: Float
    let tempOutside: Float
    let hatch1: String
    let hatch2: String
    
    enum CodingKeys: String, CodingKey {
        case tempInside = "temp_inside"
        case tempOutside = "temp_outside"
        case hatch1 = "hatch1"
        case hatch2 = "hatch2"
    }
}
```

### Distribution:

1. **TestFlight** (easiest):
   - Upload to App Store Connect
   - Beta test with TestFlight
   - No code signing hassles

2. **App Store**:
   - Requires Apple Developer account ($99/year)
   - Full review process
   - Can take 1-2 weeks

---

## Option 3: Native Android App

### Using Flutter (Cross-Platform)

Flutter works for both iOS and Android with single codebase:

```dart
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';

void main() => runApp(OrangeriApp());

class OrangeriApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Orangeri Control',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        useMaterial3: true,
      ),
      home: DashboardPage(),
    );
  }
}

class DashboardPage extends StatefulWidget {
  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  late SystemStatus status;
  bool manualMode = false;
  
  @override
  void initState() {
    super.initState();
    fetchStatus();
    // Refresh every 5 seconds
    Future.delayed(Duration(seconds: 5), () {
      if (mounted) {
        fetchStatus();
        initState(); // Re-schedule
      }
    });
  }
  
  Future<void> fetchStatus() async {
    try {
      final response = await http.get(
        Uri.parse('http://192.168.1.123/api/status'),
      );
      
      if (response.statusCode == 200) {
        setState(() {
          status = SystemStatus.fromJson(jsonDecode(response.body));
        });
      }
    } catch (e) {
      print('Error fetching status: $e');
    }
  }
  
  Future<void> controlHatch(int hatchNum, String command) async {
    try {
      await http.post(
        Uri.parse('http://192.168.1.123/api/manual'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({
          'hatch$hatchNum': command,
        }),
      );
      await fetchStatus();
    } catch (e) {
      print('Error: $e');
    }
  }
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('🌱 Orangeri Control'),
        centerTitle: true,
      ),
      body: SingleChildScrollView(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Temperature Card
            Card(
              child: Padding(
                padding: EdgeInsets.all(16),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceAround,
                  children: [
                    Column(
                      children: [
                        Text('Inside', style: TextStyle(color: Colors.grey)),
                        Text(
                          '${status.tempInside.toStringAsFixed(1)}°C',
                          style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
                        ),
                      ],
                    ),
                    Column(
                      children: [
                        Text('Outside', style: TextStyle(color: Colors.grey)),
                        Text(
                          '${status.tempOutside.toStringAsFixed(1)}°C',
                          style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            SizedBox(height: 16),
            
            // Hatch Controls
            HatchControlCard(
              hatchName: 'Hatch 1',
              state: status.hatch1,
              onOpen: () => controlHatch(1, 'open'),
              onClose: () => controlHatch(1, 'close'),
              onStop: () => controlHatch(1, 'stop'),
              manualMode: manualMode,
            ),
            SizedBox(height: 16),
            
            HatchControlCard(
              hatchName: 'Hatch 2',
              state: status.hatch2,
              onOpen: () => controlHatch(2, 'open'),
              onClose: () => controlHatch(2, 'close'),
              onStop: () => controlHatch(2, 'stop'),
              manualMode: manualMode,
            ),
            SizedBox(height: 16),
            
            // Manual Mode Toggle
            CheckboxListTile(
              title: Text('Manual Control'),
              value: manualMode,
              onChanged: (bool? value) {
                setState(() => manualMode = value ?? false);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class HatchControlCard extends StatelessWidget {
  final String hatchName;
  final String state;
  final VoidCallback onOpen;
  final VoidCallback onClose;
  final VoidCallback onStop;
  final bool manualMode;
  
  const HatchControlCard({
    required this.hatchName,
    required this.state,
    required this.onOpen,
    required this.onClose,
    required this.onStop,
    required this.manualMode,
  });
  
  @override
  Widget build(BuildContext context) {
    Color stateColor = Colors.grey;
    if (state.contains('open')) stateColor = Colors.green;
    if (state.contains('closed')) stateColor = Colors.red;
    if (state.contains('slightly')) stateColor = Colors.orange;
    
    return Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(hatchName, style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
            SizedBox(height: 8),
            Chip(
              label: Text(state.toUpperCase()),
              backgroundColor: stateColor,
              labelStyle: TextStyle(color: Colors.white),
            ),
            if (manualMode) ...[
              SizedBox(height: 12),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                children: [
                  ElevatedButton.icon(
                    onPressed: onOpen,
                    icon: Icon(Icons.arrow_upward),
                    label: Text('Open'),
                    style: ElevatedButton.styleFrom(backgroundColor: Colors.green),
                  ),
                  ElevatedButton.icon(
                    onPressed: onClose,
                    icon: Icon(Icons.arrow_downward),
                    label: Text('Close'),
                    style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
                  ),
                  ElevatedButton.icon(
                    onPressed: onStop,
                    icon: Icon(Icons.stop),
                    label: Text('Stop'),
                    style: ElevatedButton.styleFrom(backgroundColor: Colors.grey),
                  ),
                ],
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class SystemStatus {
  final double tempInside;
  final double tempOutside;
  final String hatch1;
  final String hatch2;
  
  SystemStatus({
    required this.tempInside,
    required this.tempOutside,
    required this.hatch1,
    required this.hatch2,
  });
  
  factory SystemStatus.fromJson(Map<String, dynamic> json) {
    return SystemStatus(
      tempInside: (json['sensors']['temp_inside'] as num).toDouble(),
      tempOutside: (json['sensors']['temp_outside'] as num).toDouble(),
      hatch1: json['hatches']['hatch1'] as String,
      hatch2: json['hatches']['hatch2'] as String,
    );
  }
}
```

### To use Flutter:

1. Install Flutter SDK: https://flutter.dev/docs/get-started/install
2. Create project: `flutter create orangeri_app`
3. Replace lib/main.dart with code above
4. Run: `flutter run`
5. Build: `flutter build apk` (Android) or `flutter build ios` (iOS)

---

## My Recommendation

### For Quick Deployment ⭐⭐⭐
**Use Option 1: Progressive Web App**
- Already works with your current code
- Works on iOS and Android
- No additional development needed
- Just add manifest + service worker
- Users can install like native app
- Updates automatically

### For Professional Appearance ⭐⭐
**Use Option 3: Flutter App**
- Single codebase for iOS + Android
- Beautiful native UI
- Easy deployment to Play Store
- ~2-3 days of development

### For iOS Purists ⭐
**Use Option 2: SwiftUI**
- Best integration with iOS
- TestFlight for easy distribution
- More complex setup

---

## Comparison Table

| Feature | PWA | Native iOS | Flutter |
|---------|-----|-----------|---------|
| Development Time | 1-2 hours | 1-2 days | 2-3 days |
| iOS Support | ✓ | ✓ | ✓ |
| Android Support | ✓ | ✗ | ✓ |
| Offline Support | ✓ | ✓ | ✓ |
| App Store | ✗ | ✓ | ✓ |
| Push Notifications | ✓ | ✓ | ✓ |
| Maintenance | Easy | Medium | Easy |
| Cost | Free | $99/year | Free |

---

## Push Notifications (Optional but Nice)

To send alerts when frost is detected or system fails:

### Using Firebase Cloud Messaging:

```cpp
// Arduino code to trigger alerts
if (isFrostRisk() && !lastFrostAlert) {
  sendPushNotification("⚠️ Frost Risk", "Temperature below 2°C");
  lastFrostAlert = true;
}

// Send to web/app
void sendPushNotification(String title, String message) {
  // This requires Firebase setup - see Firebase docs
}
```

### Or simpler: Email alerts

```cpp
void sendEmailAlert(String subject, String body) {
  // Connect to SMTP server and send
  // Requires internet connection
  // Easy to implement but slower
}
```

---

## Testing Your App

### PWA Testing (Easiest):

1. On Android:
   - Open Chrome
   - Go to http://[IP]/
   - Menu → "Install app"

2. On iPhone:
   - Open Safari
   - Share → "Add to Home Screen"

3. Verify:
   - App installs
   - Dashboard loads
   - Buttons work
   - Real-time updates work

### Native App Testing:

1. iOS: Use TestFlight
2. Android: Use Google Play Console internal testing

---

## What You Already Have Working ✓

Your system already includes:
- ✓ Responsive web dashboard
- ✓ Real-time sensor updates
- ✓ Manual control buttons
- ✓ Beautiful UI
- ✓ Works on mobile browsers

Just add manifest + service worker and you have a full PWA!

---

## Next Steps

1. **Start with PWA** (1-2 hours) - best time investment
2. **Test thoroughly** on iOS and Android
3. **If needed, move to Flutter** (2-3 days) for app store presence
4. **Set up cloud logging** for historical data
5. **Add push notifications** for critical alerts

Your mobile app is ready! 📱🌱
