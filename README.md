# Lumi — the study buddy that opens its eyes when you open your books

<img width="100%" alt="Lumi study buddy on a breadboard with glowing cyan OLED eyes" src="https://github.com/user-attachments/assets/dda8d7a0-dce2-4ae8-bdd4-f085ab939594" />

Lumi is a small desk study companion with a face. It notices when you sit down and tracks your
real study time. The web app runs the session, and an AI study buddy (**Google Gemini**) teaches
you from your own course material, quizzes you, finds your weak spots, and sends them back to
Lumi's screen.

> Built for Devpost's **Build With AI: Basics** hackathon. Planning docs are in [`devpost/`](devpost/).

## The study loop

1. **Sit down.** Lumi's ultrasonic sensor sees you and starts a focus session. The page starts tracking it.
2. **Learn.** Upload a chapter (PDF, Word, text) or write a note. Ask the AI to explain it. Answers come from your own material.
3. **Get quizzed.** "Quiz me" asks 5 questions, one at a time. Each question is tagged with a topic, and the AI grades your answer.
4. **Weak spot → Lumi.** Miss a topic more often than you get it right and it becomes a weak spot. Lumi's screen shows `REVIEW: METAPHASE`.
5. **Walk away.** When the session ends, the AI writes a report: time focused, quiz score, weak area, and a specific next session.

## How it fits together

```text
 Lumi (Arduino Uno + HC-SR04 + SSD1306 OLED + buzzer + button)
        │  USB serial, 115200 baud
        ▼
 Web app (web/index.html, Chrome/Edge, Web Serial)
        │  /api/...  (localhost only)
        ▼
 Study server (server/server.js) ──► Gemini API (OpenAI-compatible)
        └─ stores course chunks + session logs in server/data/
```

The AI API key lives only on the study server. The server only listens on your own
computer, so nobody else on your network can use it.

## Run it

Requirements: Node.js 18+, Chrome or Edge.

```bash
cp .env.example .env      # Windows: copy .env.example .env
# put your Gemini key in .env as AI_API_KEY (free at aistudio.google.com/apikey)
npm install
npm start
```

Open **http://localhost:8787**.

**No key yet?** Set `PRACTICE_MODE=1` in `.env`. Every AI feature then gives pretend answers,
clearly labelled "Practice mode", so you can try the whole flow. It switches off as soon as a real key is set.

**No hardware?** Click **Try demo**. A pretend Lumi runs 20× faster: press **Sit down** (or the **S** key)
to start a session and step away to pause. Nothing you do in the demo is saved.

## With the real device

1. Install the **Adafruit SSD1306** and **Adafruit GFX** libraries, then upload
   `arduino/lumi_lamp/lumi_lamp.ino` to an Arduino Uno.
2. Plug Lumi in by USB and close the Arduino Serial Monitor.
3. In the web app, click **Connect lamp**.

### Wiring

| Part | Pin → Arduino |
|---|---|
| HC-SR04 sensor | VCC → 5V, GND → GND, TRIG → D9, ECHO → D10 |
| 0.96" OLED (I2C) | VCC → 5V, GND → GND, SDA → A4, SCL → A5 |
| Active buzzer | + → D8, − → GND |
| Push-button | one leg → D2, the diagonal leg → GND |

### How Lumi behaves

- When nobody is near, Lumi's eyes blink, look around and make different expressions.
- Sit within **40 cm** for **5 seconds** and the focus countdown starts (the length you pick on the page).
- Step away and it pauses at once; after **10 seconds** away it shows *Paused*. Sit back down for 5 seconds to carry on.
- At 00:00 it plays a tune and counts down your break.
- Today's study total is saved in EEPROM, so it survives a power loss.
- Button: short press shows today's stats; hold 3 seconds for a new day.

## How the AI is used

| Feature | Endpoint | What the AI does |
|---|---|---|
| Study buddy chat | `POST /api/chat` | Answers from the most relevant passages of your uploaded course + your current note |
| Quiz question | `POST /api/quiz/question` | Writes one question as JSON `{topic, question, answer}`, aiming at your weak topics |
| Quiz grading | `POST /api/quiz/grade` | Judges your answer fairly and explains the right one |
| Session report | `POST /api/session/end` | Turns the session log + quiz results into a report with a next-session plan |

Course files are split into chunks and the best matches are picked by keyword overlap, so every
answer is grounded in the student's own material. Model: `gemini-3.8-flash` through Gemini's
OpenAI-compatible `/chat/completions` endpoint. Any OpenAI-compatible service works: set
`AI_BASE_URL` and `AI_MODEL` in `.env`.

## USB protocol (Lumi ↔ page)

**Lumi → page**, one line per update:
`S,mode,focusDone,focusMin,breakMin,modeSec,dist,pomos,today,here,overlay,pending`, plus `E,DONE` when a session finishes.

Modes: 0 away · 1 left mid-session · 2 asleep · 3 saying hi · 4 focus · 5 finished · 6 break.

**Page → Lumi:** `A<sec>` acknowledge saved seconds · `K<text>` current task · `F<min>` focus length ·
`B<min>` break length · `M<text>` note on screen (max 21 characters) · `T` play tune.

## Folders

| Folder | What's inside |
|---|---|
| `web/` | The web app (one HTML file) |
| `server/` | Study server: uploads, retrieval, chat, quiz, session reports |
| `devpost/` | Planning docs: scope, PRD, spec |
| `arduino/` | Lumi's Arduino sketch |

## What's next

- Water reminders during long study sessions
- More eye expressions and reactions
- A finished enclosure, so Lumi is a neat desk companion instead of a breadboard

## How it was built

I designed Lumi and the study experience, built the hardware myself, and used AI coding
assistants to help write the software.

## License

MIT. See [LICENSE](LICENSE).
