import SwiftUI

struct ContentView: View {
    @ObservedObject var monitor: CaneMonitor
    @ObservedObject var bluetooth: CaneBluetooth

    private var hazard: Hazard { bluetooth.reading?.hazard ?? .floor }

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(bluetooth.status == .connected ? hazard.title : bluetooth.status.rawValue)
                            .font(.largeTitle.bold())
                        if let reading = bluetooth.reading, reading.hazard.isAlert, reading.hazard != .noSignal {
                            Text("\(abs(reading.depthCm)) cm \(reading.depthCm > 0 ? "down" : "up"), about \(reading.aheadCm) cm ahead")
                                .font(.title3)
                        }
                        if let reading = bluetooth.reading {
                            Text("Sensor: \(reading.distanceCm) cm")
                                .foregroundStyle(.secondary)
                        }
                    }
                    .padding(.vertical, 12)
                    .accessibilityElement(children: .combine)
                    .listRowBackground(hazard.isAlert ? Color.red.opacity(0.25) : Color.green.opacity(0.2))
                }

                Section("Alerts") {
                    Toggle("Sound", isOn: $monitor.soundOn)
                    Toggle("Vibration", isOn: $monitor.vibrationOn)
                    Toggle("Spoken warnings", isOn: $monitor.speechOn)
                }

                Section {
                    Button("Recalibrate floor") { bluetooth.recalibrate() }
                        .disabled(bluetooth.status != .connected)
                } footer: {
                    Text("Hold the cane in its normal walking position over flat ground, then tap.")
                }

                Section("Preview alerts") {
                    ForEach(Hazard.alertCases, id: \.self) { hazard in
                        Button(hazard.title) { monitor.alerts.play(hazard, announce: true) }
                    }
                }
            }
            .navigationTitle("Spark Cane")
        }
    }
}
