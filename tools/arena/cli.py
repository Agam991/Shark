"""Command-line interface for the Shark bot registry.

Examples:
    python3 -m tools.arena.cli list
    python3 -m tools.arena.cli show shark-d6
    python3 -m tools.arena.cli validate
    python3 -m tools.arena.cli snapshot shark-v0.1-d8 --depth 8 --threads 2 \\
        --desc "First tagged build, depth 8"

`snapshot` freezes the CURRENT engine binary into bin/<name> and writes a
manifest, so the bot stays fixed even as the codebase evolves (essential for
honest version-vs-version Elo comparisons).
"""

from __future__ import annotations

import argparse
import shutil
import stat
import subprocess
import sys
from pathlib import Path
from typing import Optional

from . import bots as reg
# NOTE: tournament/match import python-chess. We import them lazily inside the
# royale/gauntlet handlers so `list`/`show`/`validate`/`snapshot` work with only
# the standard library (keeps CI + quick registry ops dependency-free).

# Generated tournament output goes here (served by the web dashboard).
DATA_DIR = reg.REPO_ROOT / "web" / "data"


def _git_commit() -> str:
    """Short HEAD commit, or '' if not in a git checkout."""
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=reg.REPO_ROOT, capture_output=True, text=True, check=True)
        return out.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def _cmd_list(args: argparse.Namespace) -> int:
    roster = reg.load_registry()
    if not roster:
        print("(no bots registered yet -- try 'snapshot')")
        return 0
    name_w = max(len(b.name) for b in roster)
    print(f"{'NAME':<{name_w}}  {'LIMIT':<16}  {'THREADS':<7}  {'BUILT':<5}  DESCRIPTION")
    for b in roster:
        threads = str(b.options.get("Threads", 1))
        built = "yes" if b.engine_exists() else "NO"
        print(f"{b.name:<{name_w}}  {b.limit_summary():<16}  {threads:<7}  "
              f"{built:<5}  {b.description}")
    return 0


def _cmd_show(args: argparse.Namespace) -> int:
    bot = reg.get_bot(args.name)
    if bot is None:
        print(f"error: no bot named {args.name!r}", file=sys.stderr)
        return 1
    print(bot.to_json(), end="")
    marker = "yes" if bot.engine_exists() else "NO (build/snapshot first)"
    print(f"# engine present: {marker}  -> {bot.engine_path()}")
    return 0


def _cmd_validate(args: argparse.Namespace) -> int:
    try:
        roster = reg.load_registry()
    except ValueError as exc:
        print(f"registry INVALID: {exc}", file=sys.stderr)
        return 1
    missing = [b.name for b in roster if not b.engine_exists()]
    print(f"registry OK: {len(roster)} bot(s) validated.")
    if missing:
        print(f"note: {len(missing)} bot(s) have no built binary yet: "
              f"{', '.join(missing)}")
    return 0


def _cmd_snapshot(args: argparse.Namespace) -> int:
    src = Path(args.source)
    if not src.is_absolute():
        src = reg.REPO_ROOT / src
    if not src.is_file():
        print(f"error: engine binary not found: {src}\n"
              f"       build it first (cmake --build build)", file=sys.stderr)
        return 1

    # Exactly one limit.
    chosen = {k: v for k, v in
              (("movetime", args.movetime), ("depth", args.depth),
               ("nodes", args.nodes)) if v is not None}
    if len(chosen) != 1:
        print("error: specify exactly one of --movetime / --depth / --nodes",
              file=sys.stderr)
        return 1

    reg.BIN_DIR.mkdir(parents=True, exist_ok=True)
    dest = reg.BIN_DIR / args.name
    shutil.copy2(src, dest)
    dest.chmod(dest.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    options = {"Hash": args.hash, "Threads": args.threads}
    bot = reg.Bot(
        name=args.name,
        engine=str(dest.relative_to(reg.REPO_ROOT)),
        description=args.desc,
        version=args.bot_version,
        options=options,
        limits=chosen,
        source_commit=_git_commit(),
    )
    path = reg.save_bot(bot)
    print(f"snapshotted {src} -> {dest}")
    print(f"wrote manifest {path.relative_to(reg.REPO_ROOT)}")
    return 0


def _stockfish_bot(skill: int, movetime: Optional[int], depth: Optional[int],
                   path: Optional[str]) -> reg.Bot:
    sf = path or shutil.which("stockfish") or "/opt/homebrew/bin/stockfish"
    if not Path(sf).is_file():
        raise SystemExit(f"error: stockfish not found at {sf} (brew install stockfish)")
    limits = {"movetime": movetime} if movetime else {"depth": depth or 1}
    return reg.Bot(
        name=f"stockfish-skill{skill}",
        engine=sf,
        description=f"Stockfish (Skill Level {skill})",
        options={"Skill Level": skill},
        limits=limits,
    )


def _cmd_royale(args: argparse.Namespace) -> int:
    from . import tournament
    roster = reg.load_registry()
    if args.bots:
        wanted = set(args.bots)
        roster = [b for b in roster if b.name in wanted]
    roster = [b for b in roster if b.engine_exists()]
    if len(roster) < 2:
        print("error: need >= 2 built bots for a royale", file=sys.stderr)
        return 1
    print(f"Bots Royale: {len(roster)} bots, {args.games} games/pair")
    results = tournament.royale(roster, games_per_pair=args.games,
                                max_plies=args.max_plies)
    path = tournament.save_results(results, DATA_DIR, "royale")
    print(f"\nStandings:")
    for s in results["standings"]:
        print(f"  {s['rank']}. {s['name']:<20} {s['points']:.1f} pts "
              f"({s['wins']}-{s['draws']}-{s['losses']})  "
              f"Elo {s['elo']:+.0f} +/- {s['elo_margin']:.0f}")
    print(f"\nwrote {path.relative_to(reg.REPO_ROOT)} (open web dashboard to view)")
    return 0


def _cmd_gauntlet(args: argparse.Namespace) -> int:
    from . import tournament
    challenger = reg.get_bot(args.challenger)
    if challenger is None or not challenger.engine_exists():
        print(f"error: challenger {args.challenger!r} not found / not built",
              file=sys.stderr)
        return 1
    opponent = _stockfish_bot(args.skill, args.sf_movetime, args.sf_depth,
                              args.stockfish)
    print(f"Gauntlet: {challenger.name} vs {opponent.name}, {args.games} games")
    results = tournament.gauntlet(challenger, opponent, games=args.games,
                                  elo0=args.elo0, elo1=args.elo1,
                                  max_plies=args.max_plies)
    path = tournament.save_results(results, DATA_DIR, "gauntlet")
    print(f"\nResult ({challenger.name} POV): "
          f"+{results['wins']} ={results['draws']} -{results['losses']}  "
          f"score {results['score_rate']*100:.1f}%")
    print(f"Elo: {results['elo']:+.1f} +/- {results['elo_margin']:.1f}")
    print(f"SPRT: {results['sprt']['verdict']} "
          f"(LLR {results['sprt']['llr']:.2f})")
    print(f"\nwrote {path.relative_to(reg.REPO_ROOT)} (open web dashboard to view)")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="arena", description="Shark bot registry")
    sub = p.add_subparsers(dest="command", required=True)

    sub.add_parser("list", help="list registered bots").set_defaults(fn=_cmd_list)

    ps = sub.add_parser("show", help="print a bot manifest")
    ps.add_argument("name")
    ps.set_defaults(fn=_cmd_show)

    sub.add_parser("validate", help="validate all manifests").set_defaults(
        fn=_cmd_validate)

    pn = sub.add_parser("snapshot", help="freeze the current binary as a bot")
    pn.add_argument("name", help="unique bot name (no spaces)")
    pn.add_argument("--source", default="core/engine",
                    help="engine binary to freeze (default: core/engine)")
    pn.add_argument("--desc", default="", help="human description")
    pn.add_argument("--bot-version", default="0.1.0", help="version tag")
    pn.add_argument("--threads", type=int, default=1)
    pn.add_argument("--hash", type=int, default=64, help="TT size in MB")
    grp = pn.add_argument_group("search limit (choose one)")
    grp.add_argument("--movetime", type=int, help="ms per move")
    grp.add_argument("--depth", type=int, help="fixed search depth (ply)")
    grp.add_argument("--nodes", type=int, help="fixed node budget")
    pn.set_defaults(fn=_cmd_snapshot)

    pr = sub.add_parser("royale", help="round-robin among registered bots")
    pr.add_argument("--games", type=int, default=4, help="games per pair")
    pr.add_argument("--bots", nargs="*", help="restrict to these bot names")
    pr.add_argument("--max-plies", type=int, default=400)
    pr.set_defaults(fn=_cmd_royale)

    pg = sub.add_parser("gauntlet", help="a bot vs Stockfish (Elo + SPRT)")
    pg.add_argument("challenger", help="a registered bot name")
    pg.add_argument("--games", type=int, default=20)
    pg.add_argument("--skill", type=int, default=0, help="Stockfish Skill Level 0-20")
    pg.add_argument("--sf-movetime", type=int, help="Stockfish ms/move")
    pg.add_argument("--sf-depth", type=int, help="Stockfish fixed depth")
    pg.add_argument("--stockfish", help="path to stockfish binary")
    pg.add_argument("--elo0", type=float, default=0.0, help="SPRT H0 Elo")
    pg.add_argument("--elo1", type=float, default=50.0, help="SPRT H1 Elo")
    pg.add_argument("--max-plies", type=int, default=400)
    pg.set_defaults(fn=_cmd_gauntlet)

    return p


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
