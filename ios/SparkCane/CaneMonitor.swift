import Combine
import Foundation

/// Decides when to alert: immediately when a new hazard appears, then on a repeat while it stays ahead.
final class CaneMonitor: ObservableObject {
    let bluetooth = CaneBluetooth()
    let alerts = AlertPlayer()

    @Published var soundOn = true { didSet { alerts.soundOn = soundOn } }
    @Published var vibrationOn = true { didSet { alerts.vibrationOn = vibrationOn } }
    @Published var speechOn = true { didSet { alerts.speechOn = speechOn } }

    /// Alerts never run closer together than this, unless the danger gets worse.
    static let minimumAlertGap: TimeInterval = 1.0
    static let minimumSpeechGap: TimeInterval = 2.5

    private var current: Hazard = .floor
    private var lastAlert = Date.distantPast
    private var lastSpeech = Date.distantPast

    init() {
        bluetooth.onReading = { [weak self] reading in self?.handle(reading) }
        bluetooth.onDisconnect = { [weak self] in
            self?.current = .floor
            self?.alerts.say("Cane disconnected")
        }
    }

    private func handle(_ reading: CaneReading) {
        let hazard = reading.hazard
        let changed = hazard != current
        let worse = hazard.severity > current.severity
        current = hazard
        guard hazard.isAlert else { return }

        let now = Date()
        let gap = changed ? Self.minimumAlertGap : hazard.repeatInterval
        guard worse || now.timeIntervalSince(lastAlert) >= gap else { return }
        lastAlert = now

        let announce = changed && (worse || now.timeIntervalSince(lastSpeech) >= Self.minimumSpeechGap)
        if announce { lastSpeech = now }
        alerts.play(hazard, announce: announce)
    }
}
