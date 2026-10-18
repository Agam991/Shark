"""Rating math: Elo estimates, error margins, and SPRT verdicts.

Kept dependency-free (pure math). These operate on aggregate W/D/L counts from a
match, which is all the Elo/SPRT machinery needs.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


def _clamp(x: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, x))


def elo_from_score(score_rate: float) -> float:
    """Elo difference implied by a score rate in (0, 1)."""
    s = _clamp(score_rate, 1e-6, 1 - 1e-6)
    return -400.0 * math.log10(1.0 / s - 1.0)


@dataclass
class EloEstimate:
    elo: float
    margin: float  # +/- 95% confidence half-width
    score_rate: float
    games: int

    def __str__(self) -> str:
        sign = "+" if self.elo >= 0 else ""
        return f"{sign}{self.elo:.1f} +/- {self.margin:.1f} Elo"


def elo_with_error(wins: int, draws: int, losses: int) -> EloEstimate:
    """Elo point estimate with a 95% confidence margin from W/D/L."""
    n = wins + draws + losses
    if n == 0:
        return EloEstimate(0.0, 0.0, 0.5, 0)

    w, d, l = wins / n, draws / n, losses / n
    score = w + d / 2.0
    # Variance of a single game's points (0, 0.5, 1 outcomes).
    m2 = w * 1.0 + d * 0.25 + l * 0.0
    var = m2 - score * score
    stddev = math.sqrt(var / n) if var > 0 else 1e-6

    elo = elo_from_score(score)
    # Convert the score confidence interval to Elo at the interval edges.
    lo = elo_from_score(_clamp(score - 1.96 * stddev, 1e-6, 1 - 1e-6))
    hi = elo_from_score(_clamp(score + 1.96 * stddev, 1e-6, 1 - 1e-6))
    margin = (hi - lo) / 2.0
    return EloEstimate(elo, margin, score, n)


@dataclass
class SprtResult:
    llr: float
    lower: float
    upper: float
    verdict: str  # "H1 accepted" / "H0 accepted" / "continue"

    def __str__(self) -> str:
        return (f"LLR {self.llr:.2f} (bounds [{self.lower:.2f}, "
                f"{self.upper:.2f}]) -> {self.verdict}")


def sprt(wins: int, draws: int, losses: int,
         elo0: float = 0.0, elo1: float = 20.0,
         alpha: float = 0.05, beta: float = 0.05) -> SprtResult:
    """Sequential Probability Ratio Test on W/D/L.

    H0: the Elo difference is `elo0`.  H1: it is `elo1`.
    Uses the standard logistic-model LLR approximation (Van den Bergh),
    which is accurate for the trinomial result model.
    """
    lower = math.log(beta / (1.0 - alpha))
    upper = math.log((1.0 - beta) / alpha)

    n = wins + draws + losses
    if n == 0:
        return SprtResult(0.0, lower, upper, "continue")

    w, d, l = wins / n, draws / n, losses / n
    score = w + d / 2.0
    m2 = w * 1.0 + d * 0.25
    var = m2 - score * score
    if var <= 0:
        var = 1e-6

    def score_of(elo: float) -> float:
        return 1.0 / (1.0 + 10.0 ** (-elo / 400.0))

    s0, s1 = score_of(elo0), score_of(elo1)
    llr = n * (s1 - s0) * (2.0 * score - s0 - s1) / (2.0 * var)

    if llr >= upper:
        verdict = "H1 accepted"
    elif llr <= lower:
        verdict = "H0 accepted"
    else:
        verdict = "continue"
    return SprtResult(llr, lower, upper, verdict)
