# Results

This page records every game played between Shark's four personalities, how the
games were run, and what the numbers do and do not show.

**Short version:** 420 games, no personality was measurably stronger or weaker
than the others. Every difference is inside its margin of error. What the games
do show clearly is that Shark draws by repetition far too often, because its
search cannot see repetitions.

- [Method](#method)
- [Experiment 1: evaluation terms only (300 games)](#experiment-1-evaluation-terms-only-300-games)
- [Experiment 2: with opening repertoires (120 games)](#experiment-2-with-opening-repertoires-120-games)
- [Both experiments together](#both-experiments-together)
- [What the games show](#what-the-games-show)
- [Strength against Stockfish](#strength-against-stockfish)
- [Limits of these results](#limits-of-these-results)
- [Reproduce and extend](#reproduce-and-extend)

---

## Method

| Setting | Value |
|---|---|
| Engine | The committed Shark source, built with CMake in Release mode. Same binary for every player. |
| What differs | Only the UCI option `Personality`. |
| Search limit | 30 ms per move, 1 thread, 16 MB hash, for both sides |
| Machine | Single-core Linux x86-64 container, one game at a time |
| Date | 18 October 2026 |
| Referee | python-chess: legality, mate and stalemate, draw claims (threefold repetition, fifty moves), insufficient material |
| Game limit | 400 plies (the longest game was 179 plies) |
| Openings | The 10 lines in `OPENING_BOOK` (`tools/arena/match.py`), each played with both colors |
| Pairings | Round robin: 6 pairings of the 4 personalities |

Every pairing alternates colors, so each personality plays White and Black equally.
Openings advance two games at a time, so a line is played once with each color
before the next line starts. With 50 games per pairing the first five lines are
played three times with each color and the last five twice.

**Reading the tables**

- *Score* is points per game: win 1, draw 0.5, loss 0.
- *Elo difference* converts a score into Elo with the logistic formula, with a
  95% confidence margin computed from the win, draw and loss counts
  (`tools/arena/rating.py`). A difference is only meaningful if it is bigger than
  its margin.
- *Elo vs field* converts one personality's score against all three others.

---

## Experiment 1: evaluation terms only (300 games)

The personality opening repertoire was switched off (`PersonalityBook false`), so
the only difference between players is the extra evaluation terms. 50 games per
pairing.

**Standings**

| Rank | Personality | Games | Points | Score | Elo vs field | Score as White |
|---:|---|---:|---:|---:|---:|---:|
| 1 | Positional | 150 | 81 | 54.0% | +28 | 57.3% |
| 2 | Aggressive | 150 | 74.5 | 49.7% | -2 | 53.3% |
| 3 | Defensive | 150 | 73 | 48.7% | -9 | 50.7% |
| 4 | Balanced | 150 | 71.5 | 47.7% | -16 | 50.0% |

**Head to head** (W-D-L from the first personality's side)

| Pairing | Games | W-D-L | Score | Elo difference (95%) |
|---|---:|---:|---:|---:|
| Balanced vs Aggressive | 50 | 8-30-12 | 46.0% | -28 ± 62 |
| Balanced vs Defensive | 50 | 9-34-7 | 52.0% | +14 ± 55 |
| Balanced vs Positional | 50 | 8-29-13 | 45.0% | -35 ± 63 |
| Aggressive vs Defensive | 50 | 8-33-9 | 49.0% | -7 ± 57 |
| Aggressive vs Positional | 50 | 8-30-12 | 46.0% | -28 ± 62 |
| Defensive vs Positional | 50 | 6-35-9 | 47.0% | -21 ± 53 |

**How the games ended**

| Ending | Games | Share |
|---|---:|---:|
| threefold repetition | 188 | 62.7% |
| checkmate | 109 | 36.3% |
| insufficient material | 3 | 1.0% |

Decisive games: 109 of 300 (36.3%). White wins 63, Black wins 46, draws 191. Average length 97 plies.

**Analysis**

- **Positional finished first, but not convincingly.** Its 54.0% score is +28 Elo
  over the field, with a 95% interval of -6 to +62 for 150 games. That interval
  includes zero, so this result alone does not show Positional is stronger.
- **No head-to-head is significant.** The largest gap is Balanced against
  Positional at -35, with a margin of ±63. All six pairings sit within one margin
  of zero.
- **Balanced finished last.** Placing the unmodified engine last is the opposite
  of what you would expect if the extra terms were hurting play, and it is again
  within the margin of error.
- **Almost two thirds of the games were draws by repetition.** See
  [What the games show](#what-the-games-show). When most games are draws, there is
  little left to separate the players.

---

## Experiment 2: with opening repertoires (120 games)

The personality opening books were switched on (`PersonalityBook true`). 20 games
per pairing, so each of the 10 opening lines is played once with each color.

**Standings**

| Rank | Personality | Games | Points | Score | Elo vs field | Score as White |
|---:|---|---:|---:|---:|---:|---:|
| 1 | Aggressive | 60 | 32.5 | 54.2% | +29 | 56.7% |
| 2 | Balanced | 60 | 31 | 51.7% | +12 | 53.3% |
| 3 | Positional | 60 | 30 | 50.0% | -0 | 51.7% |
| 4 | Defensive | 60 | 26.5 | 44.2% | -41 | 46.7% |

**Head to head** (W-D-L from the first personality's side)

| Pairing | Games | W-D-L | Score | Elo difference (95%) |
|---|---:|---:|---:|---:|
| Balanced vs Aggressive | 20 | 2-18-0 | 55.0% | +35 ± 46 |
| Balanced vs Defensive | 20 | 3-15-2 | 52.5% | +17 ± 77 |
| Balanced vs Positional | 20 | 5-9-6 | 47.5% | -17 ± 117 |
| Aggressive vs Defensive | 20 | 6-12-2 | 60.0% | +70 ± 98 |
| Aggressive vs Positional | 20 | 6-11-3 | 57.5% | +52 ± 105 |
| Defensive vs Positional | 20 | 1-16-3 | 45.0% | -35 ± 68 |

**How the games ended**

| Ending | Games | Share |
|---|---:|---:|
| threefold repetition | 81 | 67.5% |
| checkmate | 39 | 32.5% |

Decisive games: 39 of 120 (32.5%). White wins 22, Black wins 17, draws 81. Average length 98 plies.

**Analysis**

- **The ranking flipped.** Aggressive is first (+29) and Defensive last (-41),
  while Positional, which won Experiment 1, is in the middle at 0, and Balanced,
  which was last, is now second. Rankings that reverse between two runs of the
  same engines are what noise looks like.
- **The sample is too small to separate players.** With 20 games per pairing the
  margins are between ±46 and ±117 Elo. The biggest gap, Aggressive over
  Defensive at +70, has a margin of ±98.
- **The opening books had very little chance to matter.** The harness plays one of
  its 10 fixed opening lines from the first move in most games, so a personality
  book is only consulted in the 12 games (of 120) that start from the initial
  position. This experiment therefore mostly measures the same thing as
  Experiment 1, with fewer games.

---

## Both experiments together

Pooling the 420 games gives each personality 210 games.

| Personality | Games | Score | 95% interval | Elo vs field | 95% interval (Elo) |
|---|---:|---:|---|---:|---|
| Positional | 210 | 52.9% | 48.7 to 57.0 | +20 | -9 to +49 |
| Aggressive | 210 | 51.0% | 46.9 to 55.0 | +7 | -22 to +35 |
| Balanced | 210 | 48.8% | 44.8 to 52.9 | -8 | -37 to +20 |
| Defensive | 210 | 47.4% | 43.6 to 51.1 | -18 | -45 to +8 |

All four intervals include 50% (zero Elo). The order, Positional then Aggressive
then Balanced then Defensive, is a hint, and a single run does not confirm it.

**What this means for the weights.** The extra terms are small on purpose (see
`core/src/personality.cpp`), they are hand-chosen and untuned, and at 30 ms the
search only reaches a few plies. With those conditions, a style change moves the
score by less than this test can detect. This is a statement about how large an
effect we can see, not proof that the personalities are equal.

**How many games would it take?** About 4 in 10 games here are decisive. To
resolve a true difference of ±15 Elo between two personalities at 95% confidence
you need roughly 740 games per pairing at this draw rate, or about 4,400 games
for the full round robin. At the speed above (3 seconds per game) that is about
3.7 hours on this machine.

---

## What the games show

### Draws by repetition dominate

| | Experiment 1 | Experiment 2 |
|---|---:|---:|
| Games ending in threefold repetition | 188 of 300 (62.7%) | 81 of 120 (67.5%) |
| Games ending in checkmate | 109 of 300 (36.3%) | 39 of 120 (32.5%) |
| Other endings | 3 (insufficient material) | 0 |

Across all 420 games, 272 were drawn (64.8%) and 148 were decisive (35.2%).

This is the most useful finding. Shark's search has no repetition detection, so it
does not know that going back to an earlier position scores a draw. It has no way
to avoid repeating when it is ahead, or to aim for repetition when it is behind.
In Experiment 1 the repetition draws ended after 88 plies on average, against 113
for checkmates, and the shortest was only 22 plies. A game that is repeated that early
is an engine that shuffles instead of making progress.

Adding repetition handling to the search (and a small "contempt" value so the
engine avoids draws when it is better) should turn some of these draws into wins
and losses, and it should also make the personalities easier to measure.

### White has a small, real advantage

Over all 420 games White won 85, Black won 63 and 272 were drawn. That is a
52.6% score for White, or +18 Elo with a margin of ±20. This matches the usual
size of the first-move advantage, and it is a good check that the harness is not
biased: colors are balanced exactly, so this advantage cancels out of every
pairing.

### Some openings produce more decisive games than others

Experiment 1, share of games that were not drawn:

| Opening | Games | Decisive |
|---|---:|---:|
| Start position | 36 | 44% |
| Open Game (1.e4 e5) | 36 | 58% |
| Sicilian (1.e4 c5) | 36 | 25% |
| French (1.e4 e6) | 36 | 19% |
| Caro-Kann (1.e4 c6) | 36 | 44% |
| Queen's Gambit | 24 | 46% |
| King's Indian setup | 24 | 38% |
| Reti | 24 | 33% |
| English (1.c4 e5) | 24 | 25% |
| Ruy Lopez | 24 | 25% |

Open, symmetrical positions (the Open Game) lead to the most decisive games, and
the closed pawn structures of the French and Sicilian lead to the fewest. These
samples are small (24 to 36 games each), so treat the pattern as a hint.

### Game speed

The 420 games took about 22 minutes in total, 3.1 seconds per game on average,
which is why a few hundred games are cheap to run and a few thousand are
practical.

---

## Strength against Stockfish

This is a separate measurement, recorded on 18 July 2026 with
`tools.arena.calibrate` and stored in [`docs/elo.json`](elo.json). It rates
`shark-blitz` (Balanced) at 100 ms per move, with both sides on the same move
time, against Stockfish limited to a given `UCI_Elo`.

| Stockfish `UCI_Elo` | Games | Wins | Draws | Losses | Score | Performance |
|---:|---:|---:|---:|---:|---:|---:|
| 2200 | 20 | 12 | 5 | 3 | 72.5% | 2368 |
| 2500 | 20 | 3 | 12 | 5 | 45.0% | 2465 |
| 2800 | 20 | 2 | 7 | 11 | 27.5% | 2632 |
| 3050 | 20 | 0 | 4 | 16 | 10.0% | 2668 |
| 3190 | 20 | 1 | 3 | 16 | 12.5% | 2852 |

**Headline: about 2445 Elo.** This is where Shark scores 50% (the crossover). The
performance rating averaged over the near-even levels is about 2488.

Treat it as a blitz-strength estimate. The time control is fast, Stockfish's
`UCI_Elo` scale is an approximation, and 20 games per level is a small sample, so
the figure carries an uncertainty of well over 50 Elo. The personality tournaments
above use 30 ms per move, so their strength is lower than this number. They are
comparisons between personalities, not ratings.

---

## Limits of these results

- **Low time control.** 30 ms per move is only a few plies of search. Style terms
  may matter more at longer time controls, where the search is deeper and the
  evaluation is trusted more. Re-running at 200 ms or more would be a fair test.
- **Small samples.** 50 games per pairing in Experiment 1 and 20 in Experiment 2
  can only reveal differences of roughly 50 Elo or more.
- **Draw-heavy games.** Two thirds of the games are repetition draws, which
  compresses every difference toward zero.
- **Nothing is tuned.** The weights were chosen by hand. A tuned set could be
  stronger or weaker than what was tested here.
- **Not exactly repeatable.** Search is limited by time, so identical settings give
  different games on different runs. Expect the same picture, not the same
  numbers.
- **Move lists were not saved.** The data files hold the players, opening, result,
  ending, length and duration of every game, but not the moves. Behavioral claims
  such as "Aggressive trades fewer pieces" cannot be checked from this data. Add
  PGN output to the tournament script before testing them.
- **One machine, one binary.** A single-core container, and no other engine
  versions were compared.

---

## Reproduce and extend

Build the engine, install the tools (`pip install -r tools/requirements.txt`),
then:

```bash
# Experiment 1: evaluation only
python -m tools.arena.personality_tournament --games 50 --movetime 30 \
    --out docs/data/personalities.json

# Experiment 2: with opening repertoires
python -m tools.arena.personality_tournament --games 20 --movetime 30 --book \
    --out docs/data/personalities-book.json

# Print the tables used on this page
python -m tools.arena.render_results docs/data/personalities.json
```

Useful follow-ups, in order of value:

1. Add repetition detection and a contempt value to the search, then re-run.
2. Run the tournament at 200 to 500 ms per move.
3. Run about 700 games per pairing for a ±15 Elo margin.
4. Tune the weights in `core/src/personality.cpp` against Balanced, using
   self-play, then re-measure.
5. Save PGNs so play style can be compared, for example captures per game and
   how often each side castles on opposite wings.

Raw data: [`data/personalities.json`](data/personalities.json) and
[`data/personalities-book.json`](data/personalities-book.json).
