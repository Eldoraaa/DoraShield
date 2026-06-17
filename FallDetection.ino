#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Definisi Pin ESP32-C3
#define I2C_SDA 8
#define I2C_SCL 9
#define BUZZER_PIN 3

Adafruit_MPU6050 mpu;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10); // Tunggu Serial Monitor siap

  // Set pin buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Pastikan buzzer mati saat awal

  // Inisialisasi I2C khusus untuk ESP32-C3
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("Mencari MPU6050...");
  
  if (!mpu.begin()) {
    Serial.println("Sensor MPU6050 tidak ditemukan! Cek kabel.");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Ditemukan!");

  // Konfigurasi rentang sensor
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
  // Ambil data terbaru dari sensor
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Tampilkan data akselerasi di Serial Monitor
  Serial.print("Akselerasi X: "); Serial.print(a.acceleration.x);
  Serial.print(" Y: "); Serial.print(a.acceleration.y);
  Serial.print(" Z: "); Serial.print(a.acceleration.z);
  Serial.println(" m/s^2");

  // LOGIKA TRIGGER: Nyalakan buzzer jika dimiringkan/diguncang keras di sumbu X
  // Nilai normal saat diam mendatar adalah sekitar 0. Kita pakai batas > 5 atau < -5
  if (a.acceleration.x > 5.0 || a.acceleration.x < -5.0) {
    Serial.println("ALARM! Kemiringan terdeteksi!");
    digitalWrite(BUZZER_PIN, HIGH); // Nyalakan Buzzer Aktif
  } else {
    digitalWrite(BUZZER_PIN, LOW);  // Matikan Buzzer
  }

  delay(200); // Jeda agar pembacaan tidak terlalu cepat
}