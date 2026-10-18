# Shark Tooling: Arena & Bot Registry

Developer tooling for running the **Bots Royale** and **Stockfish Gauntlet**.
Pure Python standard library (no third-party deps yet).

## Bot registry (Phase 5)

A **bot** is a frozen fighter: an engine binary + the UCI options and search
limits that define how it plays. Same binary + different limits = different
strength, which is what makes a Royale meaningful.

Manifests live in [`bots/`](../bots) as committed JSON. Binaries are **not**
committed; a manifest points at a binary path, and `snapshot` can freeze the
current build into `bin/` (gitignored) while recording the git commit it came
from for reproducibility.

### CLI

Run from the repo root:

```bash
# list the roster (BUILT column shows if the binary is present)
python3 -m tools.arena.cli list

# print one manifest
python3 -m tools.arena.cli show shark-d6

# validate every manifest (used in CI later)
python3 -m tools.arena.cli validate

# freeze the CURRENT engine binary as a permanent versioned bot
python3 -m tools.arena.cli snapshot shark-v0.1-d8 \
    --depth 8 --threads 2 --hash 128 \
    --desc "First tagged build, depth 8"
```

### Manifest format

```json
{
  "name": "shark-d6",
  "engine": "core/engine",
  "protocol": "uci",
  "options": { "Hash": 64, "Threads": 1 },
  "limits":  { "depth": 6 },
  "description": "The middleweight.",
  "version": "0.1.0",
  "source_commit": ""
}
```

Rules (enforced by `validate`):
- `name` is non-empty and space-free (unique across the roster)
- exactly **one** of `movetime` / `depth` / `nodes` in `limits`, positive int
- `protocol` must be `uci`

### Tests

```bash
python3 -m unittest discover -s tools/tests
```

## Coming next (Phase 6)
- `cutechess-cli`-driven round-robin (Bots Royale) with Elo + crosstable
- Stockfish gauntlet: thousands of games, Elo + SPRT verdict, PGN output
