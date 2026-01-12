# FlipDoctor 🩺  
*A physics-based snapping puzzle game for Flipper Zero*

**FlipDoctor** is a physics-based Flipper Zero game inspired by classic peg-and-stick mechanics. You control a continuously rotating stick anchored to pegs on a grid, snapping from peg to peg while avoiding an enemy and navigating obstacles.

---

## 🎮 Controls

| Button | Action |
|------|-------|
| **OK** | Snap to nearest peg |
| **Left / Right** | Reverse stick spin direction |
| **Back** | Exit game |
| **Any key (after win/loss)** | Restart level |

---

## 🧠 Gameplay

- Stick rotates continuously around the current peg  
- Press **OK** to snap when the tip is close to another peg  
- Avoid the enemy’s rotating stick  
- Bounce off obstacle walls  
- Reach the goal peg to win  
- Completion time is displayed  

---

## 📁 Optional Level Loading

FlipDoctor can load level data from the SD card:

```
/apps_data/flip_doctor/level.bin
```

If the file is missing or invalid, the game falls back to built-in defaults.

---

## 🛠 Building & Running

### 1. Clone Flipper Firmware

```bash
git clone https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware
```

---

### 2. Clone FlipDoctor into `applications_user`

From the **firmware root**:

```bash
cd applications_user
git clone https://github.com/pydevcs/flip-doctor.git
```

You should now have:

```
flipperzero-firmware/
└── applications_user/
    └── flipdoctor/
        ├── flip_doctor.c
        └── application.fam
```

---

### 3. Build & Launch on Flipper Zero

Plug in your Flipper Zero via USB, then run these commands:

```bash
cd applications_user
git clone https://github.com/pydevcs/flip-doctor.git
./fbt launch debug APPSRC=flipdoctor
```

This will:
- Change to the applications_user directory
- Clone the FlipDoctor repository
- Build only the FlipDoctor app
- Flash it to your connected Flipper Zero
- Launch it immediately in debug mode

---

## 🔔 Feedback & Effects

- **Snap** → short vibration  
- **Level complete** → vibration + RGB LED sequence  
- **Failure** → error notification  

---

## 📜 License

MIT License. See the LICENSE file for details.
