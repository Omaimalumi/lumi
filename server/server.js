const fs = require('fs');
const path = require('path');

function loadDotEnv(file) {
  try {
    const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/);
    for (const line of lines) {
      const m = line.match(/^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$/);
      if (!m || m[1] in process.env) continue;
      process.env[m[1]] = m[2].replace(/^['"]|['"]$/g, '');
    }
  } catch {}
}

loadDotEnv(path.resolve(__dirname, '..', '.env'));

const express = require('express');
const multer = require('multer');
const crypto = require('crypto');
const mammoth = require('mammoth');
const JSZip = require('jszip');
// unpdf uses a modern pdf.js, so it reads PDFs that the old pdf-parse rejected ("bad XRef entry")
let unpdfModule = null;
async function readPdfText(buffer) {
  unpdfModule = unpdfModule || await import('unpdf');
  const pdf = await unpdfModule.getDocumentProxy(new Uint8Array(buffer));
  const { text } = await unpdfModule.extractText(pdf, { mergePages: true });
  return text;
}

const ROOT = path.resolve(__dirname, '..');
const WEB_DIR = path.join(ROOT, 'web');
const DATA_DIR = path.join(__dirname, 'data');
const COURSE_DIR = path.join(DATA_DIR, 'courses');
const SESSION_DIR = path.join(DATA_DIR, 'sessions');
const PORT = Number(process.env.PORT || 8787);
// On your own computer the server only listens to this computer (127.0.0.1).
// Online (Render), set HOST=0.0.0.0 so visitors can reach it.
const HOST = process.env.HOST || '127.0.0.1';
// Online, set APP_PASSWORD so strangers can't use up your Nebius credits.
// Leave it empty on your own computer and no password is asked.
const APP_PASSWORD = (process.env.APP_PASSWORD || '').trim();
// The placeholder from .env.example counts as "no key yet".
const NEBIUS_API_KEY = /^(paste-your-key-here)?$/i.test((process.env.NEBIUS_API_KEY || '').trim()) ? '' : process.env.NEBIUS_API_KEY.trim();
const NEBIUS_MODEL = process.env.NEBIUS_MODEL || 'nvidia/nemotron-3-super-120b-a12b';
const NEBIUS_BASE_URL = (process.env.NEBIUS_BASE_URL || 'https://api.tokenfactory.nebius.com/v1').replace(/\/$/, '');
// PRACTICE_MODE=1 (and no API key) lets you click through every feature with pretend answers.
// It is clearly labelled in the page and is never used once a real key is set.
const PRACTICE = !NEBIUS_API_KEY && /^(1|true|yes)$/i.test(process.env.PRACTICE_MODE || '');
const AI_READY = Boolean(NEBIUS_API_KEY) || PRACTICE;

for (const dir of [DATA_DIR, COURSE_DIR, SESSION_DIR]) fs.mkdirSync(dir, { recursive: true });

const app = express();

// Health check stays open so the host can see the server is alive.
app.get('/api/health', (_req, res) => res.json({ ok: true }));

// Password gate: the browser shows its own login box. Any username works; only the password is checked.
if (APP_PASSWORD) {
  const want = crypto.createHash('sha256').update(APP_PASSWORD).digest();
  app.use((req, res, next) => {
    const [type, value] = String(req.headers.authorization || '').split(' ');
    if (type === 'Basic' && value) {
      const decoded = Buffer.from(value, 'base64').toString('utf8');
      const given = decoded.slice(decoded.indexOf(':') + 1);
      const got = crypto.createHash('sha256').update(given).digest();
      if (crypto.timingSafeEqual(want, got)) return next();
    }
    res.set('WWW-Authenticate', 'Basic realm="Lumi Study Lamp", charset="UTF-8"');
    res.status(401).send('Password required.');
  });
}

app.use(express.json({ limit: '1mb' }));
app.use(express.urlencoded({ extended: true }));

const upload = multer({
  storage: multer.memoryStorage(),
  limits: { files: 10, fileSize: 60 * 1024 * 1024 }
});

function safeName(value) {
  return String(value || 'course').replace(/[^a-zA-Z0-9._-]+/g, '_').slice(0, 100) || 'course';
}

function coursePath(courseId) {
  return path.join(COURSE_DIR, safeName(courseId) + '.json');
}

function readCourse(courseId) {
  try { return JSON.parse(fs.readFileSync(coursePath(courseId), 'utf8')); }
  catch { return null; }
}

function writeCourse(course) {
  fs.writeFileSync(coursePath(course.id), JSON.stringify(course, null, 2));
}

function sessionPath(id) { return path.join(SESSION_DIR, safeName(id) + '.json'); }
function readSession(id) { try { return JSON.parse(fs.readFileSync(sessionPath(id), 'utf8')); } catch { return null; } }
function writeSession(session) { fs.writeFileSync(sessionPath(session.id), JSON.stringify(session, null, 2)); }

function cleanText(text) {
  return String(text || '')
    .replace(/\u0000/g, ' ')
    .replace(/\r/g, '')
    .replace(/[ \t]+\n/g, '\n')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

function chunkText(text, size = 2200, overlap = 350) {
  const out = [];
  const normalized = cleanText(text);
  if (!normalized) return out;
  let start = 0;
  while (start < normalized.length) {
    let end = Math.min(normalized.length, start + size);
    if (end < normalized.length) {
      const boundary = Math.max(normalized.lastIndexOf('\n\n', end), normalized.lastIndexOf('. ', end));
      if (boundary > start + Math.floor(size * 0.55)) end = boundary + (normalized[boundary] === '.' ? 1 : 0);
    }
    out.push(normalized.slice(start, end).trim());
    if (end >= normalized.length) break;
    start = Math.max(start + 1, end - overlap);
  }
  return out.filter(Boolean);
}

function terms(text) {
  return cleanText(text).toLowerCase().match(/[a-z0-9]{2,}/g) || [];
}

function retrieve(course, query, limit = 6) {
  const q = terms(query);
  if (!q.length || !course.chunks?.length) return [];
  const counts = new Map();
  for (const t of q) counts.set(t, (counts.get(t) || 0) + 1);
  return course.chunks.map((chunk, index) => {
    const words = terms(chunk.text);
    const set = new Set(words);
    let score = 0;
    for (const [term, qCount] of counts) if (set.has(term)) score += 1 + Math.min(qCount, 3) * 0.25;
    // Small bonus for exact phrases and headings-like lines.
    const low = chunk.text.toLowerCase();
    const qLow = cleanText(query).toLowerCase();
    if (qLow.length > 8 && low.includes(qLow)) score += 8;
    return { ...chunk, index, score };
  }).filter(x => x.score > 0).sort((a, b) => b.score - a.score).slice(0, limit);
}

async function extractFile(file) {
  const ext = path.extname(file.originalname).toLowerCase();
  if (ext === '.pdf') {
    let raw;
    try { raw = await readPdfText(file.buffer); }
    catch { throw new Error(`${file.originalname} couldn't be opened as a PDF. It may be damaged or password-protected.`); }
    const text = cleanText(raw);
    if (!text) throw new Error(`${file.originalname} has no selectable text (it may be a scanned book). Try a text-based PDF or paste the chapter as a .txt file.`);
    return text;
  }
  if (ext === '.pptx') {
    // A .pptx is a zip of slide XML files; the visible text sits in <a:t> tags.
    const zip = await JSZip.loadAsync(file.buffer);
    const slides = Object.keys(zip.files).filter(n => /^ppt\/slides\/slide\d+\.xml$/.test(n))
      .sort((a, b) => Number(a.match(/\d+/)[0]) - Number(b.match(/\d+/)[0]));
    const parts = [];
    for (const name of slides) {
      const xml = await zip.file(name).async('string');
      const text = [...xml.matchAll(/<a:t>([^<]*)<\/a:t>/g)].map(m => m[1]).join(' ')
        .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&apos;/g, "'");
      if (text.trim()) parts.push(`Slide ${parts.length + 1}: ${text}`);
    }
    return cleanText(parts.join('\n\n'));
  }
  if (ext === '.doc' || ext === '.ppt') {
    throw new Error(`${file.originalname} is an old Office format. Open it and use "Save As" to save it as ${ext === '.doc' ? '.docx' : '.pptx'}, then upload that.`);
  }
  if (ext === '.docx') {
    const data = await mammoth.extractRawText({ buffer: file.buffer });
    return cleanText(data.value);
  }
  if (['.txt', '.md', '.markdown', '.csv', '.html', '.htm', '.js', '.ts', '.py', '.json'].includes(ext)) {
    return cleanText(file.buffer.toString('utf8'));
  }
  throw new Error(`${file.originalname}: this file type isn't supported. Use PDF, Word (.docx), PowerPoint (.pptx), TXT or Markdown.`);
}

function buildSystemPrompt(baseRules, course, retrieved, note) {
  const sourceBlock = retrieved.length
    ? retrieved.map((x, i) => `SOURCE ${i + 1} — ${x.fileName}\n${x.text}`).join('\n\n')
    : 'No matching course passages were found for this question.';
  return `${baseRules || "You are Omaima's kind, patient study buddy."}\n\n` +
    `COURSE: ${course?.name || 'No course selected'}\n` +
    `GROUNDING RULES:\n` +
    `- Use the supplied course passages as the primary source when answering course questions.\n` +
    `- If the passages do not contain the answer, say that the uploaded material does not cover it before using general knowledge.\n` +
    `- Never invent a fact, quotation, citation, page number, or source.\n` +
    `- If the student asks for a quiz, ask exactly one question at a time.\n` +
    `- Keep the answer practical and appropriate for a student.\n\n` +
    `RELEVANT COURSE PASSAGES:\n${sourceBlock}` +
    (note ? `\n\nTHE STUDENT'S CURRENT NOTEBOOK NOTE (use it when they mention "my note"):\n${note}` : '');
}

async function nebiusChat(messages, options = {}) {
  if (!NEBIUS_API_KEY && PRACTICE) return practiceReply(options);
  if (!NEBIUS_API_KEY) throw new Error('NEBIUS_API_KEY is not configured on the study server.');
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), options.timeoutMs || 90000);
  try {
    const response = await fetch(`${NEBIUS_BASE_URL}/chat/completions`, {
      method: 'POST',
      signal: controller.signal,
      headers: {
        'Authorization': `Bearer ${NEBIUS_API_KEY}`,
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        model: NEBIUS_MODEL,
        messages,
        temperature: 0.35,
        max_tokens: options.maxTokens || 900
      })
    });
    const raw = await response.text();
    let data;
    try { data = JSON.parse(raw); } catch { data = { error: raw }; }
    if (!response.ok) {
      const detail = data?.error?.message || data?.message || data?.error || `HTTP ${response.status}`;
      throw new Error(`Nebius request failed: ${detail}`);
    }
    // Nemotron can include its private reasoning in <think> tags — keep only the answer.
    const text = String(data?.choices?.[0]?.message?.content || '').replace(/<think>[\s\S]*?<\/think>/gi, '').trim();
    if (!text) throw new Error('Nebius returned no assistant text.');
    return text;
  } finally {
    clearTimeout(timer);
  }
}

// The model is asked for JSON; pull the first {...} block out of whatever it returns.
function parseJsonReply(text) {
  const m = String(text || '').match(/\{[\s\S]*\}/);
  if (m) { try { return JSON.parse(m[0]); } catch {} }
  throw new Error('The AI answered in an unexpected format. Please try again.');
}
function toList(value, max) {
  return (Array.isArray(value) ? value : []).map(x => cleanText(x).slice(0, 200)).filter(Boolean).slice(-max);
}
function truthy(v) { return v === true || /^(true|yes|correct)$/i.test(String(v).trim()); }

// ---------- Practice mode: simple pretend answers so the whole flow can be tested without a key ----------
const STOP = new Set('the and for are was were that this with from into have has had their they them then than when what which while where your you our its it his her she him who whom why how can could would should will about there these those been being also only just very more most some such each other over under after before during between both does done doing make made many much must like into onto upon within without'.split(' '));
function keyTerms(text) { return [...new Set(terms(text).filter(w => w.length > 3 && !STOP.has(w)))]; }
function sentences(text) { return cleanText(text).split(/(?<=[.!?])\s+|\n+/).map(x => x.trim()).filter(x => x.length > 25 && x.length < 400); }
function practiceReply(o) {
  const tag = '(Practice mode, not the real AI) ';
  if (o.kind === 'quiz-question') {
    const all = sentences(o.material || '');
    const fresh = all.filter(x => !(o.asked || []).some(a => a.includes(x.slice(0, 40))));
    const pool = fresh.length ? fresh : all;
    const weak = (o.weak || []).map(w => w.toLowerCase());
    const pick = pool.find(x => weak.some(w => x.toLowerCase().includes(w))) || pool[Math.floor(Math.random() * pool.length)];
    if (!pick) throw new Error('Practice mode needs a few full sentences in your note or course to make a question.');
    const word = weak.find(w => pick.toLowerCase().includes(w)) || keyTerms(pick).sort((a, b) => b.length - a.length)[0] || 'this idea';
    const topic = word.charAt(0).toUpperCase() + word.slice(1);
    return JSON.stringify({ topic, question: `${tag}What does your material say about "${word}"?`, answer: pick });
  }
  if (o.kind === 'quiz-grade') {
    const want = keyTerms(o.answer), got = new Set(keyTerms(o.studentAnswer));
    const hit = want.filter(w => got.has(w)).length;
    const correct = want.length ? hit / want.length >= 0.4 : false;
    return JSON.stringify({ correct, feedback: tag + (correct ? 'That matches your material.' : `Your material says: ${o.answer}`) });
  }
  if (o.kind === 'chat') {
    const p = (o.passages || [])[0];
    return tag + (p ? `Here is the part of your material that matches your question:\n\n${p.text.slice(0, 700)}` : 'Once your Nebius API key is set up, NVIDIA Nemotron will answer here.');
  }
  return null; // reports fall back to the built-in summary
}

app.get('/api/status', (_req, res) => {
  res.json({
    ok: AI_READY,
    practice: PRACTICE,
    provider: PRACTICE ? 'Practice mode (no API key)' : 'Nebius Token Factory',
    model: NEBIUS_MODEL,
    configured: Boolean(NEBIUS_API_KEY)
  });
});

// Turn upload-limit errors into a clear message instead of an error page.
function uploadFiles(req, res, next) {
  upload.array('files', 10)(req, res, err => {
    if (!err) return next();
    const msg = err.code === 'LIMIT_FILE_SIZE' ? 'That file is bigger than 60 MB. Upload one chapter at a time.'
      : err.code === 'LIMIT_FILE_COUNT' ? 'Upload at most 10 files at once.' : err.message;
    res.status(400).json({ error: msg });
  });
}

app.post('/api/upload', uploadFiles, async (req, res) => {
  try {
    const courseId = safeName(req.body.courseId || `course_${Date.now().toString(36)}`);
    const courseName = cleanText(req.body.courseName || 'Untitled course').slice(0, 160);
    if (!req.files?.length) return res.status(400).json({ error: 'No files were uploaded.' });

    const files = [];
    const chunks = [];
    for (const file of req.files) {
      const text = await extractFile(file);
      if (!text) throw new Error(`${file.originalname} did not contain readable text.`);
      const fileChunks = chunkText(text).map((chunk, i) => ({ id: crypto.createHash('sha1').update(`${file.originalname}:${i}:${chunk}`).digest('hex').slice(0, 12), fileName: file.originalname, text: chunk }));
      chunks.push(...fileChunks);
      files.push({ name: file.originalname, size: file.size, type: file.mimetype, characters: text.length, chunks: fileChunks.length });
    }

    const course = { id: courseId, name: courseName, created: Date.now(), files, chunks };
    writeCourse(course);
    res.json({ ok: true, courseId, files });
  } catch (error) {
    console.error('Upload error:', error);
    res.status(400).json({ error: error.message || 'Upload failed.' });
  }
});

app.post('/api/chat', async (req, res) => {
  try {
    const message = cleanText(req.body.message);
    if (!message) return res.status(400).json({ error: 'Message is required.' });

    const course = req.body.courseId ? readCourse(req.body.courseId) : null;
    const retrieved = course ? retrieve(course, message, 6) : [];
    const note = cleanText(req.body.note).slice(0, 6000);
    const system = buildSystemPrompt(req.body.system, course, retrieved, note);

    const history = Array.isArray(req.body.history) ? req.body.history.slice(-10).filter(x => x && ['user', 'assistant'].includes(x.role) && typeof x.content === 'string') : [];
    const messages = [{ role: 'system', content: system }, ...history, { role: 'user', content: message }];
    const text = await nebiusChat(messages, { kind: 'chat', passages: retrieved });
    res.json({ ok: true, text, model: NEBIUS_MODEL, grounded: Boolean(course && retrieved.length), sources: retrieved.map(x => ({ fileName: x.fileName, chunkId: x.id })) });
  } catch (error) {
    console.error('Chat error:', error);
    const message = error.name === 'AbortError' ? 'The AI request timed out. Try again.' : error.message || 'The study buddy could not answer.';
    res.status(500).json({ error: message });
  }
});

// ---------- Quiz: one question at a time, graded, tagged with a topic so weak spots can be found ----------
app.post('/api/quiz/question', async (req, res) => {
  try {
    const course = req.body.courseId ? readCourse(req.body.courseId) : null;
    const note = cleanText(req.body.note).slice(0, 6000);
    const weak = toList(req.body.weakTopics, 5);
    const asked = toList(req.body.asked, 12);
    let passages = course ? retrieve(course, [weak.join(' '), note.slice(0, 500)].join(' '), 4) : [];
    if (course && !passages.length && course.chunks?.length) {
      passages = [...course.chunks].sort(() => Math.random() - 0.5).slice(0, 3);
    }
    const material = [...passages.map(x => x.text), note].filter(Boolean).join('\n\n---\n\n').slice(0, 9000);
    if (!material.trim()) return res.status(400).json({ error: 'Add a course file or write a note first, so I know what to quiz you on.' });

    const system = `You write ONE short quiz question for a student, using ONLY the study material below.\n` +
      `Reply with ONLY a JSON object and nothing else: {"topic": "...", "question": "...", "answer": "..."}\n` +
      `- topic: the specific concept being tested, 1 to 3 words (for example "Metaphase").\n` +
      `- question: one clear question that can be answered in a sentence or two.\n` +
      `- answer: the correct answer in one or two sentences, taken from the material.\n` +
      (weak.length ? `- The student keeps missing: ${weak.join(', ')}. Ask about one of these from a new angle.\n` : `- Pick an important concept.\n`) +
      (asked.length ? `- Do not repeat these questions: ${asked.join(' | ')}\n` : '') +
      `\nSTUDY MATERIAL:\n${material}`;
    const raw = await nebiusChat([{ role: 'system', content: system }, { role: 'user', content: 'Write the next question now.' }],
      { kind: 'quiz-question', material, weak, asked, maxTokens: 1500 });
    const q = parseJsonReply(raw);
    const topic = cleanText(q.topic).slice(0, 40) || 'General';
    const question = cleanText(q.question).slice(0, 600);
    const answer = cleanText(q.answer).slice(0, 800);
    if (!question || !answer) throw new Error('The AI did not write a usable question. Please try again.');
    res.json({ ok: true, topic, question, answer, practice: PRACTICE });
  } catch (error) {
    console.error('Quiz question error:', error);
    res.status(500).json({ error: error.name === 'AbortError' ? 'The AI request timed out. Try again.' : error.message });
  }
});

app.post('/api/quiz/grade', async (req, res) => {
  try {
    const question = cleanText(req.body.question).slice(0, 600);
    const answer = cleanText(req.body.answer).slice(0, 800);
    const studentAnswer = cleanText(req.body.studentAnswer).slice(0, 1500);
    if (!question || !studentAnswer) return res.status(400).json({ error: 'Question and answer are required.' });
    const system = `You grade a student's answer to a quiz question. Be fair and kind: accept answers that show the right idea, even if worded differently or slightly incomplete.\n` +
      `Reply with ONLY a JSON object and nothing else: {"correct": true or false, "feedback": "..."}\n` +
      `- feedback: one to three short sentences. If the answer is wrong, explain the right answer simply.\n\n` +
      `QUESTION: ${question}\nCORRECT ANSWER: ${answer}\nSTUDENT ANSWER: ${studentAnswer}`;
    const raw = await nebiusChat([{ role: 'system', content: system }, { role: 'user', content: 'Grade the answer now.' }],
      { kind: 'quiz-grade', answer, studentAnswer, maxTokens: 1200 });
    const g = parseJsonReply(raw);
    res.json({ ok: true, correct: truthy(g.correct), feedback: cleanText(g.feedback).slice(0, 900) || (truthy(g.correct) ? 'Correct!' : `The answer: ${answer}`) });
  } catch (error) {
    console.error('Quiz grade error:', error);
    res.status(500).json({ error: error.name === 'AbortError' ? 'The AI request timed out. Try again.' : error.message });
  }
});

app.post('/api/session/start', (req, res) => {
  try {
    const id = `session_${Date.now().toString(36)}_${crypto.randomBytes(3).toString('hex')}`;
    const session = {
      id,
      startedAt: Date.now(),
      endedAt: null,
      courseId: req.body.courseId || null,
      courseName: req.body.courseName || null,
      focusMin: Number(req.body.focusMin || 0),
      events: [],
      chat: []
    };
    writeSession(session);
    res.json({ ok: true, sessionId: id });
  } catch (error) { res.status(500).json({ error: error.message || 'Could not start session.' }); }
});

app.post('/api/session/event', (req, res) => {
  try {
    const session = readSession(req.body.sessionId);
    if (!session) return res.status(404).json({ error: 'Study session not found.' });
    session.events.push({ at: Date.now(), type: String(req.body.type || 'event'), data: req.body.data || null });
    if (Array.isArray(req.body.chat)) session.chat = req.body.chat.slice(-12);
    writeSession(session);
    res.json({ ok: true });
  } catch (error) { res.status(500).json({ error: error.message || 'Could not save session event.' }); }
});

app.post('/api/session/end', async (req, res) => {
  try {
    const session = readSession(req.body.sessionId);
    if (!session) return res.status(404).json({ error: 'Study session not found.' });
    session.endedAt = Date.now();
    session.events.push({ at: session.endedAt, type: 'session_end', data: req.body.data || null });
    if (Array.isArray(req.body.chat)) session.chat = req.body.chat.slice(-12);
    writeSession(session);

    const durationMin = Math.max(0, Math.round((session.endedAt - session.startedAt) / 60000));
    const course = session.courseId ? readCourse(session.courseId) : null;
    const retrieved = course && session.chat?.length
      ? retrieve(course, session.chat.filter(x => x.role === 'user').map(x => x.content).join(' '), 8)
      : [];
    const quiz = req.body.data?.quiz || null;
    const quizLine = quiz && quiz.asked ? `${quiz.correct}/${quiz.asked} quiz answers correct (${Math.round(quiz.correct / quiz.asked * 100)}%)` : 'No quiz this session';
    const weakList = Array.isArray(quiz?.weak) ? quiz.weak.slice(0, 5) : [];
    const system = `You are Omaima's study coach. Create a concise end-of-session report from the supplied session data.\n` +
      `Return plain text with exactly these headings: SESSION, WHAT YOU LEARNED, WEAK AREA, NEXT STEP.\n` +
      `Do not invent facts. If the quiz results name weak topics, use them for WEAK AREA. If nothing reveals a weak area, say so.\n` +
      `In NEXT STEP, recommend one specific next session (minutes of review + number of quiz questions).\n` +
      `QUIZ: ${quizLine}. Weak topics: ${weakList.join(', ') || 'none found'}. Per topic: ${JSON.stringify(quiz?.topics || {})}\n` +
      `SESSION duration: ${durationMin} minutes. Focus target: ${session.focusMin || 'unknown'} minutes.\n` +
      `SESSION EVENTS:\n${JSON.stringify(session.events.slice(-30))}\n` +
      `STUDENT/AI CHAT:\n${JSON.stringify(session.chat || [])}\n` +
      `COURSE PASSAGES:\n${retrieved.map(x => x.text).join('\n\n') || 'None available.'}`;
    const practised = Object.keys(quiz?.topics || {});
    const shaky = practised.filter(k => quiz.topics[k]?.wrong > 0 && !weakList.includes(k));
    let report = `SESSION\n${durationMin} minutes focused · ${quizLine}\n\n` +
      `WHAT YOU LEARNED\n${practised.length ? 'Topics practised: ' + practised.join(', ') : 'Session completed.'}\n\n` +
      `WEAK AREA\n${weakList.length ? weakList.join(', ') : shaky.length ? `Nothing serious. ${shaky.join(', ')} needed a second try.` : 'No weak spot found yet.'}\n\n` +
      `NEXT STEP\n${weakList.length ? `15 minutes reviewing ${weakList[0]}, then a 5-question quiz.` : shaky.length ? `10 minutes reviewing ${shaky[0]}, then a 5-question quiz.` : 'Take a 5-question quiz next session to check what stuck.'}`;
    if (NEBIUS_API_KEY) report = await nebiusChat([{ role: 'system', content: system }, { role: 'user', content: 'Write the end-of-session report now.' }], { timeoutMs: 60000 });
    session.report = report;
    writeSession(session);
    res.json({ ok: true, report, durationMin });
  } catch (error) {
    console.error('Session end error:', error);
    res.status(500).json({ error: error.message || 'Could not create study report.' });
  }
});

app.use(express.static(WEB_DIR));
app.use((req, res) => {
  if (req.method === 'GET' && !req.path.startsWith('/api/')) return res.sendFile(path.join(WEB_DIR, 'index.html'));
  res.status(404).json({ error: 'Not found' });
});

// On your computer HOST is 127.0.0.1, so nobody on your Wi-Fi can use your API key.
// Online, HOST=0.0.0.0 and APP_PASSWORD protects it instead.
app.listen(PORT, HOST, () => {
  console.log(`Lumi study server running at http://localhost:${PORT}`);
  console.log(`Listening on: ${HOST}`);
  console.log(`Password protection: ${APP_PASSWORD ? 'on' : 'off'}`);
  console.log(`Nebius model: ${NEBIUS_MODEL}`);
  console.log(`API key configured: ${Boolean(NEBIUS_API_KEY)}`);
  if (PRACTICE) console.log('PRACTICE MODE: answers are pretend. Add NEBIUS_API_KEY to .env for the real AI.');
  if (HOST !== '127.0.0.1' && !APP_PASSWORD) console.log('WARNING: server is reachable from outside but APP_PASSWORD is not set.');
});
