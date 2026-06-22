# DoraShield

ESP32 firmware for Eldora DoraShield, the wearable fall-detection device focused on detecting strong motion/fall events and triggering a local buzzer alert.

## Hardware
- ESP32 board
- MPU6050 accelerometer/gyroscope
- Buzzer
- Li-ion battery / power module

## Pin configuration
Defined in `FallDetection.ino`:
- I2C SDA: GPIO 4
- I2C SCL: GPIO 5
- Buzzer: GPIO 3

## Firmware features
- Initializes MPU6050 over I2C
- Reads acceleration continuously
- Calculates total acceleration vector
- Detects possible fall/impact when acceleration exceeds threshold
- Activates buzzer during detected fall/impact event
- Prints sensor and alarm status through Serial Monitor

## Fall detection logic
The current firmware checks total acceleration:

```cpp
if (totalAccel > 15.0) {
  digitalWrite(BUZZER_PIN, HIGH);
} else {
  digitalWrite(BUZZER_PIN, LOW);
}
```

Tune the `15.0` threshold after real-world testing to reduce false positives and missed falls.

## Build / flash
Open `FallDetection.ino` in Arduino IDE, select the ESP32 target board, install required libraries, then upload.

## Libraries
- Wire
- Adafruit MPU6050
- Adafruit Sensor
