#include "model.h"
#include <LSM6DS3.h> // The library you were using
#include <Wire.h> // Required for I2C communication

#define RED_LED   LED_BUILTIN
#define GREEN_LED 6
#define BLUE_LED  5
// Define the number of features your model expects
const int NUM_FEATURES = 1206;
float sensor_data[NUM_FEATURES];

// Create an instance of the RandomForest class for the model
Eloquent::ML::Port::RandomForest model;

// Create an instance of the LSM6DS3 IMU using your working initialization method
LSM6DS3 myIMU(I2C_MODE, 0x6A);

// Function to collect data from the IMU
void collectData() {
    Serial.println("Collecting data...");
    
    for (int i = 0; i < 201; i++) {
        // Read accelerometer and gyroscope values from the IMU using your library's functions
        float accX = myIMU.readFloatAccelX();
        float accY = myIMU.readFloatAccelY();
        float accZ = myIMU.readFloatAccelZ();
        float gyroX = myIMU.readFloatGyroX();
        float gyroY = myIMU.readFloatGyroY();
        float gyroZ = myIMU.readFloatGyroZ();

        // Store the values in the flattened array
        sensor_data[i * 6 + 0] = accX;
        sensor_data[i * 6 + 1] = accY;
        sensor_data[i * 6 + 2] = accZ;
        sensor_data[i * 6 + 3] = gyroX;
        sensor_data[i * 6 + 4] = gyroY;
        sensor_data[i * 6 + 5] = gyroZ;

        delay(20); // Adjust this delay to match your original sampling rate
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    while (!Serial);
    // Initialize the IMU sensor using your working code
    if (myIMU.begin() != 0) {
        Serial.println("Failed to initialize IMU!");
        while (1);
    } else {
        Serial.println("IMU initialized successfully!");
    }
}

void loop() {
    collectData();
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    // Call the predict method on the 'model' object
    int prediction = model.predict(sensor_data);

    // Interpret the prediction and print the state
    if (prediction == 0) {
        Serial.println("State: Normal");
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        digitalWrite(BLUE_LED, LOW);
    } else if (prediction == 1) {
        Serial.println("State: Accident");
        digitalWrite(RED_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(BLUE_LED, LOW);
    } else if (prediction == 2) {
        Serial.println("State: Hit");
        digitalWrite(BLUE_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, LOW);
    }
    
    delay(200);
}