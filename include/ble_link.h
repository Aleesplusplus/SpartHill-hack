#pragma once
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>

// Must match CaneBluetooth.swift in the iPhone app.
#define CANE_DEVICE_NAME "SparkCane"
#define CANE_SERVICE_UUID "6e3c0001-8b1f-4c7a-9d2e-5a1b2c3d4e5f"
#define HAZARD_CHAR_UUID "6e3c0002-8b1f-4c7a-9d2e-5a1b2c3d4e5f"   // notify: "HAZARD,depth,ahead,distance"
#define COMMAND_CHAR_UUID "6e3c0003-8b1f-4c7a-9d2e-5a1b2c3d4e5f"  // write: "CAL" to recalibrate

static BLECharacteristic *hazardCharacteristic = nullptr;
static volatile bool phoneConnected = false;
static volatile bool recalibrateRequested = false;

class CaneServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override { phoneConnected = true; }
    void onDisconnect(BLEServer *server) override {
        phoneConnected = false;
        BLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) override {
        if (characteristic->getValue() == "CAL") recalibrateRequested = true;
    }
};

inline void beginBle() {
    BLEDevice::init(CANE_DEVICE_NAME);
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new CaneServerCallbacks());

    BLEService *service = server->createService(CANE_SERVICE_UUID);
    hazardCharacteristic = service->createCharacteristic(
        HAZARD_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    hazardCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *command = service->createCharacteristic(
        COMMAND_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    command->setCallbacks(new CommandCallbacks());
    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(CANE_SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

inline void sendHazard(const char *message) {
    if (!phoneConnected) return;
    hazardCharacteristic->setValue((uint8_t *)message, strlen(message));
    hazardCharacteristic->notify();
}
