// Shark arena.
//   - A human is playing  -> interactive game (drag to move, the bot replies through the API).
//   - Bot vs bot          -> live stream (SSE) with a scrubbable replay.

// The order must match OPENING_BOOK in tools/arena/match.py.
const OPENINGS = [
    'Start position', 'Open Game (1.e4 e5)', 'Sicilian (1.e4 c5)',
    'French (1.e4 e6)', 'Caro-Kann (1.e4 c6)', "Queen's Gambit",
    "King's Indian setup", 'Reti', 'English (1.c4 e5)', 'Ruy Lopez',
];

let board = null;
let chessLocal = new Chess();       // rules engine for interactive mode
let players = { w: 'human', b: 'human' };
let mode = 'interactive';
let orientation = 'white';          // side shown at the bottom of the board

// Shared view state
let moves = [];   // [{san, uci, ms}]
let fens = [];    // position after each ply
let ply = 0;      // view cursor

// Interactive state
let thinking = false;
let started = false;

// Stream state
let evtSource = null;
let liveFollow = true;
let sessionId = null;               // current bot-vs-bot stream (for live personality switching)
let result = null, termination = null, mWhite = '', mBlack = '';

const botDesc = { human: 'You play this side. Drag pieces to move.' };
const persDesc = {};                // personality name -> description
let sharkBots = new Set();          // names of bots that understand personalities

const $ = (id) => document.getElementById(id);
const COLOR_NAME = { w: 'White', b: 'Black' };

function pieceTheme(p) { return 'img/wikipedia/' + p + '.png'; }
function isBot(c) { return players[c] !== 'human'; }
function fmtMs(ms) {
    if (ms == null) return '';
    return ms >= 1000 ? (ms / 1000).toFixed(2) + 's' : ms + 'ms';
}

// Personality selected for a side ('Balanced' for humans and non-Shark bots).
function persFor(color) {
    const el = $(color === 'w' ? 'white-pers' : 'black-pers');
    return el && !el.disabled ? el.value : 'Balanced';
}
// Optional per-side move-time override in ms. Empty or invalid means 0 (use the bot default).
function mtFor(color) {
    const v = parseInt($(color === 'w' ? 'white-mt' : 'black-mt').value, 10);
    return Number.isFinite(v) && v > 0 ? v : 0;
}
// Player in a side: the running game's player, or the current picker value before a game starts.
function playerOf(c) {
    const live = (mode === 'interactive' && started) || (mode === 'stream' && (evtSource || fens.length));
    return live ? players[c] : $(c === 'w' ? 'white-player' : 'black-player').value;
}
function nameOf(p) { return p === 'human' ? 'You' : p; }

// ---- theme ----------------------------------------------------------------
function effectiveTheme() {
    const set = document.documentElement.dataset.theme;
    if (set) return set;
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}
function renderThemeButton() {
    const dark = effectiveTheme() === 'dark';
    $('theme-icon').setAttribute('href', dark ? '#i-sun' : '#i-moon');
    $('theme-btn').setAttribute('aria-label', dark ? 'Switch to light theme' : 'Switch to dark theme');
}
function toggleTheme() {
    const next = effectiveTheme() === 'dark' ? 'light' : 'dark';
    document.documentElement.dataset.theme = next;
    try { localStorage.setItem('shark-theme', next); } catch (e) { /* storage blocked */ }
    renderThemeButton();
}

// ---- toast ----------------------------------------------------------------
let toastTimer = null;
function toast(msg) {
    const el = $('toast');
    el.textContent = msg;
    el.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.remove('show'), 3200);
}

// ---- board ----------------------------------------------------------------
function initBoard() {
    board = Chessboard('board', {
        pieceTheme, draggable: true, position: 'start',
        onDragStart, onDrop, onSnapEnd,
    });
    // Size the board to a multiple of 8px so squares stay whole, and refit when the panel resizes.
    const panel = document.querySelector('.board-panel');
    let raf = null;
    const fit = () => {
        const size = Math.floor(Math.min(panel.clientWidth, 600) / 8) * 8;
        if (size < 160) return;
        panel.style.setProperty('--board-size', size + 'px');
        board.resize(); markLastMove();
    };
    new ResizeObserver(() => { cancelAnimationFrame(raf); raf = requestAnimationFrame(fit); }).observe(panel);
}

// Squares of the move shown at the current view position.
function markLastMove() {
    document.querySelectorAll('#board .sq-last').forEach(el => el.classList.remove('sq-last'));
    const m = ply > 0 ? moves[ply - 1] : null;
    if (!m || !m.uci) return;
    [m.uci.slice(0, 2), m.uci.slice(2, 4)].forEach(sq => {
        const el = document.querySelector('#board .square-' + sq);
        if (el) el.classList.add('sq-last');
    });
}
function flipBoard() {
    board.flip();
    orientation = orientation === 'white' ? 'black' : 'white';
    markLastMove();
    renderPlates();
}

// ---- setup panel ----------------------------------------------------------
function showSetupError(msg) {
    const el = $('setup-error');
    el.textContent = msg || '';
    el.classList.toggle('hidden', !msg);
}

async function loadBots() {
    let bots = [], stockfish = [];
    try {
        const r = await fetch('/api/bots');
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const d = await r.json();
        bots = d.bots || []; stockfish = d.stockfish || [];
    } catch (e) {
        console.error(e);
        showSetupError('Could not load the bot list. Check that the server is running.');
    }

    for (const which of ['white-player', 'black-player']) {
        const sel = $(which); sel.innerHTML = '';
        const o = document.createElement('option');
        o.value = 'human'; o.textContent = 'Human (you)'; sel.appendChild(o);
        const grp = (label, list) => {
            if (!list.length) return;
            const og = document.createElement('optgroup'); og.label = label;
            list.forEach(b => {
                const x = document.createElement('option');
                x.value = b.name; x.textContent = b.name; og.appendChild(x);
                if (b.description) botDesc[b.name] = b.description;
            });
            sel.appendChild(og);
        };
        grp('Shark bots', bots); grp('Stockfish', stockfish);
    }
    sharkBots = new Set(bots.map(b => b.name));
    $('white-player').value = 'human';
    $('black-player').value = bots[0] ? bots[0].name : 'human';

    const os = $('opening-select');
    OPENINGS.forEach((label, i) => {
        const x = document.createElement('option');
        x.value = String(i); x.textContent = label; os.appendChild(x);
    });

    // Helper text under each picker so newcomers know what they are facing.
    for (const [selId, descId] of [['white-player', 'white-desc'], ['black-player', 'black-desc']]) {
        const sel = $(selId), desc = $(descId);
        const update = () => { desc.textContent = botDesc[sel.value] || ''; };
        sel.addEventListener('change', () => { update(); updateOpeningField(); renderPlates(); });
        update();
    }
    await loadPersonalities();
    updateOpeningField();
}

async function loadPersonalities() {
    let list = [];
    try { list = (await (await fetch('/api/personalities')).json()).personalities || []; }
    catch (e) { console.error(e); }
    list.forEach(p => { persDesc[p.name] = p.description; });

    for (const side of ['white', 'black']) {
        const c = side === 'white' ? 'w' : 'b';
        const sel = $(side + '-pers'), desc = $(side + '-pers-desc'), player = $(side + '-player');
        sel.innerHTML = '';
        list.forEach(p => {
            const o = document.createElement('option');
            o.value = p.name; o.textContent = p.name; sel.appendChild(o);
        });
        // Personality only applies to Shark bots (not humans or Stockfish).
        const refresh = () => {
            const ok = sharkBots.has(player.value);
            sel.disabled = !ok;
            if (!ok) sel.value = 'Balanced';
            desc.textContent = ok ? (persDesc[sel.value] || '') : 'Personalities apply to Shark bots only.';
            renderPlates();
        };
        player.addEventListener('change', refresh);
        // A personality can be switched at any time and applies from that side's next move.
        sel.addEventListener('change', () => { refresh(); onPersonalityChanged(c); });
        refresh();
    }
}

// The opening only matters when both sides are bots.
function updateOpeningField() {
    const bothBots = $('white-player').value !== 'human' && $('black-player').value !== 'human';
    $('opening-wrap').hidden = !bothBots;
}

function swapSides() {
    if (mode === 'interactive' && started && !result) {
        toast('Finish this game, or press Start game, before swapping colors.');
        return;
    }
    const w = { p: $('white-player').value, mt: $('white-mt').value, pers: $('white-pers').value };
    const b = { p: $('black-player').value, mt: $('black-mt').value, pers: $('black-pers').value };
    $('white-player').value = b.p; $('black-player').value = w.p;
    $('white-mt').value = b.mt; $('black-mt').value = w.mt;
    // The change events refresh helper text and disable personalities for non-Shark players.
    $('white-player').dispatchEvent(new Event('change'));
    $('black-player').dispatchEvent(new Event('change'));
    if (!$('white-pers').disabled) $('white-pers').value = b.pers;
    if (!$('black-pers').disabled) $('black-pers').value = w.pers;
    for (const side of ['white', 'black']) {
        const pe = $(side + '-pers');
        if (!pe.disabled) $(side + '-pers-desc').textContent = persDesc[pe.value] || '';
    }
    renderPlates();
}

// Lock the setup while a bot-vs-bot game streams (the running game ignores later edits).
function setSetupLocked(locked) {
    ['white-player', 'black-player', 'white-mt', 'black-mt', 'opening-select', 'swap-btn', 'start-btn']
        .forEach(id => { $(id).disabled = locked; });
}

function onPersonalityChanged(color) {
    renderPlates();
    const running = (mode === 'stream' && sessionId) || (mode === 'interactive' && started && !result);
    if (!running || !isBot(color)) return;
    toast(`${persFor(color)} applies from ${COLOR_NAME[color]}'s next move.`);
    if (mode === 'stream') {
        fetch('/api/arena/personality', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ session: sessionId, side: color, personality: persFor(color) }),
        }).catch(() => {});
    }
    // Interactive mode needs nothing else: botMove() reads the dropdown on every turn.
}

async function loadElo() {
    const show = (value, note, rated) => {
        $('elo-value').textContent = value;
        $('elo-value').classList.toggle('is-unrated', !rated);
        $('elo-unit').hidden = !rated;
        $('elo-note').textContent = note;
    };
    try {
        const r = await fetch('/api/elo');
        const d = r.ok ? await r.json() : null;
        if (d && d.estimate_elo) {
            show(d.estimate_elo, `${d.bot} at ${d.movetime_ms} ms/move, rated against Stockfish`, true);
            return;
        }
    } catch (e) { console.error(e); }
    show('Unrated', 'Run tools.arena.calibrate to measure a rating', false);
}

// ---- shared rendering -----------------------------------------------------
function setStatus(msg, opts = {}) {
    const el = $('status');
    el.textContent = msg;
    el.classList.toggle('is-error', !!opts.error);
    renderPlates();
}

// Side to move in the running game, or null when no game is running.
function activeTurn() {
    if (result) return null;
    if (mode === 'interactive') return started ? chessLocal.turn() : null;
    if (mode === 'stream' && evtSource) return fens.length % 2 === 0 ? 'w' : 'b';
    return null;
}
function lastMsFor(c) {
    for (let i = moves.length - 1; i >= 0; i--) {
        if ((i % 2 === 0) === (c === 'w') && moves[i].ms != null) return moves[i].ms;
    }
    return null;
}
function renderPlates() {
    const bottom = orientation === 'white' ? 'w' : 'b';
    const top = bottom === 'w' ? 'b' : 'w';
    const turn = activeTurn();
    for (const [id, c] of [['plate-top', top], ['plate-bottom', bottom]]) {
        const el = $(id);
        const pers = playerOf(c) !== 'human' && persFor(c) !== 'Balanced' ? persFor(c) : '';
        const ms = lastMsFor(c);
        let state = '';
        if (turn === c && (thinking || mode === 'stream')) state = '<span class="plate-thinking">Thinking</span>';
        else if (ms != null) state = `<span class="plate-time">${fmtMs(ms)}</span>`;
        const name = nameOf(playerOf(c));
        el.className = 'plate' + (turn === c ? ' is-turn' : '');
        el.innerHTML =
            `<img src="img/wikipedia/${c}K.png" alt="" width="28" height="28">` +
            `<span class="plate-name"></span>` +
            (pers ? `<span class="plate-pers">${pers}</span>` : '') +
            `<span class="plate-state">${state}</span>`;
        el.querySelector('.plate-name').textContent = name;
        el.setAttribute('aria-label', `${COLOR_NAME[c]}: ${name}`);
    }
}

function renderResult() {
    const el = $('result');
    if (!result) { el.classList.add('hidden'); return; }
    el.classList.remove('hidden');
    el.classList.toggle('is-draw', result === '1/2-1/2');
    if (result === '1/2-1/2') {
        $('result-text').textContent = `Draw by ${termination}`;
    } else {
        const w = result === '1-0';
        const who = nameOf(w ? mWhite : mBlack);
        $('result-text').textContent = `${who} (${w ? 'White' : 'Black'}) ${who === 'You' ? 'win' : 'wins'} by ${termination}`;
    }
}

function renderMoves(clickable) {
    const list = $('moves-list'); list.innerHTML = '';
    if (!moves.length) {
        const p = document.createElement('p');
        p.className = 'empty';
        p.textContent = 'Moves appear here once the game starts.';
        list.appendChild(p);
        return;
    }
    for (let i = 0; i < moves.length; i += 2) {
        const row = document.createElement('div'); row.className = 'move-row';
        const num = document.createElement('span');
        num.className = 'mn'; num.textContent = Math.floor(i / 2) + 1;
        row.appendChild(num);
        row.appendChild(cell(i, clickable));
        if (moves[i + 1]) row.appendChild(cell(i + 1, clickable));
        list.appendChild(row);
    }
    list.scrollTop = list.scrollHeight;
}
function cell(i, clickable) {
    const s = document.createElement(clickable ? 'button' : 'span');
    s.className = 'm';
    if (clickable) {
        s.type = 'button';
        s.onclick = () => { liveFollow = false; stopAuto(); goToPly(i + 1); };
    }
    s.dataset.ply = i + 1;
    const san = document.createElement('span'); san.textContent = moves[i].san;
    s.appendChild(san);
    const mt = fmtMs(moves[i].ms);
    if (mt) {
        const t = document.createElement('span'); t.className = 'mt'; t.textContent = mt;
        s.appendChild(t);
    }
    return s;
}
function goToPly(k) {
    ply = Math.max(0, Math.min(k, fens.length));
    board.position(ply === 0 ? 'start' : fens[ply - 1]);
    $('ply-indicator').textContent = mode === 'stream' && fens.length ? `${ply} / ${fens.length}` : '';
    document.querySelectorAll('#moves-list .m').forEach(el =>
        el.classList.toggle('current', parseInt(el.dataset.ply, 10) === ply));
    const cur = document.querySelector('#moves-list .m.current');
    if (cur) cur.scrollIntoView({ block: 'nearest' });
    markLastMove();
}
function showControls(streamMode, human) {
    ['first-btn', 'prev-btn', 'play-btn', 'next-btn', 'last-btn'].forEach(
        id => $(id).classList.toggle('hidden', !streamMode));
    $('undo-btn').classList.toggle('hidden', !human);
}

// ---- start dispatch -------------------------------------------------------
function resetView() {
    moves = []; fens = []; ply = 0; result = null; termination = null;
    renderMoves(false); board.start(); renderResult();
    $('ply-indicator').textContent = '';
    markLastMove();
}

function startGame() {
    showSetupError('');
    if (evtSource) { evtSource.close(); evtSource = null; }
    stopAuto();
    players.w = $('white-player').value;
    players.b = $('black-player').value;
    mWhite = players.w; mBlack = players.b;
    resetView();

    const bothBots = isBot('w') && isBot('b');
    mode = bothBots ? 'stream' : 'interactive';
    if (bothBots) startStream();
    else startInteractive();
}

// ---- interactive (human involved) -----------------------------------------
function startInteractive() {
    showControls(false, true);
    chessLocal.reset(); board.start();
    started = true; thinking = false;
    orientation = isBot('w') && !isBot('b') ? 'black' : 'white';
    board.orientation(orientation);
    markLastMove();
    setStatus('Game on. ' + turnLabel() + ' to move.');
    scheduleInteractiveTurn();
}
function turnLabel() {
    const t = chessLocal.turn();
    const who = isBot(t) ? players[t] : 'You';
    return `${who} (${COLOR_NAME[t]})`;
}
function onDragStart(src, piece) {
    if (mode !== 'interactive' || !started || thinking || chessLocal.game_over())
        return false;
    const t = chessLocal.turn();
    if (isBot(t)) return false;
    if ((t === 'w' && piece.search(/^b/) !== -1) ||
        (t === 'b' && piece.search(/^w/) !== -1)) return false;
}
function onDrop(src, tgt) {
    const mv = chessLocal.move({ from: src, to: tgt, promotion: 'q' });
    if (mv === null) return 'snapback';
    pushLocal(mv, null);
    setTimeout(scheduleInteractiveTurn, 80);
}
function onSnapEnd() { if (mode === 'interactive') board.position(chessLocal.fen()); }
function pushLocal(mv, ms) {
    moves.push({ san: mv.san, uci: mv.from + mv.to + (mv.promotion || ''), ms });
    fens.push(chessLocal.fen());
    renderMoves(false); goToPly(fens.length);
}
function scheduleInteractiveTurn() {
    if (!started || chessLocal.game_over()) { finishInteractive(); return; }
    if (isBot(chessLocal.turn())) setTimeout(botMove, 200);
    else setStatus(turnLabel() + ' to move.');
}
async function botMove() {
    if (!started || chessLocal.game_over()) return;
    const t = chessLocal.turn();
    thinking = true;
    setStatus(`${players[t]} (${COLOR_NAME[t]}) is thinking.`);
    try {
        const r = await fetch('/api/bot-move', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ fen: chessLocal.fen(), bot: players[t],
                                   movetime: mtFor(t), personality: persFor(t) }),
        });
        const d = await r.json();
        thinking = false;
        if (d.error) {
            setStatus('Bot error: ' + String(d.error).split('\n').slice(-3).join(' | '), { error: true });
            return;
        }
        if (!d.move) { finishInteractive(); return; }
        const mv = chessLocal.move({
            from: d.move.slice(0, 2), to: d.move.slice(2, 4),
            promotion: d.move.slice(4, 5) || 'q',
        });
        if (mv) { pushLocal(mv, d.ms); setTimeout(scheduleInteractiveTurn, 150); }
    } catch (e) {
        thinking = false;
        setStatus('Could not reach the server: ' + e.message, { error: true });
    }
}
function finishInteractive() {
    if (chessLocal.in_checkmate()) {
        result = chessLocal.turn() === 'w' ? '0-1' : '1-0'; termination = 'checkmate';
    } else if (chessLocal.in_stalemate()) { result = '1/2-1/2'; termination = 'stalemate'; }
    else if (chessLocal.insufficient_material()) { result = '1/2-1/2'; termination = 'insufficient material'; }
    else if (chessLocal.in_threefold_repetition()) { result = '1/2-1/2'; termination = 'repetition'; }
    else if (chessLocal.in_draw()) { result = '1/2-1/2'; termination = 'the fifty-move rule'; }
    if (result) { renderResult(); setStatus('Game over.'); }
    else setStatus(turnLabel() + ' to move.');
}
function undoMove() {
    if (mode !== 'interactive' || moves.length === 0) return;
    chessLocal.undo(); moves.pop(); fens.pop();
    if (isBot(chessLocal.turn()) && moves.length > 0) { chessLocal.undo(); moves.pop(); fens.pop(); }
    result = null; termination = null; renderResult(); renderMoves(false);
    goToPly(fens.length); scheduleInteractiveTurn();
}

// ---- stream (bot vs bot) --------------------------------------------------
function startStream() {
    showControls(true, false);
    started = false;
    liveFollow = true;
    orientation = 'white'; board.orientation('white');
    const opening = parseInt($('opening-select').value, 10) || 0;
    setSetupLocked(true);

    sessionId = Math.random().toString(36).slice(2) + Date.now().toString(36);
    const url = `/api/arena/stream?sid=${sessionId}` +
                `&wp=${encodeURIComponent(persFor('w'))}&bp=${encodeURIComponent(persFor('b'))}` +
                `&white=${encodeURIComponent(mWhite)}` +
                `&black=${encodeURIComponent(mBlack)}&opening=${opening}` +
                `&wmt=${mtFor('w')}&bmt=${mtFor('b')}`;
    evtSource = new EventSource(url);
    setStatus(`Live: ${mWhite} vs ${mBlack}. Moves appear as they are played.`);
    evtSource.onmessage = (e) => {
        let m; try { m = JSON.parse(e.data); } catch { return; }
        if (m.type === 'move') {
            moves.push({ san: m.san, uci: m.uci, ms: m.ms }); fens.push(m.fen);
            renderMoves(true);
            if (liveFollow) goToPly(fens.length);
            else $('ply-indicator').textContent = `${ply} / ${fens.length}`;
            renderPlates();
        } else if (m.type === 'end') {
            result = m.result; termination = m.termination;
            renderResult();
            setStatus(`Game over after ${m.plies} plies. Use the controls or arrow keys to replay.`);
        }
    };
    evtSource.addEventListener('done', endStream);
    evtSource.onerror = () => {
        if (!result) setStatus('The connection to the game was lost.', { error: true });
        endStream();
    };
}
function endStream() {
    sessionId = null;
    if (evtSource) { evtSource.close(); evtSource = null; }
    setSetupLocked(false);
    renderPlates();
}

// ---- autoplay (replay) ----------------------------------------------------
let autoTimer = null;
function setPlayIcon(playing) {
    $('play-icon').setAttribute('href', playing ? '#i-pause' : '#i-play');
    $('play-btn').setAttribute('aria-label', playing ? 'Pause replay' : 'Replay automatically');
}
function stopAuto() {
    if (autoTimer) { clearInterval(autoTimer); autoTimer = null; }
    setPlayIcon(false);
}
function toggleAuto() {
    if (autoTimer) { stopAuto(); return; }
    liveFollow = false;
    if (ply >= fens.length) goToPly(0);
    setPlayIcon(true);
    autoTimer = setInterval(() => {
        if (ply >= fens.length) { stopAuto(); return; }
        goToPly(ply + 1);
    }, 600);
}

// ---- PGN ------------------------------------------------------------------
function buildPgn() {
    const g = new Chess();
    for (const m of moves) { if (!g.move(m.san)) break; }
    g.header('Event', 'Shark arena', 'White', mWhite === 'human' ? 'Human' : mWhite,
             'Black', mBlack === 'human' ? 'Human' : mBlack, 'Result', result || '*');
    return g.pgn();
}
async function copyPgn() {
    const text = buildPgn();
    try {
        await navigator.clipboard.writeText(text);
        toast('PGN copied to the clipboard.');
    } catch (e) {
        const ta = document.createElement('textarea');
        ta.value = text; document.body.appendChild(ta); ta.select();
        let ok = false;
        try { ok = document.execCommand('copy'); } catch (err) { /* ignored */ }
        ta.remove();
        toast(ok ? 'PGN copied to the clipboard.' : 'Could not copy the PGN.');
    }
}

// ---- keyboard -------------------------------------------------------------
function replayStep(fn) { liveFollow = false; stopAuto(); fn(); }
function onKey(e) {
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    const tag = e.target.tagName;
    if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
    if (e.key === 'f' || e.key === 'F') { flipBoard(); return; }
    if (mode !== 'stream' || !fens.length) return;
    switch (e.key) {
        case 'ArrowLeft': replayStep(() => goToPly(ply - 1)); break;
        case 'ArrowRight': replayStep(() => goToPly(ply + 1)); break;
        case 'Home': replayStep(() => goToPly(0)); break;
        case 'End': replayStep(() => goToPly(fens.length)); break;
        case ' ':
            if (tag === 'BUTTON') return;      // let a focused button handle its own Space
            toggleAuto(); break;
        default: return;
    }
    e.preventDefault();
}

window.addEventListener('DOMContentLoaded', () => {
    initBoard(); renderMoves(false); renderPlates(); renderThemeButton();
    loadBots(); loadElo();
    $('theme-btn').onclick = toggleTheme;
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', renderThemeButton);
    $('start-btn').onclick = startGame;
    $('rematch-btn').onclick = startGame;
    $('pgn-btn').onclick = copyPgn;
    $('swap-btn').onclick = swapSides;
    $('flip-btn').onclick = flipBoard;
    $('undo-btn').onclick = undoMove;
    $('first-btn').onclick = () => replayStep(() => goToPly(0));
    $('prev-btn').onclick = () => replayStep(() => goToPly(ply - 1));
    $('next-btn').onclick = () => replayStep(() => goToPly(ply + 1));
    $('last-btn').onclick = () => replayStep(() => goToPly(fens.length));
    $('play-btn').onclick = toggleAuto;
    document.addEventListener('keydown', onKey);
});
