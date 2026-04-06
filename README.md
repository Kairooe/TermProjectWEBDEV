# ESP32 AI Trivia Game

An AI-powered trivia game built on an ESP32 microcontroller. The device generates questions in real time using a locally-running Ollama LLM, displays them on an OLED screen, and records every answer to a cloud database linked to a web account.

**Live URLs**
- Web app: https://trivia.ktran.tech
- API: https://api.ktran.tech

---

## Table of Contents

- [System Architecture](#system-architecture)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Backend Setup](#backend-setup)
  - [Frontend Setup](#frontend-setup)
  - [Firmware Setup](#firmware-setup)
- [Starting All Services](#starting-all-services)
- [How It Works](#how-it-works)
  - [WiFi Provisioning](#wifi-provisioning)
  - [Device Pairing](#device-pairing)
  - [Game Loop](#game-loop)
  - [AI Question Generation](#ai-question-generation)
  - [Answer Recording](#answer-recording)
- [API Reference](#api-reference)
- [Hardware](#hardware)
- [Environment Variables](#environment-variables)
- [Tech Stack](#tech-stack)

---

## System Architecture

```
ESP32 Device
    │
    │ HTTPS (via WiFiClientSecure)
    ▼
Cloudflare Tunnel ──► api.ktran.tech
    │
    ▼
Express Backend (localhost:3001)
    ├── /api/ai/generate  ──► Ollama LLM (localhost:11434)
    ├── /api/device/*     ──► MongoDB Atlas
    ├── /api/auth/*       ──► MongoDB Atlas
    └── /api/study/*      ──► MongoDB Atlas

React Frontend (localhost:5173 / trivia.ktran.tech)
    │
    │ HTTPS + JWT
    ▼
Express Backend (api.ktran.tech)
```

---

## Repository Structure

```
TermProject/
├── server/                         # Node.js / Express backend
│   ├── server.js                   # App entry point
│   ├── .env                        # Secrets — not committed
│   └── src/
│       ├── config/db.js            # MongoDB connection
│       ├── middleware/auth.js      # JWT verification
│       ├── models/
│       │   ├── User.js
│       │   ├── DeviceSession.js
│       │   └── StudyRecord.js
│       └── routes/
│           ├── auth.js             # Register / login / me
│           ├── device.js           # ESP32 registration, polling, record
│           ├── study.js            # Stats, history, leaderboard, config
│           └── ai.js               # Ollama proxy
│
├── client/                         # React / Vite web app
│   ├── .env                        # VITE_API_URL
│   ├── vite.config.js
│   └── src/
│       ├── api/client.js           # Axios instance with JWT interceptor
│       ├── context/AuthContext.jsx # Global auth state
│       ├── components/
│       │   ├── NavBar.jsx
│       │   └── PrivateRoute.jsx
│       └── pages/
│           ├── LoginPage.jsx
│           ├── RegisterPage.jsx
│           ├── DashboardPage.jsx   # Stats + ESP32 pairing widget
│           ├── ProfilePage.jsx     # Study config + answer history
│           └── LeaderboardPage.jsx
│
└── PlatformIO/Projects/Ai_ESP_TermProject/
    ├── platformio.ini              # Board: ESP32 DoIt DevKit V1, COM4
    └── src/
        ├── main.cpp                # Game state machine
        ├── config.h                # URLs, model name, preset usernames
        ├── TriviaQ.h               # Shared question struct
        ├── wifi_manager.{h,cpp}    # WiFi provisioning + captive portal
        ├── OllamaClient.{h,cpp}    # HTTPS POST to AI proxy
        ├── BackendClient.{h,cpp}   # HTTPS POST answer records
        ├── DeviceClient.{h,cpp}    # Device registration + polling
        ├── OLEDDisplay.{h,cpp}     # 128×64 OLED with scrolling zones
        ├── ButtonHandler.{h,cpp}   # Debounce + A+B combo detection
        └── LEDController.{h,cpp}   # LED feedback patterns
```

---

## Getting Started

### Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| Node.js | 18+ | Backend + frontend |
| Ollama | latest | Local LLM inference |
| PlatformIO | latest | ESP32 firmware |
| cloudflared | latest | Public tunnel (Windows service) |
| MongoDB Atlas | — | Cloud database |

Pull the `qwen2.5:7b` model before running:

```bash
ollama pull qwen2.5:7b
```

---

### Backend Setup

```bash
cd server
npm install
```

Create `server/.env`:

```env
MONGODB_URI=mongodb+srv://<user>:<password>@<cluster>.mongodb.net/<dbname>
JWT_SECRET=<any long random string>
PORT=3001
OLLAMA_HOST=localhost
OLLAMA_PORT=11434
```

Start the server:

```bash
node server.js
```

Verify: `curl http://localhost:3001/health` should return `{"status":"ok"}`.

---

### Frontend Setup

```bash
cd client
npm install
```

`client/.env` is already configured for the public tunnel:

```env
VITE_API_URL=https://api.ktran.tech
```

Change to `http://localhost:3001` for fully local development.

Start the dev server:

```bash
npm run dev
```

---

### Firmware Setup

1. Open the `PlatformIO/Projects/Ai_ESP_TermProject` folder in VS Code with the PlatformIO extension installed.
2. Connect the ESP32 to USB (COM4).
3. Click **Upload** or run `pio run --target upload`.

To monitor serial output:

```bash
pio device monitor --baud 115200
```

---

## Starting All Services

After a reboot, start everything in this order:

| # | Service | Command | Confirm |
|---|---|---|---|
| 1 | Express backend | `cd server && node server.js >> server.log 2>&1 &` | `curl http://localhost:3001/health` |
| 2 | Cloudflared tunnel | `sc start cloudflared` (Windows service) | `sc query cloudflared` → STATE: RUNNING |
| 3 | Ollama | `ollama serve &` | `curl http://localhost:11434` |
| 4 | Vite frontend | `cd client && npm run dev &` | `curl http://localhost:5173` → 200 |

---

## How It Works

### WiFi Provisioning

On first boot (or when no saved credentials exist), the ESP32 opens a SoftAP hotspot named `TriviaGame-Setup`. Connect to it from a phone or laptop — a captive portal opens automatically at `192.168.4.1`.

Supported network types:
- **WPA/WPA2 Personal** — standard home/office WiFi
- **Open** — no password
- **Open + Captive Portal** — hotel/cafe networks (connect the ESP32, then complete the browser login on your own device)
- **WPA2 Enterprise (PEAP/MSCHAPv2)** — school/university networks with a username

Credentials are saved to ESP32 NVS (non-volatile storage) and used on every subsequent boot.

**Re-provisioning at any time:** Hold Button C + Button D together for 3 seconds. The device erases saved credentials and re-opens the captive portal.

During a boot connection attempt, the OLED shows `"Hold C+D to reset"` — this hint is visible throughout the 10-second connection window.

---

### Device Pairing

Pairing links the physical ESP32 to a web user account so that quiz results are attributed to the correct player.

```
ESP32                                  Web App
  │                                       │
  │── POST /api/device/register ─────────►│ Creates pending session (15 min TTL)
  │   { code: "482917" }                  │
  │                                       │
  │  OLED: "482917 / Enter on website"    │
  │                                       │
  │◄─ GET /api/device/poll/482917 ────────│ (every 3s)
  │   { claimed: false }                  │
  │                                       │
  │                    User types "482917" and clicks Connect
  │                                       │
  │                     POST /api/device/claim { code: "482917" }
  │                     Session: status → "claimed", userConfig copied
  │                                       │
  │◄─ GET /api/device/poll/482917 ────────│
  │   { claimed: true, username: "Alice", │
  │     userConfig: { subject, difficulty } }
  │                                       │
  │  Game starts with Alice's config      │
```

If the user presses C or D during the wait, the ESP32 skips pairing and uses a local preset username (no web tracking).

---

### Game Loop

```
GS_FETCHING  →  GS_QUESTION  →  GS_HIGHLIGHT (300ms)  →  GS_FEEDBACK (2500ms)  →  GS_FETCHING
                                                                │
                                              POST answer record to backend
```

Hold A + B together for 1.5 seconds at any time to trigger game over. The OLED cycles through three summary screens: total score → best streak → restart prompt.

---

### AI Question Generation

1. The ESP32 builds a JSON body with the Ollama model name, a structured prompt, `"stream": false`, and a random seed.
2. `POST https://api.ktran.tech/api/ai/generate` — 90-second timeout.
3. The Express proxy forwards the request to `http://localhost:11434/api/generate` with a 120-second timeout, buffers the full response, and returns it with `Content-Type: application/json`.
4. The ESP32 parses only the `"response"` field using ArduinoJson's streaming filter (the large `"context"` array is discarded without being loaded into RAM).
5. The response string is searched for a `{...}` JSON block and validated — all 6 fields (`question`, `A`, `B`, `C`, `D`, `answer`) must be non-empty.

**Topic rotation:** 12 general topics cycle when no subject is set. When a subject is configured, 10 question aspects rotate to prevent repetition.

**Prompt demanded format:**
```json
{"question":"...","A":"...","B":"...","C":"...","D":"...","answer":"B"}
```

---

### Answer Recording

After every answer (during the 300ms highlight phase):

```
POST https://api.ktran.tech/api/device/record
{
  "code":           "482917",
  "question":       "Which planet...",
  "selectedAnswer": "B",
  "correctAnswer":  "C",
  "isCorrect":      false,
  "questionNumber": 4,
  "subject":        "science and nature",
  "timestamp":      "2025-04-05T14:32:01Z"
}
```

The device code is used as authentication — the server looks up the claimed session to find the user. A 2-second timeout ensures a slow network never stalls the game. If the save succeeds (HTTP 201), the OLED shows `"Saved!"` during the feedback phase.

---

## API Reference

All routes are under `https://api.ktran.tech`.

### Auth — `/api/auth`

| Method | Path | Auth | Body | Response |
|---|---|---|---|---|
| POST | `/register` | — | `{username, email, password}` | `{token, username}` |
| POST | `/login` | — | `{email, password}` | `{token, username}` |
| GET | `/me` | JWT | — | `{username, email, studyConfig}` |

### Device — `/api/device`

| Method | Path | Auth | Body / Param | Description |
|---|---|---|---|---|
| POST | `/register` | — | `{code}` | Create 15-min pending session |
| GET | `/poll/:code` | — | `:code` | `{claimed, username?, userConfig?}` or 404 |
| POST | `/claim` | JWT | `{code}` | Link session to logged-in user |
| POST | `/record` | device code | Answer fields | Save one StudyRecord |

### Study — `/api/study` (JWT required on all)

| Method | Path | Query / Body | Description |
|---|---|---|---|
| POST | `/record` | Answer object(s) | Save records from web |
| GET | `/history` | `limit`, `skip`, `subject` | Paginated answer history |
| GET | `/stats` | — | Accuracy, streak, per-subject breakdown |
| PUT | `/config` | `{subject, difficulty, customNotes}` | Save study preferences |
| GET | `/leaderboard` | — | Top 10 players with ≥10 answers |

### AI — `/api/ai`

| Method | Path | Auth | Description |
|---|---|---|---|
| POST | `/generate` | — | Proxy to Ollama, 120s timeout, buffered |

---

## Hardware

**Board:** ESP32 DoIt DevKit V1

| GPIO | Component | Notes |
|---|---|---|
| 19 | Button A | Answer A — INPUT_PULLUP, active LOW |
| 18 | Button B | Answer B — INPUT_PULLUP, active LOW |
| 5 | Button C | Answer C — also WiFi reset trigger |
| 4 | Button D | Answer D — also WiFi reset trigger |
| 12 | LED A | Feedback for answer A |
| 13 | LED B | Feedback for answer B |
| 14 | LED C | Feedback for answer C |
| 15 | LED D | Feedback for answer D |
| 21 | SDA | I2C for OLED |
| 22 | SCL | I2C for OLED |

**Display:** 128×64 SSD1306 OLED — top 16px yellow zone (question text), bottom 48px white zone (answers). Text scrolls horizontally when too long to fit.

**Button behaviour:**
- C / D → answer registered immediately on press (50ms debounce)
- A / B → 200ms combo window: both pressed = combo mode; released solo within 400ms = answer
- A + B held 1.5s → game over

---

## Environment Variables

### `server/.env`

| Variable | Description |
|---|---|
| `MONGODB_URI` | MongoDB Atlas connection string |
| `JWT_SECRET` | Secret key for signing JWTs |
| `PORT` | Server port (default: 3001) |
| `OLLAMA_HOST` | Ollama hostname (default: localhost) |
| `OLLAMA_PORT` | Ollama port (default: 11434) |

### `client/.env`

| Variable | Description |
|---|---|
| `VITE_API_URL` | Backend base URL (`https://api.ktran.tech` or `http://localhost:3001`) |

---

## Tech Stack

| Layer | Technology |
|---|---|
| Firmware | C++ / Arduino (ESP32, PlatformIO) |
| Firmware HTTP | WiFiClientSecure + HTTPClient + ArduinoJson |
| Firmware display | Adafruit SSD1306 (game), U8g2 (provisioning) |
| Backend | Node.js, Express |
| Database | MongoDB Atlas, Mongoose |
| Auth | bcryptjs, jsonwebtoken |
| Frontend | React, Vite, React Router, Axios |
| AI model | Ollama — `qwen2.5:7b` |
| Tunnel | Cloudflare Tunnel (cloudflared Windows service) |
| DNS | `ktran.tech` — `api.*` → backend, `trivia.*` → frontend |
