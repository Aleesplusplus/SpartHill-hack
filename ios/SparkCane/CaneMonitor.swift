import Combine
import Foundation

/// Decides when to alert: immediately when a new hazard appears, then on a repeat while it stays ahead.
final class CaneMonitor: ObservableObject {
    let bluetooth = CaneBluetooth()
    let alerts = AlertPlayer()

    @Published var soundOn = true { didSet { alerts.soundOn = soundOn } }
    @Published var vibrationOn = true { didSet { alerts.vibrationOn = vibrationOn } }
    @Published var speechOn = true { didSet { alerts.speechOn = speechOn } }

    private var current: Hazard = .floor
    private var lastAlert = Date.distantPast

    init() {
        bluetooth.onReading = { [weak self] reading in self?.handle(reading) }
        bluetooth.onDisconnect = { [weak self] in
            self?.current = .floor
            self?.alerts.say("Cane disconnected")
        }
    }

    private func handle(_ reading: CaneReading) {
        let changed = reading.hazard != current
        current = reading.hazard
        guard reading.hazard.isAlert else { return }

        let now = Date()
        if changed || now.timeIntervalSince(lastAlert) >= reading.hazard.repeatInterval {
            lastAlert = now
            alerts.play(reading.hazard, announce: changed)
        }
    }
}
