import AVFoundation
import CoreHaptics
import UIKit

/// One beep and buzz in an alert pattern.
struct Pulse {
    let frequency: Double  // Hz
    let duration: Double   // seconds
    let gap: Double        // silence after this pulse
    let intensity: Float   // vibration strength, 0...1
}

extension Hazard {
    /// Each hazard gets a distinct rhythm and pitch so it can be told apart without looking.
    var pattern: [Pulse] {
        switch self {
        case .dip:
            return [Pulse(frequency: 600, duration: 0.12, gap: 0, intensity: 0.6)]
        case .stepDown:
            return Array(repeating: Pulse(frequency: 800, duration: 0.12, gap: 0.1, intensity: 0.8), count: 2)
        case .ledge:
            return Array(repeating: Pulse(frequency: 1000, duration: 0.08, gap: 0.06, intensity: 1.0), count: 6)
        case .stepUp:
            return [Pulse(frequency: 400, duration: 0.35, gap: 0, intensity: 0.7)]
        case .obstacle:
            return Array(repeating: Pulse(frequency: 500, duration: 0.15, gap: 0.1, intensity: 0.9), count: 3)
        case .noSignal:
            return [Pulse(frequency: 300, duration: 0.06, gap: 0, intensity: 0.4)]
        case .floor, .calibrating:
            return []
        }
    }
}

/// Plays hazard alerts as tones, vibration, and speech.
final class AlertPlayer {
    var soundOn = true
    var vibrationOn = true
    var speechOn = true

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private let format = AVAudioFormat(standardFormatWithSampleRate: 44_100, channels: 1)!
    private let speech = AVSpeechSynthesizer()
    private var haptics: CHHapticEngine?

    init() {
        // .playback keeps alerts audible with the silent switch on and while the screen is locked.
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, options: [.mixWithOthers])
        try? session.setActive(true)

        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: format)
        try? engine.start()

        if CHHapticEngine.capabilitiesForHardware().supportsHaptics {
            haptics = try? CHHapticEngine()
            haptics?.resetHandler = { [weak self] in try? self?.haptics?.start() }
            try? haptics?.start()
        }
    }

    func play(_ hazard: Hazard, announce: Bool) {
        let pulses = hazard.pattern
        guard !pulses.isEmpty else { return }
        if soundOn { playTones(pulses) }
        if vibrationOn { vibrate(pulses) }
        if speechOn && announce { say(hazard.title) }
    }

    func say(_ text: String) {
        speech.stopSpeaking(at: .immediate)
        speech.speak(AVSpeechUtterance(string: text))
    }

    private func playTones(_ pulses: [Pulse]) {
        if !engine.isRunning { try? engine.start() }
        player.stop()
        for pulse in pulses {
            player.scheduleBuffer(tone(frequency: pulse.frequency, duration: pulse.duration))
            if pulse.gap > 0 { player.scheduleBuffer(tone(frequency: 0, duration: pulse.gap)) }
        }
        player.play()
    }

    /// A sine wave with 5 ms fades to avoid clicks; frequency 0 gives silence.
    private func tone(frequency: Double, duration: Double) -> AVAudioPCMBuffer {
        let frames = Int(duration * format.sampleRate)
        let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(frames))!
        buffer.frameLength = AVAudioFrameCount(frames)
        let samples = buffer.floatChannelData![0]
        let fade = max(1, min(frames / 2, Int(0.005 * format.sampleRate)))
        for i in 0..<frames {
            let envelope = Float(min(1.0, Double(min(i, frames - 1 - i)) / Double(fade)))
            samples[i] = frequency == 0 ? 0 : 0.6 * envelope * Float(sin(2 * .pi * frequency * Double(i) / format.sampleRate))
        }
        return buffer
    }

    private func vibrate(_ pulses: [Pulse]) {
        guard let haptics else {
            UINotificationFeedbackGenerator().notificationOccurred(.warning)
            return
        }
        var events: [CHHapticEvent] = []
        var time = 0.0
        for pulse in pulses {
            events.append(CHHapticEvent(
                eventType: .hapticContinuous,
                parameters: [
                    CHHapticEventParameter(parameterID: .hapticIntensity, value: pulse.intensity),
                    CHHapticEventParameter(parameterID: .hapticSharpness, value: 0.6),
                ],
                relativeTime: time,
                duration: pulse.duration))
            time += pulse.duration + pulse.gap
        }
        do {
            try haptics.start()
            let pattern = try CHHapticPattern(events: events, parameters: [])
            try haptics.makePlayer(with: pattern).start(atTime: CHHapticTimeImmediate)
        } catch {
            UINotificationFeedbackGenerator().notificationOccurred(.warning)
        }
    }
}
