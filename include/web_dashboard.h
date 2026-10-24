#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "ble_link.h"
#include "hazard.h"

// The board makes its own WiFi network, so the demo works anywhere.
// Join it from a laptop or phone and open http://192.168.4.1
#define DASHBOARD_SSID "SparkCane-Demo"
#define DASHBOARD_PASSWORD "sparkcane"  // at least 8 characters

// Press the board's BOOT button to turn WiFi on or off. The blue LED shows when it's on.
const bool DASHBOARD_ON_AT_START = true;
const int WIFI_BUTTON_PIN = 0;   // BOOT button
const int WIFI_LED_PIN = 2;      // onboard LED
static bool dashboardOn = false;

static WebServer dashboardServer(80);
static HazardState dashboardState = {CALIBRATING, 0, 0, 0};
static uint16_t dashboardDistanceCm = 0;

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Spark Cane Live</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--text:#16181d;--muted:#5d6470;--line:#e2e5ea;--ok:#1f9d55;--warn:#d97706;--bad:#dc2626;--accent:#2563eb}
@media (prefers-color-scheme:dark){:root{--bg:#0f1115;--card:#181b21;--text:#eef0f3;--muted:#9aa3af;--line:#2a2f37;--ok:#34d399;--warn:#fbbf24;--bad:#f87171;--accent:#60a5fa}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,-apple-system,sans-serif}
main{max-width:860px;margin:0 auto;padding:20px 16px}
h1{font-size:18px;margin:0 0 16px;color:var(--muted);font-weight:600}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:18px;margin-bottom:14px}
#hazard{font-size:clamp(36px,9vw,64px);font-weight:800;margin:0}
#detail{color:var(--muted);margin-top:4px;min-height:1.4em}
.status{border-left:10px solid var(--ok)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px}
.stat b{display:block;font-size:28px;font-variant-numeric:tabular-nums}.stat span{color:var(--muted);font-size:14px}
canvas{width:100%;height:220px;display:block}
.legend{display:flex;gap:16px;color:var(--muted);font-size:14px;margin-top:8px}
.legend i{display:inline-block;width:12px;height:3px;margin-right:6px;vertical-align:middle}
button{font:inherit;font-weight:600;padding:10px 16px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer}
#conn{float:right;font-size:14px;font-weight:500}
</style></head><body><main>
<h1>Spark Cane live <span id="conn">connecting…</span></h1>
<div class="card status" id="statusCard"><p id="hazard">—</p><div id="detail"></div></div>
<div class="grid">
<div class="card stat"><b id="dist">—</b><span>Lidar distance</span></div>
<div class="card stat"><b id="floor">—</b><span>Learned floor</span></div>
<div class="card stat"><b id="depth">—</b><span>Ground change</span></div>
<div class="card stat"><b id="ahead">—</b><span>Distance ahead</span></div>
</div>
<div class="card"><canvas id="chart"></canvas>
<div class="legend"><span><i style="background:var(--accent)"></i>Lidar distance</span><span><i style="background:var(--muted)"></i>Floor</span></div></div>
<div class="card"><button id="cal">Recalibrate floor</button> <span id="phone" style="color:var(--muted);margin-left:8px"></span></div>
</main><script>
const names={CALIBRATING:"Calibrating",FLOOR:"Path clear",DIP:"Dip",STEP_DOWN:"Step down",LEDGE:"Ledge",STEP_UP:"Step up",OBSTACLE:"Obstacle",NO_SIGNAL:"No sensor signal"};
const level={FLOOR:"ok",CALIBRATING:"warn",DIP:"warn",STEP_UP:"warn",NO_SIGNAL:"warn",STEP_DOWN:"bad",LEDGE:"bad",OBSTACLE:"bad"};
const css=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const $=id=>document.getElementById(id);
const hist=[];const MAX=150;
function draw(){const c=$("chart"),r=devicePixelRatio||1,w=c.clientWidth,h=c.clientHeight;c.width=w*r;c.height=h*r;
const g=c.getContext("2d");g.scale(r,r);g.clearRect(0,0,w,h);if(hist.length<2)return;
let hi=Math.max(...hist.map(p=>Math.max(p.d,p.f)))*1.15||100;
g.strokeStyle=css("--line");g.fillStyle=css("--muted");g.font="12px system-ui";g.lineWidth=1;
for(let i=0;i<=4;i++){const y=h-h*i/4;g.beginPath();g.moveTo(0,y);g.lineTo(w,y);g.stroke();if(i)g.fillText(Math.round(hi*i/4)+" cm",4,y+13)}
const line=(k,col,wd)=>{g.strokeStyle=col;g.lineWidth=wd;g.beginPath();hist.forEach((p,i)=>{const x=w*i/(MAX-1),y=h-h*p[k]/hi;i?g.lineTo(x,y):g.moveTo(x,y)});g.stroke()};
line("f",css("--muted"),1.5);line("d",css("--accent"),2.5)}
async function tick(){try{const s=await(await fetch("/data",{cache:"no-store"})).json();
$("conn").textContent="live";$("hazard").textContent=names[s.hazard]||s.hazard;
$("statusCard").style.borderLeftColor=css("--"+(level[s.hazard]||"warn"));
const alert=!["FLOOR","CALIBRATING","NO_SIGNAL"].includes(s.hazard);
$("detail").textContent=alert?`${Math.abs(s.depth)} cm ${s.depth>0?"down":"up"}, about ${s.ahead} cm ahead`:"";
$("dist").textContent=s.dist+" cm";$("floor").textContent=s.floor.toFixed(0)+" cm";
$("depth").textContent=(s.depth>0?"+":"")+s.depth+" cm";$("ahead").textContent=s.ahead+" cm";
$("phone").textContent=s.phone?"iPhone app connected":"iPhone app not connected";
hist.push({d:s.dist,f:s.floor});if(hist.length>MAX)hist.shift();draw()}
catch(e){$("conn").textContent="disconnected"}setTimeout(tick,150)}
$("cal").onclick=()=>fetch("/cal",{method:"POST"});addEventListener("resize",draw);tick();
</script></body></html>)HTML";

inline void startDashboard() {
    WiFi.softAP(DASHBOARD_SSID, DASHBOARD_PASSWORD);
    dashboardServer.begin();
    dashboardOn = true;
    digitalWrite(WIFI_LED_PIN, HIGH);
    Serial.printf("WiFi on: join \"%s\" (password \"%s\") and open http://%s\n",
                  DASHBOARD_SSID, DASHBOARD_PASSWORD, WiFi.softAPIP().toString().c_str());
}

inline void stopDashboard() {
    dashboardServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    dashboardOn = false;
    digitalWrite(WIFI_LED_PIN, LOW);
    Serial.println("WiFi off");
}

inline void beginDashboard() {
    pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
    pinMode(WIFI_LED_PIN, OUTPUT);

    dashboardServer.on("/", [] { dashboardServer.send_P(200, "text/html", DASHBOARD_HTML); });
    dashboardServer.on("/data", [] {
        char json[160];
        snprintf(json, sizeof(json),
                 "{\"hazard\":\"%s\",\"dist\":%u,\"floor\":%.1f,\"depth\":%d,\"ahead\":%u,\"phone\":%s}",
                 hazardName(dashboardState.hazard), dashboardDistanceCm, dashboardState.baselineCm,
                 dashboardState.depthCm, dashboardState.aheadCm, phoneConnected ? "true" : "false");
        dashboardServer.send(200, "application/json", json);
    });
    dashboardServer.on("/cal", HTTP_POST, [] {
        recalibrateRequested = true;
        dashboardServer.send(204);
    });

    if (DASHBOARD_ON_AT_START) startDashboard();
}

inline void updateDashboard(const HazardState &state, uint16_t distanceCm) {
    dashboardState = state;
    dashboardDistanceCm = distanceCm;
}

inline void handleDashboard() {
    static bool wasPressed = false;
    static uint32_t lastToggleMs = 0;
    bool pressed = digitalRead(WIFI_BUTTON_PIN) == LOW;
    if (pressed && !wasPressed && millis() - lastToggleMs > 300) {
        lastToggleMs = millis();
        dashboardOn ? stopDashboard() : startDashboard();
    }
    wasPressed = pressed;

    if (dashboardOn) dashboardServer.handleClient();
}
