/*
 * --------------------------------------------------------------------------
 *  Rogue OS - RogueOS turns the M5Stack Cardputer ADV into an autonomous, 
 *  hand-held RF direction-finding (fox-hunting) and Wi-Fi forensic 
 *  profiling platform. For Cardputer ADV
 *
 *  ETHICAL HACKERS DEN - SECURITY TOOLS SUITE
 *  Website: www.ethicalhackersden.org
 *  GitHub:  https://github.com/M5RogueOps
 *
 *  DISCLAIMER: This software is licensed under the MIT License. 
 *  Under the terms of this license, these credit links must remain 
 *  included within the source code.
 * --------------------------------------------------------------------------
 */
#include <M5Cardputer.h>
#include <WiFi.h>
#include <vector>

// Structure for historical radar point cloud
struct RadarDot {
    float angle;
    int32_t rssi;
    uint8_t lifetime;
};

// Structure for real discovered Wi-Fi targets with Forensic Metadata
struct NetworkTarget {
    String ssid;
    String bssid;
    int32_t rssi;
    int32_t channel;
    wifi_auth_mode_t authmode;
    String vendor;
};

std::vector<RadarDot> radarPoints;
std::vector<NetworkTarget> discoveredNetworks;

// System State
bool isLocked = false;
int selectedIndex = 0;
int viewMode = 0; // 0 = Radar Telemetry, 1 = Forensic Details

String lockedSSID = "NO TARGET";
String lockedBSSID = "";
int32_t lockedChannel = 1;
wifi_auth_mode_t lockedAuth = WIFI_AUTH_OPEN;
String lockedVendor = "Unknown";

// -------------------------------------------------------------------------
// Tracking Metrics & Global Variables (Declared BEFORE function use)
// -------------------------------------------------------------------------
float currentHeading = 0.0;
int32_t currentRSSI = -100;
int32_t maxRSSI = -100;
float bestHeading = 0.0;
uint32_t lastIMUTime = 0;

M5Canvas canvas(&M5Cardputer.Display);

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    // Custom Rogue Boot Screen change to what ever text you want
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(RED);
    M5Cardputer.Display.setCursor(60, 50);
    M5Cardputer.Display.print("ROGUE OS");
    delay(2000); // Let it sit for 2 seconds before loading the radar
    
    // Initialize Wi-Fi in Station Mode (Disconnected)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Initialize BMI270 IMU via M5Unified global object
    M5.Imu.init();
    
    // Setup double buffer
    canvas.createSprite(240, 135);
    canvas.setTextSize(1);
}

// -------------------------------------------------------------------------
// 0. FORENSIC HELPERS: VENDOR LOOKUP & ENCRYPTION STRINGS
// -------------------------------------------------------------------------

String getVendorFromMAC(String bssid) {
    String prefix = bssid.substring(0, 8);
    prefix.toUpperCase();
    
    // Built-in tactical OUI watchlist
    if (prefix == "A4:C1:38" || prefix == "24:6F:28" || prefix == "30:AE:A4") return "Espressif";
    if (prefix == "00:1A:11" || prefix == "3C:5A:B4" || prefix == "F8:8F:CA") return "Google";
    if (prefix == "60:03:08" || prefix == "00:1B:63" || prefix == "A4:D1:D2") return "Apple";
    if (prefix == "00:17:C4" || prefix == "00:26:37" || prefix == "BC:8C:CD") return "Samsung";
    if (prefix == "00:15:00" || prefix == "00:1E:67" || prefix == "34:17:EB") return "Intel";
    if (prefix == "00:00:0C" || prefix == "00:01:42" || prefix == "00:01:96") return "Cisco";
    
    return "Generic/Other";
}

String getAuthString(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
        default: return "SECURED";
    }
}

// -------------------------------------------------------------------------
// 1. RF ENGINE: SCANNING & TARGETED CHANNEL POLLING
// -------------------------------------------------------------------------

void scanAirspace() {
    discoveredNetworks.clear();
    int16_t count = WiFi.scanNetworks(false, true, false, 300);
    
    for (int i = 0; i < count; ++i) {
        NetworkTarget net;
        net.ssid = WiFi.SSID(i).isEmpty() ? "[HIDDEN SSID]" : WiFi.SSID(i);
        net.bssid = WiFi.BSSIDstr(i);
        net.rssi = WiFi.RSSI(i);
        net.channel = WiFi.channel(i);
        net.authmode = WiFi.encryptionType(i); // Properly scoped inside loop!
        net.vendor = getVendorFromMAC(net.bssid);
        
        discoveredNetworks.push_back(net);
    }
    WiFi.scanDelete();
    if (!discoveredNetworks.empty()) selectedIndex = 0;
}

int32_t pollLockedTarget() {
    int16_t count = WiFi.scanNetworks(false, true, false, 120, lockedChannel);
    for (int i = 0; i < count; ++i) {
        if (WiFi.BSSIDstr(i) == lockedBSSID) {
            int32_t rssi = WiFi.RSSI(i);
            WiFi.scanDelete();
            return rssi;
        }
    }
    WiFi.scanDelete();
    return -100; // Signal lost
}

// -------------------------------------------------------------------------
// 2. IMU HEADING INTEGRATION
// -------------------------------------------------------------------------

void updateIMU() {
    M5.Imu.update();
    float gx, gy, gz;
    if (M5.Imu.getGyro(&gx, &gy, &gz)) {
        uint32_t now = micros();
        if (lastIMUTime == 0) {
            lastIMUTime = now;
            return;
        }
        float dt = (now - lastIMUTime) / 1000000.0f;
        lastIMUTime = now;

        currentHeading += (gz * dt);
        
        if (currentHeading >= 360.0f) currentHeading -= 360.0f;
        if (currentHeading < 0.0f) currentHeading += 360.0f;
    }
}

// -------------------------------------------------------------------------
// 3. KEYBOARD CONTROLS
// -------------------------------------------------------------------------

void handleKeyboard() {
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        
        // TAB KEY: Toggle between Radar and Forensic Detail view
        if (status.tab) {
            viewMode = (viewMode + 1) % 2;
            M5Cardputer.Speaker.tone(1000, 30);
        }

        // ENTER: Lock onto selected target
        if (status.enter && !discoveredNetworks.empty() && !isLocked) {
            isLocked = true;
            lockedSSID = discoveredNetworks[selectedIndex].ssid;
            lockedBSSID = discoveredNetworks[selectedIndex].bssid;
            lockedChannel = discoveredNetworks[selectedIndex].channel;
            lockedAuth = discoveredNetworks[selectedIndex].authmode;
            lockedVendor = discoveredNetworks[selectedIndex].vendor;
            
            radarPoints.clear();
            maxRSSI = -100;
            M5Cardputer.Speaker.tone(1200, 100);
        }
        
        // BACKSPACE: Unlock and return to list
        if (status.del && isLocked) {
            isLocked = false;
            lockedSSID = "NO TARGET";
            radarPoints.clear();
            M5Cardputer.Speaker.tone(400, 100);
        }

        // SPACE: Scan airspace (only when unlocked)
        if (status.space && !isLocked) {
            scanAirspace();
            M5Cardputer.Speaker.tone(800, 50);
        }

        // COMMA / PERIOD: Scroll through target list
        for (auto key : status.word) {
            if (!isLocked && !discoveredNetworks.empty()) {
                if (key == ',') {
                    selectedIndex = (selectedIndex - 1 + discoveredNetworks.size()) % discoveredNetworks.size();
                }
                if (key == '.') {
                    selectedIndex = (selectedIndex + 1) % discoveredNetworks.size();
                }
            }
        }
    }
}

// -------------------------------------------------------------------------
// 4. RADAR & TELEMETRY UI RENDERER
// -------------------------------------------------------------------------

void renderUI() {
    canvas.startWrite();
    canvas.fillSprite(BLACK);

    // --- LEFT COLUMN: POLAR RADAR ---
    int rX = 68, rY = 68, maxR = 55;
    canvas.drawCircle(rX, rY, maxR, canvas.color565(0, 100, 0));
    canvas.drawCircle(rX, rY, maxR / 2, canvas.color565(0, 50, 0));
    canvas.drawFastHLine(rX - maxR, rY, maxR * 2, canvas.color565(0, 40, 0));
    canvas.drawFastVLine(rX, rY - maxR, maxR * 2, canvas.color565(0, 40, 0));

    if (isLocked) {
        for (auto it = radarPoints.begin(); it != radarPoints.end();) {
            float norm = constrain((it->rssi + 100) / 60.0f, 0.0f, 1.0f);
            int dist = norm * maxR;
            float rad = it->angle * DEG_TO_RAD;
            int px = rX + (dist * sin(rad));
            int py = rY - (dist * cos(rad));
            
            canvas.fillCircle(px, py, 2, canvas.color565(0, it->lifetime, 0));
            it->lifetime -= 3;
            if (it->lifetime <= 10) it = radarPoints.erase(it);
            else ++it;
        }

        if (maxRSSI > -100) {
            float bRad = bestHeading * DEG_TO_RAD;
            canvas.fillCircle(rX + (maxR * sin(bRad)), rY - (maxR * cos(bRad)), 3, RED);
        }
    }

    float sRad = currentHeading * DEG_TO_RAD;
    canvas.drawLine(rX, rY, rX + (maxR * sin(sRad)), rY - (maxR * cos(sRad)), WHITE);

    // --- RIGHT COLUMN: MULTI-PAGE DASHBOARD ---
    int tX = 138;
    canvas.setTextColor(WHITE);
    
    if (!isLocked) {
        canvas.drawString("[SELECT TARGET]", tX, 6);
        canvas.drawFastHLine(tX, 16, 96, canvas.color565(80, 80, 80));
        
        if (discoveredNetworks.empty()) {
            canvas.setTextColor(YELLOW);
            canvas.drawString("Press SPACE", tX, 26);
            canvas.drawString("to Scan Air", tX, 38);
        } else {
            for (int i = 0; i < min((int)discoveredNetworks.size(), 4); ++i) {
                int idx = (selectedIndex + i) % discoveredNetworks.size();
                canvas.setTextColor(i == 0 ? GREEN : canvas.color565(120, 120, 120));
                canvas.drawString(i == 0 ? "> " + discoveredNetworks[idx].ssid : "  " + discoveredNetworks[idx].ssid, tX, 24 + (i * 14));
            }
            canvas.setTextColor(WHITE);
            canvas.drawString("ENTER to Lock", tX, 118);
        }
    } else {
        // We are LOCKED ON: Check which page view is active
        if (viewMode == 0) {
            // PAGE 0: RADAR TELEMETRY
            canvas.drawString("[RADAR VIEW]", tX, 6);
            canvas.drawFastHLine(tX, 16, 96, canvas.color565(80, 80, 80));
            
            canvas.setTextColor(YELLOW);
            canvas.drawString(lockedSSID.substring(0, 12), tX, 22);
            canvas.setTextColor(canvas.color565(150, 150, 150));
            canvas.drawString("CH: " + String(lockedChannel), tX, 34);

            canvas.setTextColor(GREEN);
            canvas.setCursor(tX, 50);
            canvas.printf("RSSI: %d dBm", currentRSSI);
            canvas.setCursor(tX, 62);
            canvas.printf("HEAD: %3.0f deg", currentHeading);

            canvas.drawFastHLine(tX, 76, 96, canvas.color565(50, 50, 50));
            canvas.setTextColor(RED);
            canvas.drawString("PEAK VECTOR", tX, 84);
            
            canvas.setTextColor(WHITE);
            canvas.setCursor(tX, 98);
            canvas.printf("MAX: %d dBm", maxRSSI);
            canvas.setCursor(tX, 110);
            canvas.printf("BEAR: %d deg", (int)bestHeading);
        } else {
            // PAGE 1: FORENSIC DETAILS
            canvas.drawString("[TARGET DATA]", tX, 6);
            canvas.drawFastHLine(tX, 16, 96, canvas.color565(80, 80, 80));
            
            canvas.setTextColor(YELLOW);
            canvas.drawString(lockedSSID.substring(0, 12), tX, 22);
            
            canvas.setTextColor(WHITE);
            canvas.drawString("MAC / BSSID:", tX, 36);
            canvas.setTextColor(GREEN);
            canvas.drawString(lockedBSSID, tX, 46);
            
            canvas.setTextColor(WHITE);
            canvas.drawString("VENDOR:", tX, 60);
            canvas.setTextColor(CYAN);
            canvas.drawString(lockedVendor, tX, 70);
            
            canvas.setTextColor(WHITE);
            canvas.drawString("SECURITY:", tX, 84);
            canvas.setTextColor(ORANGE);
            canvas.drawString(getAuthString(lockedAuth), tX, 94);
            
            canvas.setTextColor(WHITE);
            canvas.setCursor(tX, 110);
            canvas.printf("SNR: ~%d dB", constrain(currentRSSI + 95, 0, 60)); // Est. against -95dBm floor
        }
    }

    canvas.drawFastVLine(133, 0, 135, canvas.color565(80, 80, 80));
    canvas.pushSprite(0, 0);
    canvas.endWrite();
}

// -------------------------------------------------------------------------
// 5. MAIN LOOP
// -------------------------------------------------------------------------

void loop() {
    M5Cardputer.update();
    handleKeyboard();
    updateIMU();

    static uint32_t lastSample = 0;
    if (isLocked && millis() - lastSample > 150) {
        lastSample = millis();
        currentRSSI = pollLockedTarget();
        
        if (currentRSSI > -100) {
            radarPoints.push_back({currentHeading, currentRSSI, 255});
            if (currentRSSI > maxRSSI) {
                maxRSSI = currentRSSI;
                bestHeading = currentHeading;
            }
            int pitch = map(constrain(currentRSSI, -90, -30), -90, -30, 400, 2000);
            M5Cardputer.Speaker.tone(pitch, 20);
        }
    }

    renderUI();
    delay(15);
}
