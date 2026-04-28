#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>

/* ==================================================================
 * 1. CONFIGURATION & CONSTANTS
 * ================================================================== */
const char* SSID = "Galaxy A35 5G A169";
const char* PASS = "12345678";

#define SENSOR_PIN   35
#define LCD_ADDR     0x27
#define SAMPLE_MS    20       // 50 Hz sampling rate

// --- Finger Detection Thresholds ---
#define FINGER_LO    500      // Minimum valid ADC reading
#define FINGER_HI    3500     // Maximum valid ADC reading
#define AMP_MIN      10       // Minimum signal amplitude to be valid
#define CONFIRM_N    6        // Required consecutive valid samples

// --- Beat Detection Parameters ---
#define BASE_ALPHA   0.995f   // Slow DC baseline smoothing factor
#define PEAK_D       12       // Threshold above baseline to trigger a beat
#define HYST_PCT     0.25f    // Falling-edge hysteresis (percentage of amplitude)
#define MIN_IBI      360      // ~167 BPM max limit
#define MAX_IBI      1500     // 40 BPM min limit
#define BEAT_TO      2500     // Silence timeout (ms) to reset system

// --- Signal Averaging ---
#define IBI_N        8        // Number of intervals to average for BPM
#define MA_N         5        // Moving average filter size
#define BPM_BLEND    0.20f    // Exponential smoothing for final BPM display

/* ==================================================================
 * 2. GLOBAL VARIABLES & OBJECTS
 * ================================================================== */
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
WebServer         srv(80);

// --- Core State ---
int   bpm        = 0;
bool  fingerOn   = false;
float baseline   = 512.0f;
bool  aboveT     = false;
unsigned long lastBeat = 0;

// --- Arrays for Averaging ---
int ibiRing[IBI_N] = {0}; int iHead = 0; int iCount = 0;
int maRing[MA_N]   = {0}; int mHead = 0; int mSum   = 0;
bool maFull = false;

// --- Signal Amplitude Tracking ---
int sMin = 4095; 
int sMax = 0;
int confCnt = 0; 
int lastFilt = 0;

// --- Timers ---
unsigned long ampTimer = 0, tSmp = 0, tLcd = 0, tDbg = 0;

// Custom Heart Icon for LCD
byte heartGlyph[8] = {0, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0, 0};

/* ==================================================================
 * 3. WEB INTERFACE (HTML/CSS)
 * ================================================================== */
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Heart Rate Monitor</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', sans-serif; background: #0b0e14; color: #ffffff; padding: 20px; display: flex; flex-direction: column; align-items: center; }
  header { margin-bottom: 25px; text-align: center; }
  h1 { font-size: 2rem; letter-spacing: 4px; margin-bottom: 5px; color: #e2e8f0; }
  .subtitle { color: #64748b; font-size: 0.9rem; letter-spacing: 1px; }
  .top-row { display: flex; gap: 20px; width: 100%; max-width: 900px; margin-bottom: 20px; }
  .card { background: #111827; border: 1px solid #334155; border-radius: 20px; padding: 25px; flex: 1; }
  .card-title { font-size: 0.8rem; color: #94a3b8; letter-spacing: 2px; margin-bottom: 20px; display: block; }
  .status-badge { display: inline-flex; align-items: center; gap: 10px; padding: 10px 20px; border-radius: 30px; border: 1px solid #059669; color: #10b981; font-weight: 600; font-size: 0.9rem; background: rgba(16, 185, 129, 0.1); }
  .status-badge.off { border-color: #475569; color: #64748b; background: transparent; }
  .dot { width: 10px; height: 10px; background: currentColor; border-radius: 50%; }
  .bpm-display { display: flex; align-items: center; gap: 15px; margin-top: 10px; }
  .heart-icon { width: 24px; height: 24px; background: #f87171; border-radius: 50%; box-shadow: 0 0 15px #f87171; }
  #bval { font-size: 3.5rem; font-weight: 700; color: #f87171; }
  #bval.off { color: #1e293b; }
  .unit { color: #475569; font-size: 1rem; align-self: flex-end; margin-bottom: 12px; }
  .chart-card { width: 100%; max-width: 900px; }
  canvas { width: 100% !important; height: 250px !important; margin-top: 10px; }
  .footer { margin-top: 30px; color: #475569; font-size: 0.8rem; }
</style></head><body>
<header><h1>♥ HEART RATE MONITOR</h1><div class="subtitle">ESP32 · Real-time PPG · 50 Hz</div></header>
<div class="top-row">
  <div class="card"><span class="card-title">SENSOR</span><div id="badge" class="status-badge off"><div class="dot"></div><span id="st">NO SIGNAL</span></div></div>
  <div class="card"><span class="card-title">HEART RATE</span><div class="bpm-display"><div class="heart-icon"></div><div id="bval" class="off">---</div><span class="unit">BPM</span></div></div>
</div>
<div class="card chart-card"><span class="card-title">BPM HISTORY</span><canvas id="cv"></canvas></div>
<div class="footer">Informational use only • Not a medical device</div>
<script>
const cv=document.getElementById('cv'); const ctx=cv.getContext('2d'); const pts=[];
function drawChart(){
  const W=cv.width=cv.offsetWidth, H=cv.height=cv.offsetHeight; ctx.clearRect(0,0,W,H);
  if(pts.length<2) return;
  const lo=40, hi=180; let grad=ctx.createLinearGradient(0,0,0,H);
  grad.addColorStop(0,'rgba(248,113,113,0.3)'); grad.addColorStop(1,'transparent');
  ctx.beginPath();
  pts.forEach((v,i)=>{ const x=i*(W/(pts.length-1)), y=H-((Math.min(Math.max(v,lo),hi)-lo)/(hi-lo))*(H-20)-10; i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); });
  let lastX=(pts.length-1)*(W/(pts.length-1)); ctx.lineTo(lastX,H); ctx.lineTo(0,H); ctx.fillStyle=grad; ctx.fill();
  ctx.strokeStyle='#f87171'; ctx.lineWidth=3; ctx.beginPath();
  pts.forEach((v,i)=>{ const x=i*(W/(pts.length-1)), y=H-((Math.min(Math.max(v,lo),hi)-lo)/(hi-lo))*(H-20)-10; i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); }); ctx.stroke();
  [60,80,100,120,140,160,180].forEach(g=>{ const y=H-((g-lo)/(hi-lo))*(H-20)-10; ctx.strokeStyle='rgba(100,116,139,0.1)'; ctx.lineWidth=1; ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke(); ctx.fillStyle='#475569'; ctx.font='12px sans-serif'; ctx.fillText(g,5,y-5); });
}
let prev=-1;
setInterval(()=>{
  fetch('/data').then(r=>r.json()).then(d=>{
    const el=document.getElementById('bval'), st=document.getElementById('st'), badge=document.getElementById('badge');
    if(!d.finger){ el.textContent='---'; el.className='off'; st.textContent='NO SIGNAL'; badge.className='status-badge off'; }
    else{ el.textContent=d.bpm; el.className=''; st.textContent='FINGER DETECTED'; badge.className='status-badge';
      if(d.bpm!==prev && d.bpm>0){ pts.push(d.bpm); if(pts.length>40) pts.shift(); drawChart(); prev=d.bpm; }
    }
  }).catch(()=>{});
}, 1000);
window.addEventListener('resize', drawChart);
</script></body></html>
)rawliteral";

/* ==================================================================
 * 4. HELPER FUNCTIONS
 * ================================================================== */

// Applies a Moving Average filter to smooth the raw signal
int maFilter(int v) {
  mSum -= maRing[mHead];
  maRing[mHead] = v;
  mSum += v;
  mHead = (mHead + 1) % MA_N;
  if (!maFull && mHead == 0) maFull = true;
  return mSum / (maFull ? MA_N : mHead);
}

// Calculates the average BPM from the stored Inter-Beat Intervals
int meanBpm() {
  if (iCount == 0) return 0;
  long s = 0;
  for (int i = 0; i < iCount; i++) s += ibiRing[i];
  return (int)(60000L * iCount / s);
}

// Returns a human-readable status string based on the current BPM
const char* statusStr() {
  if (!fingerOn || bpm == 0) return "No Finger";
  if (bpm < 60)  return "Resting";
  if (bpm < 100) return "Normal";
  if (bpm < 130) return "Active";
  return "High Alert!";
}

// Resets all tracking variables (used when finger is removed or timeout occurs)
void resetAll() {
  fingerOn = false; confCnt = 0;
  iCount = 0;       iHead = 0;
  bpm = 0;          aboveT = false;
  lastBeat = 0;
}

/* ==================================================================
 * 5. CORE SIGNAL PROCESSING
 * ================================================================== */
void processSample() {
  int raw = analogRead(SENSOR_PIN);

  // Step 1: Validate ADC Range (Hard reset if out of bounds)
  if (raw < FINGER_LO || raw > FINGER_HI) {
    if (fingerOn || confCnt > 0) {
      resetAll();
      Serial.println("[INFO] finger off (range)");
    }
    baseline = (float)raw;
    return;
  }

  // Step 2: Smooth the signal
  int f = maFilter(raw);
  lastFilt = f;
  unsigned long now = millis();

  // Step 3: Track Signal Amplitude (Rolling 2-second window)
  if (now - ampTimer >= 2000) { 
    sMin = sMax = f; 
    ampTimer = now; 
  }
  if (f < sMin) sMin = f;
  if (f > sMax) sMax = f;
  int amp = sMax - sMin;

  // Step 4: Track the DC baseline (Always tracks the signal trend)
  baseline = BASE_ALPHA * baseline + (1.0f - BASE_ALPHA) * f;

  // Step 5: Confirm Finger Presence (Check if amplitude is healthy)
  if (amp >= AMP_MIN) { 
    if (confCnt < CONFIRM_N) confCnt++; 
  } else { 
    confCnt = 0; 
  }
  
  fingerOn = (confCnt >= CONFIRM_N);
  if (!fingerOn) return; // Exit early if finger is not confirmed

  // Step 6: Peak Detection (Detecting the actual heartbeat pulse)
  float thr  = baseline + PEAK_D;
  float hyst = max(HYST_PCT * amp, 3.0f);

  if (!aboveT && f > thr) {
    // We just crossed the threshold going UP (Found a peak)
    aboveT = true;
    unsigned long ibi = now - lastBeat;

    if (lastBeat > 0 && ibi >= MIN_IBI && ibi <= MAX_IBI) {
      // Store valid IBI and recalculate average BPM
      ibiRing[iHead] = (int)ibi;
      iHead = (iHead + 1) % IBI_N;
      if (iCount < IBI_N) iCount++;

      int raw_bpm = meanBpm();
      bpm = (bpm == 0) ? raw_bpm : (int)(bpm * (1.0f - BPM_BLEND) + raw_bpm * BPM_BLEND);
      
      Serial.printf("[BEAT] ibi=%lums raw=%d bpm=%d amp=%d\n", ibi, raw_bpm, bpm, amp);
    } else if (lastBeat > 0) {
      // Invalid IBI (Noise or movement artifact)
      Serial.printf("[SKIP] ibi=%lums (valid: %d-%d)\n", ibi, MIN_IBI, MAX_IBI);
    }
    lastBeat = now;
  } 
  else if (aboveT && f < (thr - hyst)) {
    // We crossed the threshold going DOWN (Peak is over)
    aboveT = false;
  }

  // Step 7: Heartbeat Timeout (Reset if no beat detected for too long)
  if (lastBeat > 0 && now - lastBeat > BEAT_TO) {
    resetAll();
    Serial.println("[INFO] beat timeout");
  }
}

/* ==================================================================
 * 6. HARDWARE UPDATES & SETUP
 * ================================================================== */
void updateLcd() {
  // Update Top Row (BPM)
  lcd.setCursor(0, 0);
  lcd.write(byte(0)); 
  lcd.print(" HR: ");
  String s = (fingerOn && bpm > 0) ? String(bpm) + " BPM" : "---";
  while (s.length() < 10) s += ' ';
  lcd.print(s);

  // Update Bottom Row (Status)
  lcd.setCursor(0, 1);
  s = String(statusStr());
  while (s.length() < 16) s += ' ';
  lcd.print(s);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== HR Monitor ===");
  analogSetAttenuation(ADC_11db);

  // Initialize LCD
  Wire.begin(21, 22);
  lcd.init(); 
  lcd.backlight();
  lcd.createChar(0, heartGlyph);
  lcd.clear();
  lcd.print("  HR Monitor");
  lcd.setCursor(0, 1); 
  lcd.print("  Connecting...");

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.print("[WiFi] connecting");
  
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
    delay(500); 
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.printf("\n[WiFi] IP: %s\n", ip.c_str());
    lcd.setCursor(0, 1);
    while (ip.length() < 16) ip += ' ';
    lcd.print(ip);
  } else {
    Serial.println("\n[WiFi] FAILED - offline");
    lcd.setCursor(0, 1); 
    lcd.print("No WiFi         ");
  }

  // Setup Web Server Endpoints
  srv.on("/", HTTP_GET, []() {
    srv.send_P(200, "text/html", PAGE);
  });

  srv.on("/data", HTTP_GET, []() {
    String j = "{\"bpm\":" + String(bpm) + 
               ",\"finger\":" + (fingerOn ? "true" : "false") + 
               ",\"status\":\"" + statusStr() + "\"}";
    srv.sendHeader("Access-Control-Allow-Origin", "*");
    srv.sendHeader("Cache-Control", "no-cache");
    srv.send(200, "application/json", j);
  });

  srv.begin();
  delay(1000);

  tSmp = tLcd = tDbg = millis();
  Serial.println("[INFO] ready");
  Serial.println("raw  | filt | base  | amp | conf | finger | bpm");
}

/* ==================================================================
 * 7. MAIN LOOP
 * ================================================================== */
void loop() {
  srv.handleClient(); // Keep the web server responsive
  
  unsigned long now = millis();

  // Process sensor data at 50Hz (Every 20ms)
  if (now - tSmp >= SAMPLE_MS) { 
    tSmp = now; 
    processSample(); 
  }

  // Update LCD display at 1Hz (Every 1000ms)
  if (now - tLcd >= 1000) { 
    tLcd = now; 
    updateLcd(); 
  }

  // Print debug data to Serial at 2Hz (Every 500ms)
  if (now - tDbg >= 500) {
    tDbg = now;
    Serial.printf("%4d | %4d | %5.0f | %3d | %d/%d | %s | %3d\n",
      analogRead(SENSOR_PIN), lastFilt, baseline,
      sMax - sMin, confCnt, CONFIRM_N,
      fingerOn ? "YES" : " NO", bpm);
  }
}