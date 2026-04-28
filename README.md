# ❤️ Web-Based Heart Rate Monitoring System (ESP32 + IoT)

## 📌 Overview
This project is a real-time heart rate monitoring system using ESP32 and a pulse sensor.  
It measures heart rate (BPM) from the fingertip and displays it on a live web page hosted by the ESP32.

The system provides real-time monitoring with a modern dashboard including BPM value, status indicator, and live graph.

---

## 🚀 Features
- Real-time heart rate monitoring (BPM)
- Web-based dashboard hosted on ESP32
- Live updating graph
- Status indicator (Resting / Normal / Active / High Alert)
- Optional LCD/OLED display support

---

## 🛠️ Components
- ESP32
- Pulse Sensor (KY-039 or similar)
- OLED Display (Optional)
- Jumper Wires
- USB Power Supply

---

## 🔌 Circuit Connections

### Pulse Sensor:
- VCC → 5V / VIN (ESP32)
- GND → GND
- Signal → GPIO 35

### OLED (Optional):
- VCC → 3.3V
- GND → GND
- SDA → GPIO 21
- SCL → GPIO 22

---

## 💻 How It Works
- Pulse sensor reads heartbeat signals from fingertip  
- ESP32 processes signal and calculates BPM  
- ESP32 hosts a web server  
- Data is displayed live on browser dashboard  

---

## 🌐 Web Interface
The web page shows:
- Current BPM ❤️
- Live graph 📊
- Connection status 🟢

---

## ⚙️ Setup
1. Install ESP32 board in Arduino IDE  
2. Install required libraries (WiFi, WebServer, Wire, LiquidCrystal_I2C)  
3. Upload code to ESP32  
4. Set WiFi name and password in code  
5. Open Serial Monitor to get IP address  
6. Open IP in browser  

---

## 📊 BPM Status
- Below 60 → Resting  
- 60–100 → Normal  
- 100–130 → Active  
- Above 130 → High Alert
  <img width="1915" height="963" alt="Screenshot 2026-04-21 213223" src="https://github.com/user-attachments/assets/bcef7057-6ed3-4dfe-8f72-0c76596f389e" />


---

## ⚠️ Disclaimer
This project is for educational purposes only and is not a medical device.

---

## 💡 Future Work
- Mobile app integration
  ⭐ If you enjoyed this project or found it useful, don’t forget to star the repository! ⭐
 ## 🧑‍💻 Author
  
- **Farida Ayman** → [GitHub Profile](https://github.com/FaridaAyman)  
- **Nada Attia** → [GitHub Profile](https://github.com/NadaAttia04)  
- **Rodina Ahmed** → [GitHub Profile](https://github.com/RodinaAhmed)
