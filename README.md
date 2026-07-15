# XIAO ESP32S3 BLE NeoPixel Controller

A BLE-controlled NeoPixel LED controller for the **Seeed Studio XIAO ESP32S3**.

The firmware allows a mobile phone to control predefined LED modes and configure colours over Bluetooth Low Energy (BLE). All user settings are automatically stored in the ESP32's Non-Volatile Storage (NVS), so the selected configuration is restored after power cycling.

---

# Features

* BLE Peripheral
* Mobile phone control using any BLE terminal application
* Configuration stored in ESP32 NVS
* Non-blocking animations using `millis()`
* Simple text-based BLE protocol
* Automatic configuration restore after reboot

---

# Hardware

* Seeed Studio XIAO ESP32S3
* WS2812 / NeoPixel LEDs
* 5V Power Supply (recommended for multiple LEDs)

---

# Libraries

* Adafruit NeoPixel
* NimBLE-Arduino
* Preferences (built into ESP32 Arduino)

---

# BLE Device

Device Name

```
XIAO-NeoPixel
```

---

# BLE Characteristics

## Command Characteristic

Properties

* Write
* Write Without Response

Used to send commands from the mobile application.

---

## Status Characteristic

Properties

* Read
* Notify

Returns the current configuration.

---

# LED Modes

| Mode | Description      |
| ---- | ---------------- |
| 0    | All LEDs OFF     |
| 1    | Breathing Effect |
| 2    | Solid Colour     |

---

# BLE Commands

## Mode Selection

Turn LEDs OFF

```
MODE,0
```

Breathing Mode

```
MODE,1
```

Solid Colour Mode

```
MODE,2
```

---

## Set Breathing Colour

```
BREATH_COLOR,R,G,B
```

Example

```
BREATH_COLOR,255,0,0
```

Sets the breathing colour to red.

Automatically stored in NVS.

---

## Set Solid Colour

```
SOLID_COLOR,R,G,B
```

Example

```
SOLID_COLOR,0,255,255
```

Sets the solid colour to cyan.

Automatically stored in NVS.

---

## Set Brightness

Range

```
0 - 255
```

Example

```
BRIGHTNESS,120
```

Automatically stored in NVS.

---

## Set Breathing Speed

Example

```
BREATH_SPEED,15
```

Lower values produce faster breathing.

Higher values produce slower breathing.

Automatically stored in NVS.

---

## Read Current Configuration

```
STATUS
```

Example Response

```
MODE=1
BREATH_COLOR=255:0:0
SOLID_COLOR=0:255:255
BRIGHTNESS=120
BREATH_SPEED=15
```

---

# Configuration Stored in NVS

The following parameters are saved automatically whenever they are changed.

| Parameter        | Stored |
| ---------------- | ------ |
| Current Mode     | ✓      |
| Breathing Colour | ✓      |
| Solid Colour     | ✓      |
| Brightness       | ✓      |
| Breathing Speed  | ✓      |

The configuration is automatically restored after every restart.

---

# Example Usage

Set breathing colour

```
BREATH_COLOR,0,0,255
```

Start breathing

```
MODE,1
```

Change solid colour

```
SOLID_COLOR,255,120,0
```

Display solid colour

```
MODE,2
```

Turn LEDs off

```
MODE,0
```

---

# Mobile Applications

Any BLE application capable of writing UTF-8 text can be used.

Recommended applications:

* nRF Connect
* LightBlue
* BLE Scanner

---

# PlatformIO Libraries

```ini
lib_deps =
    adafruit/Adafruit NeoPixel
    h2zero/NimBLE-Arduino
```

---

# Future Improvements

* Multiple animation modes
* Rainbow effects
* Color wipe
* Chase effects
* Twinkle effect
* Individual LED control
* LED grouping
* JSON command protocol
* OTA firmware updates
* Custom mobile application
* Multiple presets stored in NVS
* Factory reset command
* Firmware version reporting
* BLE pairing and authentication

---

# License

This project is intended as a starting point for BLE-controlled NeoPixel applications using the Seeed Studio XIAO ESP32S3. Modify and extend it to suit your application.
