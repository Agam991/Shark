const express = require('express');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const app = express();
app.use(express.json());
app.use(express.static(__dirname));

const REPO_ROOT = path.join(__dirname, '..');
const BOTS_DIR = path.join(REPO_ROOT, 'bots');
// Prefer the project venv python (has python-chess); fall back to system.
const VENV_PY = process.platform === 'win32'
    ? path.join(REPO_ROOT, '.venv/Scripts/python.exe')
    : path.join(REPO_ROOT, '.venv/bin/python');
const PYTHON = fs.existsSync(VENV_PY) ? VENV_PY
    : (process.platform === 'win32' ? 'python' : 'python3');

// All engine work (human-vs-bot moves and bot-vs-bot games) now runs through the
// Python arena helpers, which drive the UCI engines. No persistent engine here.

// ---- Personalities --------------------------------------------------------
const PERSONALITIES = [
    { name: 'Balanced', description: 'Default Shark. No style bias, no opening repertoire.' },
    { name: 'Aggressive', description: 'Attacks the king, values activity and space. Plays gambits and sharp lines: King\'s Gambit, Evans, Najdorf, King\'s Indian.' },
    { name: 'Defensive', description: 'Safe king, solid pawns, trades into calm positions. Plays London, QGD, Caro-Kann, French.' },
    { name: 'Positional', description: 'Pawn structure, outposts, open files, bishop pair. Slow maneuvering: English, Catalan, Reti, Ruy Lopez, Nimzo-Indian.' },
];
const PERSONALITY_NAMES = PERSONALITIES.map((p) => p.name);
// Unknown / missing -> Balanced (never pass raw user text to the engine tools).
const cleanPersonality = (v) => (PERSONALITY_NAMES.includes(v) ? v : 'Balanced');

app.get('/api/personalities', (req, res) => res.json({ personalities: PERSONALITIES }));

// Live stream sessions, so the UI can switch a side's personality mid-game.
const sessions = new Map();

app.post('/api/arena/personality', (req, res) => {
    const { session, side, personality } = req.body || {};
    const proc = sessions.get(session);
    if (!proc || !proc.stdin.writable || !['w', 'b'].includes(side)) {
        return res.status(404).json({ error: 'no such running game' });
    }
    proc.stdin.write(JSON.stringify({ side, personality: cleanPersonality(personality) }) + '\n');
    res.json({ ok: true });
});

// ---- Arena: list fighters -------------------------------------------------
app.get('/api/bots', (req, res) => {
    let bots = [];
    try {
        bots = fs.readdirSync(BOTS_DIR)
            .filter((f) => f.endsWith('.json'))
            .map((f) => JSON.parse(fs.readFileSync(path.join(BOTS_DIR, f))))
            .map((b) => ({ name: b.name, description: b.description || '' }));
    } catch (e) {
        console.error('[BOTS]', e.message);
    }
    // Stockfish pseudo-bots at a few skill levels (novice-friendly blurbs).
    const sfBlurbs = {
        0: 'Very weak (Skill 0). Blunders a lot - good first opponent.',
        3: 'Weak (Skill 3). Beginner friendly.',
        5: 'Casual club player (Skill 5).',
        8: 'Intermediate (Skill 8). A real test.',
        12: 'Strong (Skill 12). Tough to beat.',
        20: 'Full strength (Skill 20). World-class - nearly unbeatable.',
    };
    const stockfish = [0, 3, 5, 8, 12, 20].map((s) => ({
        name: `stockfish-skill${s}`,
        description: `Stockfish - ${sfBlurbs[s]}`,
    }));
    res.json({ bots, stockfish });
});

// ---- Elo: latest calibration result --------------------------------------
app.get('/api/elo', (req, res) => {
    const candidates = [
        path.join(REPO_ROOT, 'web/data/elo.json'),
        path.join(REPO_ROOT, 'docs/elo.json'),
    ];
    for (const f of candidates) {
        try {
            if (fs.existsSync(f)) return res.json(JSON.parse(fs.readFileSync(f)));
        } catch (e) { /* try next */ }
    }
    res.json({ estimate_elo: null });
});

// ---- Arena: LIVE stream a game (Server-Sent Events) -----------------------
app.get('/api/arena/stream', (req, res) => {
    const { white, black } = req.query;
    const opening = parseInt(req.query.opening, 10) || 0;
    if (!white || !black) {
        return res.status(400).json({ error: 'white and black are required' });
    }

    res.writeHead(200, {
        'Content-Type': 'text/event-stream',
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive',
    });
    res.write('retry: 10000\n\n');

    const args = ['-m', 'tools.arena.play_one', '--stream',
        '--white', String(white), '--black', String(black),
        '--opening', String(opening),
        '--white-personality', cleanPersonality(req.query.wp),
        '--black-personality', cleanPersonality(req.query.bp)];
    const wmt = parseInt(req.query.wmt, 10);
    const bmt = parseInt(req.query.bmt, 10);
    if (Number.isFinite(wmt) && wmt > 0) args.push('--white-movetime', String(wmt));
    if (Number.isFinite(bmt) && bmt > 0) args.push('--black-movetime', String(bmt));
    const proc = spawn(PYTHON, args, { cwd: REPO_ROOT });
    const sid = String(req.query.sid || '');
    if (sid) sessions.set(sid, proc);
    proc.stdin.on('error', () => {});

    let buf = '';
    proc.stdout.on('data', (d) => {
        buf += d.toString();
        let idx;
        while ((idx = buf.indexOf('\n')) >= 0) {
            const line = buf.slice(0, idx).trim();
            buf = buf.slice(idx + 1);
            if (line) res.write(`data: ${line}\n\n`);
        }
    });
    proc.stderr.on('data', (d) => console.error('[STREAM]', d.toString().trim()));
    proc.on('close', () => {
        if (sid) sessions.delete(sid);
        res.write('event: done\ndata: {}\n\n');
        res.end();
    });

    // Kill the game if the browser disconnects (no zombie engines).
    req.on('close', () => { if (!proc.killed) proc.kill(); });
});

// ---- Play: get a chosen bot's move for a position -------------------------
app.post('/api/bot-move', (req, res) => {
    const { fen, bot } = req.body || {};
    if (!fen || !bot) {
        return res.status(400).json({ error: 'fen and bot are required' });
    }
    const args = ['-m', 'tools.arena.bot_move', '--bot', String(bot),
        '--fen', String(fen)];
    const mt = parseInt((req.body || {}).movetime, 10);
    if (Number.isFinite(mt) && mt > 0) args.push('--movetime', String(mt));
    args.push('--personality', cleanPersonality((req.body || {}).personality));
    const proc = spawn(PYTHON, args, { cwd: REPO_ROOT });
    let out = '', err = '';
    proc.stdout.on('data', (d) => (out += d.toString()));
    proc.stderr.on('data', (d) => (err += d.toString()));
    proc.on('close', (code) => {
        if (code !== 0) {
            console.error('[BOT-MOVE]', err.trim());
            return res.status(500).json({ error: err.trim() || 'bot-move failed' });
        }
        try { res.json(JSON.parse(out)); }
        catch (e) { res.status(500).json({ error: 'bad bot output' }); }
    });
});

process.on('SIGINT', () => {
    process.exit();
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, '0.0.0.0', () => {
    console.log(`Server running on port ${PORT}`);
});
