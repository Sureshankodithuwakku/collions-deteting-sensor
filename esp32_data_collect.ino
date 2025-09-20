#include <Wire.h>
#include <MPU6050.h>
#include <SPI.h>
#include <SD.h>

// ---------- Pins (ESP32 VSPI default) ----------
const int SD_SCK  = 18;
const int SD_MISO = 19;
const int SD_MOSI = 23;
const int SD_CS   = 5;    // Change if your module uses a different CS
const int BOOT_BTN = 0;   // On-board BOOT button (active LOW)
const int LED_PIN  = 2;   // On-board LED on many ESP32 dev boards

// ---------- Globals ----------
MPU6050 mpu;
SPIClass spi(VSPI);
File dataFile;

bool isLogging = false;
bool lastBtn = HIGH;
unsigned long lastDebounce = 0;
const unsigned long debounceMs = 50;

uint16_t fileIndex = 0;
char filename[32];

// Flush every N lines to reduce wear but ensure data is saved
const uint16_t FLUSH_EVERY = 10;
uint16_t linesSinceFlush = 0;

// ---------- Helpers ----------
bool openNewSessionFile() {
  // Find next available /imu_XXX.csv
  for (; fileIndex < 1000; fileIndex++) {
    snprintf(filename, sizeof(filename), "/imu_%03u.csv", fileIndex);
    if (!SD.exists(filename)) break;
  }
  if (fileIndex >= 1000) {
    Serial.println("❌ Too many files (imu_000.csv..imu_999.csv already exist).");
    return false;
  }

  dataFile = SD.open(filename, FILE_WRITE);
  if (!dataFile) {
    Serial.printf("❌ Failed to open %s for writing.\n", filename);
    return false;
  }

  dataFile.println("Time(ms),AccX_g,AccY_g,AccZ_g,GyroX_dps,GyroY_dps,GyroZ_dps");
  dataFile.flush();  // ensure header is saved
  linesSinceFlush = 0;

  Serial.printf("✅ Logging to %s\n", filename);
  return true;
}

void startLogging() {
  if (isLogging) return;
  if (!openNewSessionFile()) {
    Serial.println("❌ Cannot start logging: file open failed.");
    return;
  }
  isLogging = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.println("▶️ Logging STARTED");
}

void stopLogging() {
  if (!isLogging) return;
  if (dataFile) {
    dataFile.flush();
    dataFile.close();
  }
  isLogging = false;
  digitalWrite(LED_PIN, LOW);
  Serial.println("⏹️ Logging STOPPED (file closed).");
}

void writeIMUOnce() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getAcceleration(&ax, &ay, &az);
  mpu.getRotation(&gx, &gy, &gz);

  // Convert to g and deg/s (assuming default ±2g, ±250 dps)
  const float ACC_LSB = 16384.0f;
  const float GYR_LSB = 131.0f;

  float accX = ax / ACC_LSB;
  float accY = ay / ACC_LSB;
  float accZ = az / ACC_LSB;

  float gyroX = gx / GYR_LSB;
  float gyroY = gy / GYR_LSB;
  float gyroZ = gz / GYR_LSB;

  if (!dataFile) {
    Serial.println("❌ Data file lost/closed unexpectedly.");
    stopLogging();
    return;
  }

  dataFile.print(millis()); dataFile.print(',');
  dataFile.print(accX, 6);  dataFile.print(',');
  dataFile.print(accY, 6);  dataFile.print(',');
  dataFile.print(accZ, 6);  dataFile.print(',');
  dataFile.print(gyroX, 6); dataFile.print(',');
  dataFile.print(gyroY, 6); dataFile.print(',');
  dataFile.println(gyroZ, 6);

  if (++linesSinceFlush >= FLUSH_EVERY) {
    dataFile.flush();
    linesSinceFlush = 0;
  }
}

// ---------- Setup/Loop ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  pinMode(BOOT_BTN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Init I2C + MPU6050
  Wire.begin();              // SDA=21, SCL=22 by default
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("❌ MPU6050 connection failed. Check wiring and power (3.3V).");
  } else {
    Serial.println("✅ MPU6050 connected.");
  }

  // Init SPI + SD (explicit pins + safer clock)
  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  // Try a moderate SPI clock; some modules/cards are picky. You can increase later.
  if (!SD.begin(SD_CS, spi, 10 * 1000 * 1000)) { // 10 MHz
    Serial.println("❌ SD init failed. Check CS pin wiring, power (3.3V), and card format (FAT32).");
    // Don't halt; you can still see messages
  } else {
    Serial.println("✅ SD initialized.");
    uint8_t type = SD.cardType();
    Serial.print("Card type: ");
    if (type == CARD_MMC) Serial.println("MMC");
    else if (type == CARD_SD) Serial.println("SDSC");
    else if (type == CARD_SDHC) Serial.println("SDHC/SDXC");
    else Serial.println("Unknown");
    Serial.printf("Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
  }

  Serial.println("Press BOOT button to START/STOP logging.");
}

void loop() {
  // Debounced BOOT button toggle (active LOW)
  int reading = digitalRead(BOOT_BTN);
  if (reading != lastBtn) {
    lastDebounce = millis();
  }
  if ((millis() - lastDebounce) > debounceMs) {
    // On press event (HIGH -> LOW)
    static bool prevStable = HIGH;
    bool stable = reading;
    if (prevStable == HIGH && stable == LOW) {
      if (!isLogging) startLogging();
      else            stopLogging();
      delay(180); // extra guard against double-fire
    }
    prevStable = stable;
  }
  lastBtn = reading;

  // Log at your desired rate while active
  if (isLogging) {
    writeIMUOnce();
    delay(100); // ~10 Hz; adjust as needed
  }
}
