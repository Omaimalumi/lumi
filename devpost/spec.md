---
doc: spec
status: approved
---

# Lumi — Technical Spec

## How This Works, In Plain Language
Lumi has three pieces that work together on your own computer.

1. **The lamp** is an Arduino with a distance sensor, a small screen, and a buzzer. Its program decides whether you're at the desk, runs the countdown, and draws the eyes and messages. **The lamp is the boss of the timer.** It keeps time even if the web page or the AI has a problem.
2. **The web page** is what you see in Chrome or Edge. It talks to the lamp through the USB cable, using a browser feature called **Web Serial**. The cable carries short lines of text in both directions:
   - **Lamp → page:** every second, the lamp sends a status line such as `S,4,312,25,…`. The page splits it at the commas and copies what it says. That's why both screens always show the same time.
   - **Page → lamp:** the page sends short commands. The first letter says what kind of command it is. `F40` means "focus length is 40 minutes," and `MREVIEW: MITOSIS` means "show this message."
3. **The study server** is a small program running on your computer. It keeps your uploaded notes, and it talks to Gemini for the AI study buddy. Your Gemini **API key**, the password that lets your app use Gemini, lives only in the server's `.env` file. It never goes into the web page or to GitHub.

**Why this shape?** It already exists and it works. Keeping the timer on the lamp and the AI on the server means an AI problem can never stop your study session. This hackathon is about making the existing demo session smooth, not rebuilding anything.

## The Core Journey Through the System
PRD ref: `prd.md > The Core Journey`.

**Before the session**
1. You run `npm start`. The study server starts and serves the web page at `http://localhost:8787`.
2. You click **Connect lamp** and pick the lamp in the browser's pop-up. The page opens the USB connection, and status lines start arriving every second.
3. You pick a focus time in **Session settings**. The page sends `F25` (or `F40`, and so on) to the lamp, and the lamp saves it.
4. You upload notes to the **Course library**. The page sends the file to the server (`POST /api/upload`). The server pulls out the text, splits it into small pieces called **chunks**, and saves them in `server/data/courses/`.

**The timer moments** (all decided on the lamp)
5. Nobody is near. The lamp is in mode `0` (away) and draws the eyes.
6. You sit within 40 cm. The lamp switches to mode `3` and fills a bar for 5 seconds, then to mode `4` (focus) and starts counting down. The page sees the `4` in the status line and shows "focusing."
7. You get up. The countdown stops at once, and the lamp shows "Where'd you go?" with a 10-second countdown. After 10 seconds, it switches to mode `1` (paused), and the page shows "paused." After 5 minutes paused, the lamp cancels the session and goes back to mode `0`.
8. You sit back down for 5 seconds. The lamp returns to mode `4`, and the countdown continues.

**The AI moment** (the lamp keeps counting the whole time)
9. You click **Quiz me (5 questions)**. The page asks the server for a question (`POST /api/quiz/question`). The server picks the most relevant chunks of your notes, asks Gemini to write one question with a topic and an answer, and sends it back.
10. You type an answer and press Enter. The page sends it to the server (`POST /api/quiz/grade`). Gemini judges it, and the page shows "correct" or the explanation.
11. The page keeps score for each topic. When a topic has been missed more often than gotten right, it becomes a weak spot, and the page sends `MREVIEW: MITOSIS` to the lamp. The lamp shows the message for about 4 seconds.
12. After 5 questions, the page shows "Quiz done: 4/5 correct (80%)," your weak spots, and what to review next.

**The finish**
13. The countdown reaches 00:00. The lamp plays a tune, shows "Done! Break time" for 3 seconds, and sends `E,DONE` to the page. Then the break countdown starts.

## Stack
This is the existing stack. Nothing is being added or changed.

| Piece | Tool | Docs |
|---|---|---|
| Lamp program | Arduino (C++) on an **Arduino Uno**, with the **Adafruit SSD1306** and **Adafruit GFX** libraries for the screen | https://docs.arduino.cc · https://github.com/adafruit/Adafruit_SSD1306 |
| Lamp parts | HC-SR04 distance sensor, 0.96" OLED screen (I2C), active buzzer, push-button | Wiring table in `README.md` |
| Web page | One HTML file with plain JavaScript and CSS, no framework | — |
| Page ↔ lamp | **Web Serial API** (Chrome and Edge only), 115200 baud | https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API |
| Study server | **Node.js 18+** with **Express 5** | https://nodejs.org · https://expressjs.com |
| File uploads | `multer` (receiving files), `unpdf` (PDF), `mammoth` (Word), `jszip` | https://github.com/expressjs/multer · https://github.com/unjs/unpdf · https://github.com/mwilliamson/mammoth.js |
| AI | **Google Gemini** through its OpenAI-compatible `/chat/completions` endpoint, model `gemini-3.8-flash` | https://ai.google.dev/gemini-api/docs/openai |

**Why:** the learner already built Lumi with these tools, and the AI works with their real key. Keeping the stack means the build time goes to the demo, not to setup.

**Not checked in this session:** these doc links weren't opened during planning. The model name comes from the project's `.env.example`, and the learner confirmed the AI works with their real key.

## Where It Runs and How Someone Tries It
- **Runs locally** on the learner's Windows computer. **No deployment.** The learner chose this because the web page needs the lamp plugged into the same computer anyway.
- **Needs:** Node.js 18+, Chrome or Edge, the lamp plugged in by USB, and a real Gemini key in `.env` as `AI_API_KEY`.
- **Start:**
  ```
  npm install     (first time only)
  npm start
  ```
  Then open **http://localhost:8787** in Chrome or Edge, and click **Connect lamp**.
- **Before connecting:** close the Arduino IDE's Serial Monitor. Only one program can use the USB connection at a time.
- **For the demo recording** (`prd.md > Product Decisions`): set a custom focus time of about 1 minute, upload notes first, use **Focus mode** for the timer moments, then scroll to the **Study buddy** for the quiz. The video shows the screens and the learner's hands.
- **Submission:** the demo video and a public GitHub repository. The repo must never contain `.env` or `server/data/`. Both are already in `.gitignore`.

## Look and Feel
Carried forward from `prd.md > Look and Feel`. **This style already exists in `web/index.html` and on the lamp. Any change must match it, not replace it.**
- **Web page:** dark background, soft lilac and lavender, rounded cards, the cute character, and small ♡ and ✦ touches. Reuse the existing CSS variables (for example `--lilac`, `--bg-2`, `--mist`, `--edge`) and existing classes, rather than adding new colors.
- **Messages and copy:** warm and friendly, like a study buddy. For example: "Lamp disconnected. See you soon ♡". Never harsh or technical.
- **Lumi's screen:** the eyes stay alive and playful. Messages are short, since the lamp shows at most 21 characters.
- **Avoid:** office-tool looks, bright or harsh colors, and clutter.

## Components

### Lamp Program
`arduino/lumi_lamp/lumi_lamp.ino`. It reads the sensor, runs the session, draws the screen, and talks to the page over USB.
PRD ref: `prd.md > Noticing You (Presence Timer)`, `prd.md > Pausing When You Leave`, `prd.md > Finishing a Session`.

**Settings** (top of the file):

| Setting | Value | Meaning |
|---|---|---|
| `SIT_DISTANCE_CM` | 40 | Closer than this means you're at the desk |
| `START_DELAY_MS` | 5000 | Sit this long before the timer starts or resumes |
| `AWAY_GRACE_MS` | 10000 | Away this long, and the lamp shows "Paused" |
| `GIVE_UP_SEC` | 300 | Paused this long, and the session is cancelled |
| `DONE_SHOW_SEC` | 3 | How long "Done!" shows before the break |
| `SAVE_EVERY_MS` | 60000 | Save today's total every minute |

**Modes:** `0` away · `1` paused · `2` asleep · `3` saying hi (the 5-second bar) · `4` focus · `5` done · `6` break.

**Behavior decided in `4-spec`:** the countdown stops the moment you leave. Time away never counts. This is the existing code, which the learner chose to keep.

### USB Messages (Page ↔ Lamp)
The shared language between the lamp and the page, at 115200 baud, one line per message.
PRD ref: `prd.md > Connecting the Lamp`, `prd.md > Weak Spots on the Lamp`.

- **Lamp → page:** `S,mode,focusDone,focusMin,breakMin,modeSec,dist,pomos,today,here,overlay,pending`, sent every second and whenever something changes. `E,DONE` is sent when a session finishes.
- **Page → lamp:**
  - `A<sec>` acknowledges saved seconds
  - `K<text>` sets the current task
  - `F<min>` sets the focus length
  - `B<min>` sets the break length
  - `M<text>` shows a message for about 4 seconds (21 characters at most)
  - `T` plays the tune

### Web Page: Connection and Timer
`web/index.html`. This part connects to the lamp, reads its status lines, and shows the study timer card (character, progress ring, countdown, and focusing/paused/break label). It sends the focus time from **Session settings**. It also shows the friendly "Lost the lamp connection. Plug it back in…" message if the cable is pulled.
PRD ref: `prd.md > Connecting the Lamp`, `prd.md > Noticing You (Presence Timer)`, `prd.md > States and Boundaries`.

### Web Page: Focus Mode
Also in `web/index.html`. The **Focus mode** button hides everything except the timer. The exit button or Esc brings the page back. The timer keeps running either way, because it's driven by the lamp.
PRD ref: `prd.md > Focus Mode`.

### Web Page: Study Buddy Quiz
Also in `web/index.html`. **Quiz me** runs 5 questions, one at a time. For each one, the page asks the server for a question, shows "Question N of 5 · Topic," sends your answer for grading, and keeps score per topic. When a topic becomes a weak spot, the page sends `MREVIEW: TOPIC` to the lamp. At the end, it shows the score, the weak spots, and what to review next.
PRD ref: `prd.md > AI Study Buddy Quiz`, `prd.md > Weak Spots on the Lamp`.

### Study Server
`server/server.js`. It serves the web page, receives uploads, picks the most relevant chunks of your notes by matching keywords, and calls Gemini. The key comes from `.env`. It only listens on your own computer (`127.0.0.1`).
PRD ref: `prd.md > AI Study Buddy Quiz`.

Endpoints the demo uses:

| Endpoint | What it does |
|---|---|
| `GET /api/status` | Reports whether the AI is ready, and whether it's in Practice mode |
| `POST /api/upload` | Receives note files, pulls out the text, and saves chunks |
| `POST /api/quiz/question` | Returns `{topic, question, answer}` from your notes |
| `POST /api/quiz/grade` | Returns `{correct, feedback}` |

### Full Feature List Extras
Everything in `prd.md > Full Feature List` (tasks, progress, mission, notebook, other study buddy buttons, session report, and so on) lives in the same files. **The build doesn't change these** unless they get in the way of the demo.

## Data Model
Where each piece of information lives, how it changes, and what happens when you leave and come back:

| Information | Where it lives | How it changes | When you come back |
|---|---|---|---|
| Countdown, mode, time studied this session | The lamp's memory | The lamp updates it every second | Lost if the lamp is unplugged. A new session starts from zero. |
| Today's total, focus length, break length | The lamp's **EEPROM** (memory that survives power loss) | Saved every minute, and when you pause | Kept. At most one minute is lost if unplugged. |
| Uploaded notes (as chunks) | `server/data/courses/` on your computer | Written when you upload | Kept |
| Session logs | `server/data/sessions/` | Written during a session | Kept |
| Quiz score and weak spots for this quiz | The web page's memory | Updated after each answer | Lost if the page is reloaded |
| Page settings, tasks, progress | The browser's **localStorage** (a small storage area the browser keeps for one website) | Saved as you use the page | Kept in that browser |
| Gemini key | `.env` (never in git) | Edited by hand | Kept |

## File Structure
```
lumi-study-lamp/
├── arduino/
│   ├── README.md
│   └── lumi_lamp/
│       └── lumi_lamp.ino   # The lamp: sensor, modes, countdown, eyes, USB messages
├── web/
│   └── index.html          # The whole web page: layout, styles, lamp connection, timer, quiz, extras
├── server/
│   ├── server.js           # Study server: serves the page, uploads, Gemini calls
│   ├── README.md
│   └── data/               # Your notes and session logs (NOT in git)
├── devpost/                # Planning docs: scope, PRD, spec (learner-profile.md is NOT in git)
├── .env                    # Your Gemini key (NOT in git)
├── .env.example            # Safe template for .env (in git)
├── package.json            # npm start → runs server/server.js
├── README.md
└── LICENSE
```

## External Services and Dependencies
**Google Gemini** (the only outside service)
- **Call:** `POST https://generativelanguage.googleapis.com/v1beta/openai/chat/completions`
- **Auth:** the header `Authorization: Bearer <AI_API_KEY>`
- **Body:** `{ model: "gemini-3.8-flash", messages: [...], temperature: 0.35, max_tokens: ... }`
- **Response:** `choices[0].message.content`. For the quiz, this is a JSON object as text.
- **Timeout:** the server gives up after 90 seconds.
- **Cost and limits:** a free key from https://aistudio.google.com/apikey. Free keys have rate limits. The exact limits weren't checked during planning.
- **Docs:** https://ai.google.dev/gemini-api/docs/openai

## Important Failure Modes
- **Gemini is slow, fails, or the internet drops** → the server returns a friendly error ("The AI request timed out. Try again."). The lamp and the timer keep going. *To check in the build:* that the quiz keeps its progress and you can try that question again (`prd.md > States and Boundaries`).
- **Quiz me with no notes uploaded** → the server already answers "Add a course file or write a note first, so I know what to quiz you on." This settles the PRD's assumption.
- **The lamp is unplugged** → the page shows "Lost the lamp connection. Plug it back in…" The lamp loses power, and at most one minute of today's total is lost.
- **Connect lamp fails** → usually the Arduino Serial Monitor is still open, or the browser isn't Chrome or Edge. The fix: close the Serial Monitor, then use Chrome or Edge.
- **A long weak-spot topic** → the lamp shows at most 21 characters, so `REVIEW: PHOTOSYNTHESIS` (22) gets cut off. *To check in the build.*

## What Was Simplified and Why
- **Local only, no deployment** — the lamp has to be plugged into the same computer, and a recorded video is all the submission needs. Putting it online would mean hosting the server, protecting the key with a password, and still needing a lamp on the visitor's desk.
- **The extras stay as they are** — the build time goes to the demo (`prd.md > Product Decisions`).
- **Keyword matching instead of an AI search for notes** — the server picks note chunks by matching words. That's already built, it's simple, and it's good enough for one set of notes.

## Decisions and Open Issues

**Learner decisions**
- **Keep the lamp's existing pause behavior.** The countdown stops the moment you leave. "Paused" appears after 10 seconds, and the session is cancelled after 5 minutes away. *Why:* time away never counts, which keeps you honest. The PRD was updated to match.
- **Keep the existing stack.** It's already built, and the AI works with the learner's real key (the quiz, flashcards, and summaries all tested by the learner).
- **Run locally and record the video.** No deployment.

**Details derived from those decisions (not separately chosen by the learner)**
- The build checks and fixes; it doesn't rebuild. Changes stay inside `web/index.html`, `server/server.js`, and `arduino/lumi_lamp/lumi_lamp.ino`.
- New styling reuses existing CSS variables and classes.

**The learner's useful unknown: how the page talks to the lamp over USB**
- *What clarified it:* in planning, a walk-through of the `S,4,312,…` status line (lamp → page) and the `MREVIEW: MITOSIS` command (page → lamp).
- *Small investigation during the build* (about 5 minutes, no code changes): with the page disconnected, open the Arduino IDE's **Serial Monitor** at 115200 baud. Watch the mode number change from `0` to `3` to `4` as you sit down, and from `4` to `1` as you leave. Then type `MHELLO` and watch it appear on Lumi's screen.

**To check early in the build**
- An AI error in the middle of the quiz keeps the quiz progress, and you can retry.
- What happens to `REVIEW:` messages longer than 21 characters.

**Carried over from `prd.md > Open Questions`**
- The friendly AI error message: the server's wording already exists. Check that it shows nicely in the Study buddy.
- Quiz me with no notes uploaded: settled (see **Important Failure Modes**).
