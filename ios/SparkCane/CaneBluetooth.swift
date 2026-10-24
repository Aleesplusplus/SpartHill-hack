import Combine
import CoreBluetooth

/// Finds the cane over Bluetooth LE, keeps it connected, and publishes its readings.
final class CaneBluetooth: NSObject, ObservableObject {
    // Must match include/ble_link.h in the firmware.
    static let serviceUUID = CBUUID(string: "6e3c0001-8b1f-4c7a-9d2e-5a1b2c3d4e5f")
    static let hazardUUID = CBUUID(string: "6e3c0002-8b1f-4c7a-9d2e-5a1b2c3d4e5f")
    static let commandUUID = CBUUID(string: "6e3c0003-8b1f-4c7a-9d2e-5a1b2c3d4e5f")

    enum Status: String {
        case off = "Bluetooth is off"
        case unauthorized = "Allow Bluetooth in Settings › Spark Cane"
        case unsupported = "Bluetooth not available (the Simulator can't use it; run on a real iPhone)"
        case searching = "Searching for cane…"
        case connecting = "Connecting…"
        case connected = "Connected"
    }

    @Published private(set) var status: Status = .off
    @Published private(set) var reading: CaneReading?

    var onReading: ((CaneReading) -> Void)?
    var onDisconnect: (() -> Void)?

    private var central: CBCentralManager!
    private var cane: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func recalibrate() {
        guard let cane, let commandCharacteristic else { return }
        cane.writeValue(Data("CAL".utf8), for: commandCharacteristic, type: .withResponse)
    }

    private func startScan() {
        status = .searching
        central.scanForPeripherals(withServices: [Self.serviceUUID])
    }
}

extension CaneBluetooth: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn: startScan()
        case .unauthorized: status = .unauthorized
        case .unsupported: status = .unsupported
        default: status = .off
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        cane = peripheral
        central.stopScan()
        status = .connecting
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        peripheral.discoverServices([Self.serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        startScan()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        commandCharacteristic = nil
        reading = nil
        status = .connecting
        onDisconnect?()
        // A pending connect never times out, so this reconnects as soon as the cane is back in range.
        central.connect(peripheral)
    }
}

extension CaneBluetooth: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else { return }
        peripheral.discoverCharacteristics([Self.hazardUUID, Self.commandUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == Self.hazardUUID {
                peripheral.setNotifyValue(true, for: characteristic)
            } else if characteristic.uuid == Self.commandUUID {
                commandCharacteristic = characteristic
            }
        }
        status = .connected
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value,
              let message = String(data: data, encoding: .utf8),
              let reading = CaneReading(message: message) else { return }
        self.reading = reading
        onReading?(reading)
    }
}
