#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define I2C_SDA 4
#define I2C_SCL 5
#define BUZZER_PIN 3

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin()) {
    Serial.println("MPU6050 tidak ditemukan! Cek GPIO 4 & 5");
    while (1) delay(10);
  }
  Serial.println("MPU6050 Aktif");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  Serial.println("Node Otak Siap...");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float totalAccel = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );

  Serial.print("Akselerasi Total: ");
  Serial.print(totalAccel);
  Serial.println(" m/s2");

  if (totalAccel > 15.0) {
    Serial.println("[ALARM] Guncangan/Jatuh Terdeteksi!");
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(200);
}
