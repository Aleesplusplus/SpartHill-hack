import Foundation

/// Hazard names sent by the cane firmware (include/hazard.h).
enum Hazard: String, CaseIterable {
    case calibrating = "CALIBRATING"
    case floor = "FLOOR"
    case dip = "DIP"
    case stepDown = "STEP_DOWN"
    case ledge = "LEDGE"
    case stepUp = "STEP_UP"
    case obstacle = "OBSTACLE"
    case noSignal = "NO_SIGNAL"

    static let alertCases: [Hazard] = [.dip, .stepDown, .ledge, .stepUp, .obstacle, .noSignal]

    var title: String {
        switch self {
        case .calibrating: return "Calibrating"
        case .floor: return "Path clear"
        case .dip: return "Dip"
        case .stepDown: return "Step down"
        case .ledge: return "Ledge"
        case .stepUp: return "Step up"
        case .obstacle: return "Obstacle"
        case .noSignal: return "No sensor signal"
        }
    }

    var isAlert: Bool { self != .floor && self != .calibrating }

    /// Higher is more dangerous; matches hazardSeverity() in the firmware.
    var severity: Int {
        switch self {
        case .dip: return 1
        case .stepUp: return 2
        case .stepDown: return 3
        case .obstacle: return 4
        case .ledge: return 5
        default: return 0
        }
    }

    /// How often the alert repeats while the hazard is still ahead.
    var repeatInterval: TimeInterval {
        switch self {
        case .ledge: return 1.0
        case .obstacle, .stepDown: return 1.5
        case .dip, .stepUp: return 2.5
        default: return 4.0
        }
    }
}

/// One message from the cane: "HAZARD,depthCm,aheadCm,distanceCm".
struct CaneReading {
    let hazard: Hazard
    let depthCm: Int     // positive = ground is lower than the floor, negative = higher
    let aheadCm: Int     // horizontal distance to the spot being measured
    let distanceCm: Int  // raw lidar distance

    init?(message: String) {
        let parts = message.split(separator: ",").map(String.init)
        guard parts.count == 4,
              let hazard = Hazard(rawValue: parts[0]),
              let depth = Int(parts[1]),
              let ahead = Int(parts[2]),
              let distance = Int(parts[3]) else { return nil }
        self.hazard = hazard
        depthCm = depth
        aheadCm = ahead
        distanceCm = distance
    }
}
