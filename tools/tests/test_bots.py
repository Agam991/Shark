"""Tests for the Shark bot registry (stdlib unittest, no deps).

Run:  python3 -m unittest discover -s tools/tests
"""

import json
import tempfile
import unittest
from pathlib import Path

from tools.arena import bots as reg


class BotValidationTests(unittest.TestCase):
    def _bot(self, **over):
        base = dict(name="b1", engine="core/engine", limits={"depth": 6})
        base.update(over)
        return reg.Bot(**base)

    def test_valid_bot_passes(self):
        self._bot().validate()  # should not raise

    def test_name_must_be_space_free(self):
        with self.assertRaises(ValueError):
            self._bot(name="bad name").validate()

    def test_exactly_one_limit_required(self):
        with self.assertRaises(ValueError):
            self._bot(limits={}).validate()
        with self.assertRaises(ValueError):
            self._bot(limits={"depth": 6, "movetime": 100}).validate()

    def test_unknown_limit_rejected(self):
        with self.assertRaises(ValueError):
            self._bot(limits={"forever": 1}).validate()

    def test_positive_limit_required(self):
        with self.assertRaises(ValueError):
            self._bot(limits={"depth": 0}).validate()

    def test_non_uci_protocol_rejected(self):
        with self.assertRaises(ValueError):
            self._bot(protocol="xboard").validate()

    def test_limit_summary(self):
        self.assertEqual(self._bot(limits={"movetime": 250}).limit_summary(),
                         "movetime=250ms")


class RegistryRoundTripTests(unittest.TestCase):
    def test_save_and_load(self):
        with tempfile.TemporaryDirectory() as d:
            dd = Path(d)
            bot = reg.Bot(name="rt", engine="core/engine",
                          limits={"nodes": 100000}, description="round trip")
            path = reg.save_bot(bot, dd)
            self.assertTrue(path.is_file())
            loaded = reg.load_bot(path)
            self.assertEqual(loaded.name, "rt")
            self.assertEqual(loaded.limits, {"nodes": 100000})

    def test_duplicate_names_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            dd = Path(d)
            (dd / "a.json").write_text(json.dumps(
                {"name": "dup", "engine": "e", "limits": {"depth": 4}}))
            (dd / "b.json").write_text(json.dumps(
                {"name": "dup", "engine": "e", "limits": {"depth": 5}}))
            with self.assertRaises(ValueError):
                reg.load_registry(dd)

    def test_unknown_manifest_key_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x.json"
            p.write_text(json.dumps(
                {"name": "x", "engine": "e", "limits": {"depth": 4},
                 "bogus": 1}))
            with self.assertRaises(ValueError):
                reg.load_bot(p)


class ShippedRegistryTests(unittest.TestCase):
    """The committed bots/ roster must always be valid."""

    def test_committed_roster_is_valid(self):
        roster = reg.load_registry()
        self.assertGreaterEqual(len(roster), 1)
        for bot in roster:
            bot.validate()


if __name__ == "__main__":
    unittest.main()


class PersonalityTests(unittest.TestCase):
    def test_with_personality_sets_option_and_keeps_limits(self):
        from tools.arena.play_one import with_personality
        b = reg.Bot(name="b1", engine="core/engine", limits={"depth": 6},
                    options={"Hash": 64})
        out = with_personality(b, "aggressive")
        self.assertEqual(out.options["Personality"], "Aggressive")
        self.assertEqual(out.options["Hash"], 64)
        self.assertEqual(out.limits, {"depth": 6})
        self.assertNotIn("Personality", b.options)  # original untouched

    def test_unknown_personality_falls_back_to_balanced(self):
        from tools.arena.play_one import norm_personality
        self.assertEqual(norm_personality("reckless"), "Balanced")
        self.assertEqual(norm_personality(None), "Balanced")
