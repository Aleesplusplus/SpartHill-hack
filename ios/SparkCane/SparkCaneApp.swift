import SwiftUI

@main
struct SparkCaneApp: App {
    @StateObject private var monitor = CaneMonitor()

    var body: some Scene {
        WindowGroup {
            ContentView(monitor: monitor, bluetooth: monitor.bluetooth)
                .onAppear { UIApplication.shared.isIdleTimerDisabled = true }
        }
    }
}
