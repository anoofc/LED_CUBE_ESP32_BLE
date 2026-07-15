#include <Arduino.h>

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <NimBLEDevice.h>
#include <Preferences.h>

// ============================================================
// Hardware configuration
// ============================================================

#define NEOPIXEL_PIN    26
#define NEOPIXEL_COUNT  16

Adafruit_NeoPixel pixels(
    NEOPIXEL_COUNT,
    NEOPIXEL_PIN,
    NEO_GRB + NEO_KHZ800
);

// ============================================================
// BLE configuration
// ============================================================

#define BLE_DEVICE_NAME        "ESP32-NeoPixel"

#define SERVICE_UUID           "6f83b2a0-5123-4ef5-8e50-f60db04a1000"
#define COMMAND_CHAR_UUID      "6f83b2a0-5123-4ef5-8e50-f60db04a1001"
#define STATUS_CHAR_UUID       "6f83b2a0-5123-4ef5-8e50-f60db04a1002"

NimBLECharacteristic* statusCharacteristic = nullptr;

bool bleConnected = false;

// ============================================================
// NVS
// ============================================================

Preferences preferences;

// ============================================================
// LED modes
// ============================================================

enum LedMode : uint8_t {
    MODE_OFF       = 0,
    MODE_BREATHING = 1,
    MODE_SOLID     = 2
};

// ============================================================
// Configuration
// ============================================================

struct RGBColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct LedConfiguration {
    uint8_t mode;

    RGBColor breathingColor;
    RGBColor solidColor;

    uint8_t brightness;

    // Delay between breathing brightness updates.
    // Lower value = faster breathing.
    uint16_t breathingSpeedMs;
};

LedConfiguration config = {
    MODE_OFF,
    {255, 0, 0},       // Default breathing colour: red
    {0, 100, 255},     // Default solid colour: blue
    255,               // Default maximum brightness
    15                 // Default breathing update interval
};

// ============================================================
// Breathing animation variables
// ============================================================

unsigned long previousBreathingMillis = 0;

int16_t breathingBrightness = 0;
int8_t breathingDirection = 1;

// Prevent repeatedly sending identical LED data in OFF/SOLID mode
bool modeNeedsRefresh = true;

// ============================================================
// Utility functions
// ============================================================

uint32_t makeColor(const RGBColor& color) {
    return pixels.Color(
        color.red,
        color.green,
        color.blue
    );
}

void setAllPixels(uint32_t color) {
    for (uint16_t i = 0; i < NEOPIXEL_COUNT; i++) {
        pixels.setPixelColor(i, color);
    }
}

void resetAnimation() {
    previousBreathingMillis = millis();

    breathingBrightness = 0;
    breathingDirection = 1;

    modeNeedsRefresh = true;
}

// ============================================================
// NVS functions
// ============================================================

void saveModeToNVS() {
    preferences.putUChar("mode", config.mode);
}

void saveBreathingColorToNVS() {
    preferences.putUChar("br_r", config.breathingColor.red);
    preferences.putUChar("br_g", config.breathingColor.green);
    preferences.putUChar("br_b", config.breathingColor.blue);

    Serial.println("Breathing colour saved to NVS");
}

void saveSolidColorToNVS() {
    preferences.putUChar("so_r", config.solidColor.red);
    preferences.putUChar("so_g", config.solidColor.green);
    preferences.putUChar("so_b", config.solidColor.blue);

    Serial.println("Solid colour saved to NVS");
}

void saveBrightnessToNVS() {
    preferences.putUChar("brightness", config.brightness);
}

void saveBreathingSpeedToNVS() {
    preferences.putUShort(
        "br_speed",
        config.breathingSpeedMs
    );
}

void loadConfigurationFromNVS() {
    preferences.begin("neopixel", false);

    config.mode = preferences.getUChar(
        "mode",
        MODE_OFF
    );

    config.breathingColor.red = preferences.getUChar(
        "br_r",
        255
    );

    config.breathingColor.green = preferences.getUChar(
        "br_g",
        0
    );

    config.breathingColor.blue = preferences.getUChar(
        "br_b",
        0
    );

    config.solidColor.red = preferences.getUChar(
        "so_r",
        0
    );

    config.solidColor.green = preferences.getUChar(
        "so_g",
        100
    );

    config.solidColor.blue = preferences.getUChar(
        "so_b",
        255
    );

    config.brightness = preferences.getUChar(
        "brightness",
        120
    );

    config.breathingSpeedMs = preferences.getUShort(
        "br_speed",
        15
    );

    if (config.mode > MODE_SOLID) {
        config.mode = MODE_OFF;
    }

    config.breathingSpeedMs = constrain(
        config.breathingSpeedMs,
        5,
        500
    );

    Serial.println("Configuration loaded from NVS");
}

// ============================================================
// BLE status
// ============================================================

String getStatusString() {
    String status;

    status.reserve(160);

    status += "MODE=";
    status += String(config.mode);

    status += ",BREATH_COLOR=";
    status += String(config.breathingColor.red);
    status += ":";
    status += String(config.breathingColor.green);
    status += ":";
    status += String(config.breathingColor.blue);

    status += ",SOLID_COLOR=";
    status += String(config.solidColor.red);
    status += ":";
    status += String(config.solidColor.green);
    status += ":";
    status += String(config.solidColor.blue);

    status += ",BRIGHTNESS=";
    status += String(config.brightness);

    status += ",BREATH_SPEED=";
    status += String(config.breathingSpeedMs);

    return status;
}

void updateStatusCharacteristic() {
    if (statusCharacteristic == nullptr) {
        return;
    }

    String status = getStatusString();

    statusCharacteristic->setValue(status.c_str());

    if (bleConnected) {
        statusCharacteristic->notify();
    }

    Serial.println(status);
}

// ============================================================
// Command parsing
// ============================================================

bool parseRGBCommand(
    const String& command,
    const char* format,
    RGBColor& outputColor
) {
    int red;
    int green;
    int blue;

    int result = sscanf(
        command.c_str(),
        format,
        &red,
        &green,
        &blue
    );

    if (result != 3) {
        return false;
    }

    outputColor.red = constrain(red, 0, 255);
    outputColor.green = constrain(green, 0, 255);
    outputColor.blue = constrain(blue, 0, 255);

    return true;
}

void processCommand(String command) {
    command.trim();
    command.toUpperCase();

    Serial.print("BLE RX: ");
    Serial.println(command);

    // --------------------------------------------------------
    // Mode selection
    // MODE,0 = OFF
    // MODE,1 = breathing
    // MODE,2 = solid
    // --------------------------------------------------------

    if (command.startsWith("MODE,")) {
        int requestedMode = command.substring(5).toInt();

        if (
            requestedMode >= MODE_OFF &&
            requestedMode <= MODE_SOLID
        ) {
            config.mode = requestedMode;

            saveModeToNVS();
            resetAnimation();
        } else {
            Serial.println("Invalid mode");
        }
    }

    // --------------------------------------------------------
    // Set and save breathing colour
    // BREATH_COLOR,255,0,100
    // --------------------------------------------------------

    else if (command.startsWith("BREATH_COLOR,")) {
        RGBColor newColor;

        if (
            parseRGBCommand(
                command,
                "BREATH_COLOR,%d,%d,%d",
                newColor
            )
        ) {
            config.breathingColor = newColor;

            saveBreathingColorToNVS();
            resetAnimation();
        } else {
            Serial.println("Invalid breathing colour command");
        }
    }

    // --------------------------------------------------------
    // Set and save solid colour
    // SOLID_COLOR,0,150,255
    // --------------------------------------------------------

    else if (command.startsWith("SOLID_COLOR,")) {
        RGBColor newColor;

        if (
            parseRGBCommand(
                command,
                "SOLID_COLOR,%d,%d,%d",
                newColor
            )
        ) {
            config.solidColor = newColor;

            saveSolidColorToNVS();
            modeNeedsRefresh = true;
        } else {
            Serial.println("Invalid solid colour command");
        }
    }

    // --------------------------------------------------------
    // Maximum brightness
    // BRIGHTNESS,120
    // --------------------------------------------------------

    else if (command.startsWith("BRIGHTNESS,")) {
        int requestedBrightness =
            command.substring(11).toInt();

        config.brightness = constrain(
            requestedBrightness,
            0,
            255
        );

        saveBrightnessToNVS();
        resetAnimation();
    }

    // --------------------------------------------------------
    // Breathing update speed
    // BREATH_SPEED,15
    // --------------------------------------------------------

    else if (command.startsWith("BREATH_SPEED,")) {
        int requestedSpeed =
            command.substring(13).toInt();

        config.breathingSpeedMs = constrain(
            requestedSpeed,
            5,
            500
        );

        saveBreathingSpeedToNVS();
        resetAnimation();
    }

    // --------------------------------------------------------
    // Request current status
    // --------------------------------------------------------

    else if (command == "STATUS") {
        updateStatusCharacteristic();
        return;
    }

    else {
        Serial.println("Unknown command");
    }

    updateStatusCharacteristic();
}

// ============================================================
// LED mode functions
// ============================================================

void runOffMode() {
    if (!modeNeedsRefresh) {
        return;
    }

    pixels.setBrightness(0);
    pixels.clear();
    pixels.show();

    modeNeedsRefresh = false;
}

void runSolidMode() {
    if (!modeNeedsRefresh) {
        return;
    }

    pixels.setBrightness(config.brightness);
    setAllPixels(makeColor(config.solidColor));
    pixels.show();

    modeNeedsRefresh = false;
}

void runBreathingMode(unsigned long currentMillis) {
    if (
        currentMillis - previousBreathingMillis <
        config.breathingSpeedMs
    ) {
        return;
    }

    previousBreathingMillis = currentMillis;

    breathingBrightness += breathingDirection;

    if (breathingBrightness >= config.brightness) {
        breathingBrightness = config.brightness;
        breathingDirection = -1;
    }

    if (breathingBrightness <= 0) {
        breathingBrightness = 0;
        breathingDirection = 1;
    }

    pixels.setBrightness(breathingBrightness);

    // Every LED receives the same colour and brightness.
    setAllPixels(makeColor(config.breathingColor));

    pixels.show();
}

// ============================================================
// Main LED controller
// ============================================================

void updateLEDs() {
    unsigned long currentMillis = millis();

    switch (config.mode) {
        case MODE_OFF:
            runOffMode();
            break;

        case MODE_BREATHING:
            runBreathingMode(currentMillis);
            break;

        case MODE_SOLID:
            runSolidMode();
            break;

        default:
            config.mode = MODE_OFF;
            resetAnimation();
            break;
    }
}

// ============================================================
// BLE callbacks
// ============================================================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(
        NimBLEServer* server,
        NimBLEConnInfo& connInfo
    ) override {
        bleConnected = true;

        Serial.println("BLE client connected");
        updateStatusCharacteristic();
    }

    void onDisconnect(
        NimBLEServer* server,
        NimBLEConnInfo& connInfo,
        int reason
    ) override {
        bleConnected = false;

        Serial.print("BLE disconnected. Reason: ");
        Serial.println(reason);

        delay(100);

        bool restarted =
            NimBLEDevice::startAdvertising();

        Serial.println(
            restarted
                ? "BLE advertising restarted"
                : "BLE advertising restart failed"
        );
    }
};

class CommandCallbacks :
    public NimBLECharacteristicCallbacks {

    void onWrite(
        NimBLECharacteristic* characteristic,
        NimBLEConnInfo& connInfo
    ) override {
        std::string receivedValue =
            characteristic->getValue();

        if (receivedValue.empty()) {
            return;
        }

        processCommand(
            String(receivedValue.c_str())
        );
    }
};

// ============================================================
// BLE initialization
// ============================================================

void initializeBLE() {
    Serial.println("Initializing BLE...");

    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Optional: increase transmission power for testing.
    NimBLEDevice::setPower(3);

    NimBLEServer* server = NimBLEDevice::createServer();

    if (server == nullptr) {
        Serial.println("ERROR: Failed to create BLE server");
        return;
    }

    server->setCallbacks(new ServerCallbacks());

    NimBLEService* service =
        server->createService(SERVICE_UUID);

    if (service == nullptr) {
        Serial.println("ERROR: Failed to create BLE service");
        return;
    }

    NimBLECharacteristic* commandCharacteristic =
        service->createCharacteristic(
            COMMAND_CHAR_UUID,
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_NR
        );

    commandCharacteristic->setCallbacks(
        new CommandCallbacks()
    );

    statusCharacteristic =
        service->createCharacteristic(
            STATUS_CHAR_UUID,
            NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY
        );

    statusCharacteristic->setValue("READY");

    /*
     * Do not call:
     *
     * service->start();
     * server->start();
     *
     * NimBLE-Arduino 2.x manages service startup internally.
     */

    NimBLEAdvertising* advertising =
        NimBLEDevice::getAdvertising();

    advertising->setName(BLE_DEVICE_NAME);
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->enableScanResponse(true);

    bool advertisingStarted =
        NimBLEDevice::startAdvertising();

    if (advertisingStarted) {
        Serial.println("BLE advertising started successfully");
        Serial.print("Device name: ");
        Serial.println(BLE_DEVICE_NAME);
    } else {
        Serial.println("ERROR: BLE advertising failed");
    }
}

// ============================================================
// Setup
// ============================================================

void setup() {
    Serial.begin(115200);

    pixels.begin();
    pixels.clear();
    pixels.show();

    loadConfigurationFromNVS();

    resetAnimation();
    initializeBLE();

    Serial.println("XIAO ESP32S3 NeoPixel controller ready");
}

// ============================================================
// Main loop
// ============================================================

void loop() {
    updateLEDs();
}