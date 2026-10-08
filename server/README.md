# Lumi Study Server

This is the server-side AI layer for Lumi / Omaima's Study Lamp.

## What it does

- Keeps the Nebius API key out of the browser.
- Serves the existing `web/index.html` app.
- `GET /api/status` reports whether Nebius is configured.
- `POST /api/upload` extracts and indexes course files.
- `POST /api/chat` retrieves relevant course passages and sends a grounded prompt to NVIDIA Nemotron through Nebius Token Factory.
- `POST /api/quiz/question` and `POST /api/quiz/grade` run the topic-tagged quiz used to find weak spots.
- `POST /api/session/start|event|end` log a lamp study session and write the end-of-session report.

No key yet? Set `PRACTICE_MODE=1` in `.env` for clearly labelled pretend answers.

Supported upload formats: PDF (text-based, not scanned), DOCX, TXT, Markdown, CSV, HTML, JavaScript, TypeScript, Python, and JSON.

## Run

From the project root:

```bash
cp .env.example .env
# put your real NEBIUS_API_KEY in .env
npm install
npm start
```

Then open `http://localhost:8787`.

The browser never receives `NEBIUS_API_KEY`; only this server uses it.
