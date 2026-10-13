---
doc: prd
status: approved
---

# Lumi — Product Requirements

Lumi is a cozy desk study companion for students with short attention spans. It notices when you sit down, counts down your study time, pauses when you leave, and has an AI study buddy that quizzes you on your own notes.
Source: `scope.md > The Unique Kernel`, `scope.md > Who It's For`.

This document has two parts:
1. **The demo session.** This is what the proof of concept must do smoothly, and it's where the build time goes. It covers everything from **The Core Journey** to **States and Boundaries**.
2. **Full Feature List.** This lists everything else the app already has, described as it works today. These features stay on the page but are not checked or fixed during the build.

## The Core Journey
Source: `scope.md > The Core Loop`, `scope.md > What "Working" Looks Like`.

**Before the session**
1. Open the web page and click **Connect lamp**. The lamp is connected over USB.
2. In **Session settings**, pick the focus time (25, 40, 50, or 60 minutes, or a custom time).
3. Upload your notes to the **Course library**.

**The timer moments** (filmed in Focus mode)
4. Turn on **Focus mode**. The page now shows only the study timer.
5. Nobody is at the desk. Lumi's screen shows friendly, animated eyes.
6. Sit down within about 40 cm. A bar fills for 5 seconds, then the countdown starts on Lumi's screen and on the web page. *This is the "oh, that's cool" moment.*
7. Get up. The countdown stops right away, and Lumi's screen asks "Where'd you go?" with a 10-second countdown. Come back within 10 seconds, and it continues. After 10 seconds, it shows "Paused."
8. Sit back down for 5 seconds. The countdown continues from where it stopped.

**The AI moment** (the countdown keeps running)
9. Leave Focus mode and scroll down to the **Study buddy**.
10. Click **Quiz me (5 questions)**. The study buddy asks one question at a time, based on your own notes. Each question shows its number and topic, for example "Question 2 of 5 · Mitosis."
11. Type an answer and press Enter. The AI says whether you're correct. If you're wrong, it explains the right answer.
12. If you keep missing a topic, it becomes a weak spot, and Lumi's screen shows `REVIEW: MITOSIS`.
13. After 5 questions, the study buddy shows "Quiz done: 4/5 correct (80%)," lists your weak spots, and suggests what to review next, for example "Spend 15 minutes reviewing mitosis, then quiz again."

**The finish**
14. The countdown reaches 00:00. Lumi plays a tune and shows "Done!"

**Success:** one smooth session, start to finish, that can be recorded as a short demo video showing only the screens and the learner's hands.

## Screens and Layout
Lumi has two surfaces: **the web page** and **Lumi's small screen**. The page layout stays exactly as it is today.

**The web page, from top to bottom:**
1. **Top bar:** the title, the **Connect lamp** button, and **Try demo** (for when the lamp isn't plugged in).
2. **Quick start:** a short checklist.
3. **Study timer:** the big main card. It has a cute character, a round progress ring, the countdown in the middle, and a label showing whether you're focusing, paused, or on a break.
4. **Today's tasks**, **Your progress**, and **Session settings**.
5. **Note on the lamp:** a box for sending a short message to Lumi's screen.
6. **Today's mission.**
7. **Course library**, **Notebook**, and **Study buddy**.

**Focus mode** hides everything except the study timer. A button or the Esc key brings the full page back.

**Lumi's screen** shows the friendly eyes, the 5-second "sitting down" bar, the countdown, "Where'd you go?", "Done!", and short messages like `REVIEW: MITOSIS`.

## Look and Feel
Source: `scope.md > Inspiration & Identity`.

- **Mood:** cozy, calm, and cute, "like a friendly study buddy, not a strict timer." In the learner's words, "almost like a comfort zone with a study tracker and a study partner."
- **Web page:** dark background with soft lilac and lavender colors, rounded cards, a cute character, and small touches like ♡ and ✦.
- **Lumi's screen:** the eyes should feel alive and playful.
- **Avoid:** anything that looks like a boring office tool, is too bright or harsh, or feels cluttered and stressful.
- **Rule for the build:** this style already exists. Any change must match it, not replace it.

## Features and Behavior

### Connecting the Lamp
- [ ] Clicking **Connect lamp** connects Lumi over USB, and the page shows that it's connected.
- [ ] The focus time picked in **Session settings** is the one the countdown uses, on both Lumi's screen and the page.

### Noticing You (Presence Timer)
Source: `scope.md > The Unique Kernel`. This is the heart of Lumi.
- [ ] With nobody within about 40 cm, Lumi's screen shows animated eyes, and the countdown doesn't run.
- [ ] Sitting down within about 40 cm fills a bar on Lumi's screen for 5 seconds, then the countdown starts.
- [ ] The countdown on Lumi's screen and on the web page show the same time.
- [ ] The page's label shows "focusing" while the countdown runs.

### Pausing When You Leave
Updated during `4-spec`: the learner chose to keep the lamp's existing behavior, so time away from the desk never counts as study time.
- [ ] The moment you leave, the countdown stops, and Lumi's screen shows "Where'd you go?" with a 10-second countdown.
- [ ] If you come back within 10 seconds, the countdown continues right away.
- [ ] After 10 seconds away, Lumi's screen shows "Paused," and the page's label shows "paused."
- [ ] After you sit back down for 5 seconds, the countdown continues from the same time. It doesn't restart.
- [ ] If you stay away for 5 minutes, the session is cancelled.

### Finishing a Session
- [ ] At 00:00, Lumi plays a tune and its screen shows "Done!"

### Focus Mode
- [ ] Turning on Focus mode leaves only the study timer visible.
- [ ] Leaving Focus mode brings the full page back, and the countdown keeps running.

### AI Study Buddy Quiz
Source: `scope.md > The POC Boundary`.
- [ ] Notes uploaded to the Course library can be used by the quiz.
- [ ] **Quiz me (5 questions)** asks one question at a time, about the uploaded notes.
- [ ] Each question shows its number and topic, for example "Question 2 of 5 · Mitosis."
- [ ] After you type an answer and press Enter, the AI says whether you're correct. If you're wrong, it explains the right answer.
- [ ] After 5 questions, it shows the score (for example, "Quiz done: 4/5 correct (80%)"), lists your weak spots, and suggests what to review next.

### Weak Spots on the Lamp
The learner moved this from `scope.md > Later` into the demo, because "it connects the AI to the lamp."
- [ ] A topic becomes a weak spot when you've missed it more often than you've gotten it right.
- [ ] When a new weak spot appears, Lumi's screen shows `REVIEW: TOPIC`, for example `REVIEW: MITOSIS`.

## States and Boundaries
- **Nobody there:** friendly animated eyes, and the countdown doesn't run.
- **Focusing:** the countdown runs on both screens, and the page shows "focusing."
- **Away:** the countdown stops at once. "Where'd you go?" on Lumi's screen, then "Paused" after 10 seconds. After 5 minutes away, the session is cancelled.
- **Done:** a tune and "Done!" on Lumi's screen.
- **AI problem** (the internet drops, or Gemini returns an error): **an AI problem never stops the session.** The timer and the lamp keep working normally, and the quiz progress so far isn't lost.
- **Lamp unplugged mid-session:** the web page shows a friendly "Lost the lamp connection, plug it back in" message. Lumi runs on USB power, so it turns off, but it saves today's total study time every minute, so at most one minute is lost.

## Product Decisions
- **Keep every section on the page, and don't change the layout.** The PRD describes the demo first and lists everything else in **Full Feature List**, "so the doc shows the full app, while the video stays focused."
- **Describe the extras as they are, and spend the build time on the demo.**
- **Use Focus mode for the timer moments, and scroll to the Study buddy for the quiz.**
- **Time away never counts** (decided in `4-spec`): the countdown stops the moment you leave, and a session is cancelled after 5 minutes away. This keeps the lamp's existing behavior.
- **The AI moment is the quiz.** This settles the earlier "explain or quiz" question.
- **Weak spots on Lumi's screen are in the demo,** because they connect the AI to the lamp.
- **An AI problem never stops the session,** and quiz progress isn't lost.
- **The learner won't appear on camera.** The video shows the screens and the learner's hands.
- **Film the finish with a short custom focus time** (about 1 minute), so "Done!" can be shown without waiting.
- **Lumi's screen says "Done!"** (currently "Done! Break time"), not "DONE." The learner confirmed this is fine.
- **The look stays cozy, calm, and cute,** and any change must match the existing style.

## What We're Building
Everything in the demo session already exists. The work is to make it run smoothly and reliably for the video:
- Connecting the lamp and choosing the focus time
- Friendly eyes → sitting down → 5-second bar → countdown
- Leaving → countdown stops → "Where'd you go?" → "Paused" → resume
- Tune and "Done!" at 00:00
- Focus mode for the timer moments
- Uploading notes and the 5-question AI quiz
- Weak spots shown on Lumi's screen as `REVIEW: TOPIC`
- An AI problem never stopping the session

## Full Feature List
These features are already in the app, and they stay on the page. They're described as they work today. **They are not part of the demo, and they won't be checked or fixed during the build.**

- **Try demo:** lets you try the page with a pretend lamp when the device isn't plugged in.
- **Quick start:** a short checklist for getting set up.
- **Today's tasks:** a list of today's study tasks.
- **Your progress:** today's study time, the number of sessions, and your streak.
- **Session settings:** the focus time (25, 40, 50, or 60 minutes, or custom) and the break length. *(Picking the focus time is part of the demo; the break length is not.)*
- **Note on the lamp:** type a short message and send it to Lumi's screen.
- **Today's mission:** a daily study goal with progress.
- **Course library:** upload your notes (PDF, Word, or text). *(Uploading notes is part of the demo.)*
- **Notebook:** write and keep your own notes.
- **Study buddy buttons:** **Explain my note**, **Summarize**, **Make flashcards**, and **Find my weak areas**. *(**Quiz me** is part of the demo.)*
- **Break timer:** after "Done!", a break countdown starts.
- **Session report:** a short AI-written summary at the end of a session.

## Deferred From the POC
- **Session report.** Already in the app, but not polished or shown in the demo. It's "a nice extra if there's time" (`scope.md > Later`). Only work on it after the demo session is finished.
- **Break timer, AI explanations, and the other extras** in the Full Feature List. They stay as they are because the build time goes to the demo.

## Possible Later Enhancements
- Fitting the hardware inside a real lamp (`scope.md > Later`).
- Checking and polishing the Full Feature List extras.

## Non-Goals
- **No layout changes or redesign.** The learner wants the page to stay as it is.
- **No new features.** This hackathon is about making the existing demo session smooth.
- **No fixing of the extras** unless they get in the way of the demo.

## Open Questions
None of these block `4-spec`. They can be decided or checked during the build.
- **Assumption: the AI error message.** When the AI can't answer, the study buddy shows a short, friendly message and you can try again. The exact wording can be decided during the build, as long as it matches the cozy style.
- **Assumption: Quiz me with no notes uploaded.** The study buddy shows a friendly message asking you to upload notes first.
- **Resolved: the lamp gets unplugged mid-session.** See **States and Boundaries**.
- **Resolved: filming "Done!".** Use a short custom focus time of about 1 minute. See **Product Decisions**.
