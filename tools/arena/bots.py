"""Bot registry: load, validate, and describe versioned engine configurations.

A *bot* is a frozen fighter for the arena: an engine binary plus the UCI
options and search limits that define how it plays. Two bots can share the same
binary yet play at very different strengths (e.g. depth 4 vs depth 10) -- which
is exactly what makes a Bots Royale interesting.

Manifests are plain JSON files under ``bots/`` so they are diff-friendly and
committed to git. Binaries are NOT committed (see .gitignore); a manifest points
at a binary path, and ``snapshot`` can freeze the current build into ``bin/``.

Design notes:
* stdlib-only (json, dataclasses, pathlib) -- no third-party deps.
* Fail loudly on malformed manifests; a silent bad bot corrupts a whole
  tournament's Elo math.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

# Repo layout anchors (…/tools/arena/bots.py -> repo root is two parents up).
REPO_ROOT = Path(__file__).resolve().parents[2]
BOTS_DIR = REPO_ROOT / "bots"
BIN_DIR = REPO_ROOT / "bin"

# UCI search-limit keys we understand. Exactly one "primary" limit should be set
# (movetime / depth / nodes); cutechess uses tc/st, but per-bot st is handy.
LIMIT_KEYS = {"movetime", "depth", "nodes"}


@dataclass
class Bot:
    """A single versioned fighter."""

    name: str
    engine: str                       # path to the engine binary (repo-relative ok)
    description: str = ""
    version: str = "0.0.0"
    protocol: str = "uci"
    options: Dict[str, Any] = field(default_factory=dict)   # UCI setoption values
    limits: Dict[str, int] = field(default_factory=dict)    # movetime/depth/nodes
    source_commit: str = ""           # git ref this bot was frozen from (optional)

    # ---- Validation ---------------------------------------------------------
    def validate(self) -> None:
        if not self.name or " " in self.name:
            raise ValueError(f"bot name must be non-empty and space-free: {self.name!r}")
        if self.protocol != "uci":
            raise ValueError(f"{self.name}: only the 'uci' protocol is supported")
        if not self.engine:
            raise ValueError(f"{self.name}: 'engine' path is required")
        bad = set(self.limits) - LIMIT_KEYS
        if bad:
            raise ValueError(f"{self.name}: unknown limit keys {sorted(bad)}")
        if len(self.limits) != 1:
            raise ValueError(
                f"{self.name}: exactly one of {sorted(LIMIT_KEYS)} must be set, "
                f"got {sorted(self.limits)}"
            )
        for k, v in self.limits.items():
            if not isinstance(v, int) or v <= 0:
                raise ValueError(f"{self.name}: limit {k} must be a positive int")

    # ---- Convenience --------------------------------------------------------
    def engine_path(self) -> Path:
        """Absolute path to the engine binary."""
        p = Path(self.engine)
        return p if p.is_absolute() else (REPO_ROOT / p)

    def engine_exists(self) -> bool:
        return self.engine_path().is_file()

    def limit_summary(self) -> str:
        k, v = next(iter(self.limits.items()))
        unit = {"movetime": "ms", "depth": "ply", "nodes": "nodes"}[k]
        return f"{k}={v}{unit}"

    def to_json(self) -> str:
        return json.dumps(asdict(self), indent=2, sort_keys=True) + "\n"


def _from_dict(data: Dict[str, Any]) -> Bot:
    known = {f: data[f] for f in Bot.__dataclass_fields__ if f in data}
    unknown = set(data) - set(Bot.__dataclass_fields__)
    if unknown:
        raise ValueError(f"unknown manifest keys {sorted(unknown)}")
    return Bot(**known)


def load_bot(path: Path) -> Bot:
    """Load and validate a single manifest file."""
    with path.open() as fh:
        bot = _from_dict(json.load(fh))
    bot.validate()
    return bot


def load_registry(bots_dir: Path = BOTS_DIR) -> List[Bot]:
    """Load every ``*.json`` manifest in ``bots_dir`` (sorted by name)."""
    if not bots_dir.is_dir():
        return []
    bots = [load_bot(p) for p in sorted(bots_dir.glob("*.json"))]
    names = [b.name for b in bots]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        raise ValueError(f"duplicate bot names in registry: {sorted(dupes)}")
    return bots


def get_bot(name: str, bots_dir: Path = BOTS_DIR) -> Optional[Bot]:
    for bot in load_registry(bots_dir):
        if bot.name == name:
            return bot
    return None


def save_bot(bot: Bot, bots_dir: Path = BOTS_DIR) -> Path:
    """Validate and write a manifest to ``bots_dir/<name>.json``."""
    bot.validate()
    bots_dir.mkdir(parents=True, exist_ok=True)
    path = bots_dir / f"{bot.name}.json"
    path.write_text(bot.to_json())
    return path
