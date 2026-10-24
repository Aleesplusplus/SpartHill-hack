# Spark Cane iPhone app

Connects to the cane over Bluetooth LE and warns the user with sound, vibration and speech.

| Hazard     | What it means                     | Sound / vibration         |
|------------|-----------------------------------|---------------------------|
| Dip        | ground 4–15 cm lower (pothole)    | 1 short, mid pitch        |
| Step down  | 15–35 cm lower (curb, stair)      | 2 short, higher pitch     |
| Ledge      | more than 35 cm lower             | 6 rapid, highest pitch    |
| Step up    | 4–30 cm higher (curb, stair)      | 1 long, low pitch         |
| Obstacle   | taller than 30 cm, or very close  | 3 medium                  |
| No signal  | lidar can't see the ground        | 1 soft tick               |

Alerts repeat while the hazard is still ahead, faster for more dangerous ones.

## Set up in Xcode

1. Install **Xcode** from the Mac App Store.
2. Xcode → **File › New › Project › iOS › App**. Product Name `SparkCane`, Interface **SwiftUI**, Language **Swift**. Save it inside this `ios/` folder.
3. In the new project, delete the generated `ContentView.swift` and `SparkCaneApp.swift`, then drag every `.swift` file from `ios/SparkCane/` into the project (tick **Copy items if needed** off, **Add to target** on).
4. Select the project → target **SparkCane**:
   - **Signing & Capabilities** → Team: your Apple ID. Click **+ Capability** → **Background Modes**, tick **Uses Bluetooth LE accessories** and **Audio, AirPlay, and Picture in Picture**.
   - **Info** → add **Privacy - Bluetooth Always Usage Description**: `Connects to your cane to warn you about steps and drops.`
5. Plug in the iPhone, choose it as the run destination, press **Run** (▶). The first time, enable **Settings › Privacy & Security › Developer Mode** on the phone and trust the developer under **Settings › General › VPN & Device Management**.

With a free Apple ID the app stops opening after 7 days; press Run again to renew it.

## Using it

- Power the cane, open the app. It finds the cane (named `SparkCane`) by itself and reconnects if it drops.
- Hold the cane in its walking position over flat ground for a moment so it learns the floor. Tap **Recalibrate floor** after changing how it's held.
- Alerts keep working with the screen locked. Vibration only works while the app is on screen; sound and speech work in the background.
- Set `SENSOR_HEIGHT_CM` in `include/hazard.h` to the measured height of the lidar above the ground for accurate depths.
