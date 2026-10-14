---
doc: scope
status: approved
---

# Lumi

A friendly desk study companion that notices when you sit down, counts down your study time, and pauses when you leave.

## The Unique Kernel
Lumi **notices you**. You don't start a timer. You sit down and it starts. When you walk away, it asks "where'd you go?" and pauses. This keeps you honest about how long you really studied, and the cute animated eyes make it fun instead of boring.

## Who It's For
The learner first, and any other student with a short attention span who wants to stay focused and get better scores. They have used study timers for a long time, and "at this point they feel boring." A normal timer keeps counting while you're away, and it has no personality.

## The Core Loop
1. Open the web page, connect Lumi over USB, and pick a study time (for example, 25 or 40 minutes).
2. Lumi's screen shows friendly animated eyes while nobody is there.
3. Sit down within about 40 cm. A bar fills for 5 seconds, then the countdown starts on Lumi's screen and on the web page.
4. While studying, upload your notes and let the AI study buddy quiz you on them.
5. Get up, and the screen asks "where'd you go?" After 10 seconds, the timer pauses. Sit back down for 5 seconds, and it continues.
6. At 00:00, Lumi plays a tune and shows "DONE."

Students come back because it keeps them on track in a fun way, and the time it shows is time they really studied.

## Inspiration & Identity
Cute, friendly, and encouraging. It's a small companion with eyes, not a strict timer. Later it will live inside a real lamp.

## Why This Matters to the Learner
"What I like the most is that it keeps me honest about how long I studied, without any distractions and losing focus." It's also a chance to show a working AI project at an AI hackathon.

## What "Working" Looks Like
One smooth study session, start to finish, recorded in a short demo video. The video shows the screen and the learner's hands; the learner won't appear on camera.

**The "oh, that's cool" moment:** the friendly eyes are waiting, the learner sits down, and the countdown starts by itself.

The video shows, in order:
1. Friendly eyes on the screen while nobody is there.
2. Sitting down starts the countdown.
3. Leaving pauses it ("where'd you go?").
4. The AI study buddy quizzing the learner on their own notes.
5. The tune and "DONE" at 00:00.

## The POC Boundary
**In:**
- Connecting Lumi to the web page over USB, and choosing the study time
- Friendly animated eyes when nobody is there
- Sit down → 5-second bar → countdown starts (on Lumi's screen and the web page)
- Leave → "where'd you go?" → pause after 10 seconds → resume after sitting back down for 5 seconds
- Tune and "DONE" at 00:00
- Uploading notes, and the AI study buddy (Gemini) quizzing the learner on them

Most of this is already built. This hackathon is about making it run smoothly as one session and getting it ready for the demo, not about building new features.

## Later
- **Session report:** a short AI-written summary at the end. A "nice extra if there's time."
- **Break timer** after "DONE"
- **AI explanations** of the learner's notes (the quiz is the one AI moment for the demo)
- **Weak spots** shown on Lumi's screen (for example, `REVIEW: METAPHASE`)
- **Water reminders:** after a set time, Lumi reminds you to drink water and waits for you (raised in the final review; skipped for this submission)
- Fitting the hardware inside a real lamp

## Explicitly Cut
Nothing. The learner didn't name anything to drop. The focus is on showing and polishing the parts listed above, and the code for the "Later" features can stay in the project.
