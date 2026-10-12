# Lumi — the study lamp that opens its eyes when you open your books

A physical AI study companion. The lamp notices when you sit down and tracks your real study
time. The web app runs the session. An AI study buddy (**Google Gemini**) teaches you from
your own course material, quizzes you, finds your weak spots, and sends them back to the
lamp's screen.

> Built for Devpost's **Build With AI: Basics** hackathon. Planning docs are in [`devpost/`](devpost/).

## The study loop

1. **Sit down.** The lamp's ultrasonic sensor sees you and starts a focus session. The page starts tracking it.
2. **Learn.** Upload a chapter (PDF, Word, text) or write a note. Ask the AI to explain it. Answers come from your own material.
3. **Get quizzed.** "Quiz me" asks 5 questions, one at a time. Each question is tagged with a topic, and the AI grades your answer.
4. **Weak spot → lamp.** Miss a topic more often than you get it right and it becomes a weak spot. The lamp screen shows `REVIEW: METAPHASE`.
5. **Walk away.** When the session ends, the AI writes a report: time focused, quiz score, weak area, and a specific next session.

## How it fits together

```text
 Lamp (Arduino Uno + HC-SR04 + SSD1306 OLED + buzzer)
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

**No lamp?** Click **Try demo**. A pretend lamp runs 20× faster: press **Sit down** (or the **S** key)
to start a session and step away to pause. Nothing you do in the demo is saved.

## With the real lamp

1. Install the **Adafruit SSD1306** and **Adafruit GFX** libraries, then upload
   `arduino/lumi_lamp/lumi_lamp.ino` to an Arduino Uno.
2. Plug the lamp in by USB and close the Arduino Serial Monitor.
3. In the web app, click **Connect lamp**.

### Wiring

| Part | Pin → Arduino |
|---|---|
| HC-SR04 sensor | VCC → 5V, GND → GND, TRIG → D9, ECHO → D10 |
| 0.96" OLED (I2C) | VCC → 5V, GND → GND, SDA → A4, SCL → A5 |
| Active buzzer | + → D8, − → GND |
| Push-button | one leg → D2, the diagonal leg → GND |

### How the lamp behaves

- Sit within **40 cm** for **5 seconds** and the focus countdown starts (the length you pick on the page).
- Step away and it pauses at once; after **10 seconds** away it shows *Paused*. Sit back down for 5 seconds to carry on.
- At 00:00 it plays a tune and counts down your break.
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

## USB protocol (lamp ↔ page)

**Lamp → page**, one line per update:
`S,mode,focusDone,focusMin,breakMin,modeSec,dist,pomos,today,here,overlay,pending`, plus `E,DONE` when a session finishes.

Modes: 0 away · 1 left mid-session · 2 asleep · 3 saying hi · 4 focus · 5 finished · 6 break.

**Page → lamp:** `A<sec>` acknowledge saved seconds · `K<text>` current task · `F<min>` focus length ·
`B<min>` break length · `M<text>` note on screen (max 21 characters) · `T` play tune.

## Folders

| Folder | What's inside |
|---|---|
| `web/` | The web app (one HTML file) |
| `server/` | Study server: uploads, retrieval, chat, quiz, session reports |
| `devpost/` | Planning docs: scope, PRD, spec |
| `arduino/` | The lamp's Arduino sketch |

## How it was built

I designed the lamp and the study experience and built the hardware myself, and used AI coding
assistants to help write the software.

## License

MIT. See [LICENSE](LICENSE).
