---
doc: checklist
status: approved
---

# Build Checklist

Build mode: learn

Lumi is already built, so each slice **checks one part of the demo session end to end, fixes what's wrong, and leaves it working.** Changes stay inside `web/index.html`, `server/server.js`, and `arduino/lumi_lamp/lumi_lamp.ino` (`spec.md > Decisions and Open Issues`).

## Slices

- [x] **1. Sit down, and the countdown starts on both screens**
  Becomes usable: The server runs, the page connects to the lamp, the chosen focus time reaches the lamp, and sitting down starts the countdown with the same time on Lumi's screen and on the page.
  Why now: This is the kernel ("Lumi notices you"), and it also proves the whole chain works: server → page → USB → lamp. Setup (installing, starting, compiling) is part of this slice, so if anything is broken at the base, we find out first.
  PRD ref: `prd.md > The Core Journey` (steps 1–2, 5–6), `prd.md > Connecting the Lamp`, `prd.md > Noticing You (Presence Timer)`
  Spec ref: `spec.md > Where It Runs and How Someone Tries It`, `spec.md > Lamp Program`, `spec.md > USB Messages (Page ↔ Lamp)`, `spec.md > Web Page: Connection and Timer`
  Build: Run `npm install`. Check the server starts and serves the page. Compile the lamp sketch. Read the page's status-line handling and the `F<min>` sending, and fix anything that would make the screens disagree or ignore the chosen focus time.
  Verify (mechanical): `npm start` starts without errors; `GET /api/status` returns `ok: true` and `practice: false`; the page loads in headless Chrome with no script errors; `arduino-cli compile` succeeds for `lumi_lamp.ino`.
  Learner check: Run `npm start`, open http://localhost:8787 in Chrome or Edge, click **Connect lamp**, and pick 25 minutes. Lumi shows the eyes. Sit down: the bar fills for 5 seconds, then 25:00 starts counting down on Lumi's screen and on the page, with the page showing "focusing."
  Commit: `Verify connection and presence-started countdown`

- [x] **2. Leave, and Lumi pauses honestly; Focus mode for filming**
  Becomes usable: Getting up stops the countdown at once with "Where'd you go?", "Paused" after 10 seconds, resuming after 5 seconds seated, and cancelling after 5 minutes away. Focus mode shows only the timer, and leaving it keeps the countdown running.
  Why now: This is the other half of the kernel ("keeps me honest"). It's also the behavior we changed during planning, so it should be checked against the new PRD wording early.
  PRD ref: `prd.md > Pausing When You Leave`, `prd.md > Focus Mode`, `prd.md > States and Boundaries`
  Spec ref: `spec.md > Lamp Program`, `spec.md > Web Page: Connection and Timer`, `spec.md > Web Page: Focus Mode`
  Build: Check that the page's label follows the lamp's modes (focus `4`, paused `1`, away `0`), including when the session is cancelled after 5 minutes. Check that Focus mode hides everything except the timer and that Esc and the exit button restore the page. Fix any mismatch.
  Verify (mechanical): `arduino-cli compile` succeeds; the page loads in headless Chrome with no script errors; a code read confirms the page maps modes 0, 1, and 4 to the right labels, and the Focus mode CSS hides every section except the timer.
  Learner check: (1) **The USB investigation** from `spec.md > Decisions and Open Issues`: disconnect the page, open the Arduino IDE's Serial Monitor at 115200 baud, watch the mode number change from `0` to `3` to `4` as you sit, and from `4` to `1` as you leave. Type `MHELLO` and watch it appear on Lumi's screen. Then close the Serial Monitor. (2) Reconnect the page, turn on Focus mode, sit down, get up, and come back: check "Where'd you go?", "Paused", and resuming.
  Commit: `Verify honest pause and Focus mode`

- [ ] **3. At 00:00, a tune and "Done!"**
  Becomes usable: With a 1-minute custom focus time, the countdown reaches 00:00, Lumi plays the tune and shows "Done! Break time," and the page reacts to the finish.
  Why now: It completes the timer journey. It also tests the short custom time you'll use for filming.
  PRD ref: `prd.md > Finishing a Session`, `prd.md > Product Decisions` (film the finish with about 1 minute)
  Spec ref: `spec.md > Lamp Program`, `spec.md > USB Messages (Page ↔ Lamp)`, `spec.md > Where It Runs and How Someone Tries It`
  Build: Check that a 1-minute custom focus time is accepted by the page and sent to the lamp as `F1`, and that the page handles `E,DONE`. Fix anything that blocks a 1-minute session.
  Verify (mechanical): The page loads in headless Chrome with no script errors; a code read confirms the custom-minutes input allows 1 and sends `F1`, and that `E,DONE` is handled; `arduino-cli compile` succeeds.
  Learner check: Set a custom focus time of 1 minute, sit down, and wait. At 00:00, Lumi plays a tune and shows "Done! Break time," and the page shows the session finished.
  Commit: `Verify 1-minute session finish`

- [ ] **4. The AI study buddy quizzes you on your notes**
  Becomes usable: Upload notes, click **Quiz me (5 questions)**, and get 5 questions, one at a time, each labelled "Question N of 5 · Topic", graded with an explanation when wrong, ending with "Quiz done: X/5 correct (Y%)" and what to review next. An AI error shows a friendly message and keeps the quiz progress. Quiz me with no notes asks you to upload first.
  Why now: It's the AI moment. It depends on Gemini, which is outside our control, so it comes after the timer is solid. The "AI error keeps progress" rule was flagged as *check in build* in the spec.
  PRD ref: `prd.md > AI Study Buddy Quiz`, `prd.md > States and Boundaries` (AI problem)
  Spec ref: `spec.md > Web Page: Study Buddy Quiz`, `spec.md > Study Server`, `spec.md > External Services and Dependencies`, `spec.md > Important Failure Modes`
  Build: Check the quiz flow in the page. Check what happens when a question or grading request fails, and fix it if an error ends the quiz or loses the score, so you can retry the same question. Make sure the error message is friendly and matches the cozy style.
  Verify (mechanical): With the server running, upload a small sample notes file (`POST /api/upload`), then call `POST /api/quiz/question` and `POST /api/quiz/grade` and confirm they return `{topic, question, answer}` and `{correct, feedback}`; call `/api/quiz/question` with no notes and confirm the friendly "Add a course file…" message; the page loads in headless Chrome with no script errors.
  Learner check: Upload your own notes to the Course library, scroll to the Study buddy, click **Quiz me (5 questions)**, and answer all 5 (get at least one wrong on purpose). Check the "Question N of 5 · Topic" label, the explanation when wrong, and the final score. Then turn off Wi-Fi, start another quiz, answer a question, and check that you see a friendly message and that the quiz continues after Wi-Fi is back on.
  Commit: `Verify AI quiz and keep progress on AI errors`

- [ ] **5. Weak spots show on Lumi's screen**
  Becomes usable: Missing a topic more often than getting it right sends `REVIEW: TOPIC` to Lumi's screen, and long topics still fit within the lamp's 21 characters.
  Why now: It connects the AI to the lamp, so it needs both the connection (slice 1) and the quiz (slice 4) to be working first.
  PRD ref: `prd.md > Weak Spots on the Lamp`
  Spec ref: `spec.md > Web Page: Study Buddy Quiz`, `spec.md > USB Messages (Page ↔ Lamp)`, `spec.md > Important Failure Modes` (long weak-spot topic)
  Build: Check how the page shortens text for the lamp before sending `M` messages. If a long topic like `REVIEW: PHOTOSYNTHESIS` would be cut off badly, shorten it cleanly so it still reads well on the screen.
  Verify (mechanical): A small Node script runs the page's lamp-text function on short and long topics and confirms every result is 21 characters or fewer and starts with `REVIEW:`; the page loads in headless Chrome with no script errors.
  Learner check: During a quiz with the lamp connected, get the same topic wrong twice. Lumi's screen shows `REVIEW: TOPIC` for a few seconds, then goes back to the countdown.
  Commit: `Verify weak spots on the lamp screen`

## Hands-on Checkpoints

- [x] Early usable behavior explored — after slice 2 (the whole kernel: sit down, leave, come back)
- [ ] Final kick-the-tires exploration and feedback completed

## Final Review

- [ ] Final review complete — feedback resolved and learner confirms ready to ship

## Code Tour and App Map

- [ ] Learning activity complete — guided route, focused alternative, prior practice connected, or brief recap
- [ ] Optional edit and transfer reflection addressed — offered/declined/already covered/not applicable as appropriate
- [ ] `devpost/app-map.html` generated from finished code, checked, and shown, including a project-grounded practice to reuse

Activity and evidence: 
Route and stops: 
Edit outcome: 
Reflection: 
Activity mode: 

## Revisions
