"""Integration checks against freshly built terminal.exe and test_game.exe."""
import random
import re
import subprocess
import unittest
from pathlib import Path

BUILD = Path(__file__).resolve().parents[1] / "build/windows"
HUD = re.compile(r"Score: (\d+) \| Lives: (\d+) \| Remaining: (\d+) \| Turns: (\d+)")


def play(lines):
    return subprocess.run([str(BUILD / "terminal.exe")], input="\n".join(lines) + "\n",
                          text=True, capture_output=True, timeout=30, check=True).stdout


def winning_route(level, count):
    return subprocess.check_output([str(BUILD / "test_game.exe"), "--route", str(level), str(count)],
                                   text=True, timeout=30).strip()


class TerminalTests(unittest.TestCase):
    def test_delivery_pause_restart_and_replay(self):
        route = winning_route(42, 2)
        output = play(["2", "42", "P", "s", "P", "w", "x", "ss", "s" * 100,
                       *route.upper(), "P", "S", "R", *route, "Q"])
        states = [tuple(map(int, match)) for match in HUD.findall(output)]
        self.assertIn("PAUSED", output)
        self.assertIn("Enter exactly one command", output)
        self.assertIn("Use W A S D, P, R, N or Q", output)
        self.assertEqual(states[:6], [(0, 3, 3, 0)] * 6)
        wins = [state for state in states if state[2] == 0]
        self.assertTrue(wins)
        self.assertTrue(all(state == wins[0] for state in wins))
        self.assertIn("Quit. Score: 300 | Delivered: 3/3", output)

    def test_win_gating_and_next_level(self):
        for count in range(1, 5):
            with self.subTest(count=count):
                route = winning_route(999, count)
                output = play([str(count), "999", *route, "p", "s", "n", "q"])
                states = [tuple(map(int, match)) for match in HUD.findall(output)]
                first_win = next(i for i, state in enumerate(states) if state[2] == 0)
                self.assertTrue(all(state == states[first_win] for state in states[first_win:-1]))
                self.assertEqual(states[-1], (0, 3, 4, 0))
                self.assertIn("Level: 1000 | Map: 9x19", output)

    def test_loss_allows_restart_and_blocks_movement(self):
        rng = random.Random(42)
        route = [rng.choice("wasd") for _ in range(5000)]
        output = play(["4", "42", *route, "p", "s", "r", "q"])
        self.assertIn("GAME OVER", output)
        states = [tuple(map(int, match)) for match in HUD.findall(output)]
        first_loss = next(i for i, state in enumerate(states) if state[1] == 0)
        self.assertTrue(all(state == states[first_loss] for state in states[first_loss:-1]))
        self.assertEqual(states[-1], (0, 3, 3, 0))

    def test_natural_health_drop(self):
        output = play(["1", "4", *"dadadadadad", "q"])
        self.assertIn("Health: WAITING", output)
        self.assertIn("Health dropped!", output)
        self.assertIn("Health: ON MAP", output)
        self.assertIn("Health: NONE THIS LEVEL", play(["1", "1", "q"]))

    def test_numeric_validation_and_eof(self):
        self.assertIn("Level: 1 | Map: 9x15", play(["2", "", "q"]))
        output = play(["0", "5", "1", "0", "100000", "-1", "42"])
        self.assertEqual(output.count("Enter a whole number from 1 to 4."), 2)
        self.assertEqual(output.count("Enter a whole number from 1 to 99999."), 3)
        self.assertIn("Input ended. Goodbye.", output)
        for lines in ([], ["1"], ["1", "42"]):
            self.assertIn("Input ended. Goodbye.", play(lines))

    def test_dimensions_and_last_level(self):
        for level, dimensions in ((1, "9x15"), (1000, "9x19"), (10000, "11x23"),
                                  (30000, "13x27"), (60000, "17x31")):
            with self.subTest(level=level):
                self.assertIn(f"Level: {level} | Map: {dimensions}", play(["1", str(level), "q"]))
        route = winning_route(99999, 1)
        output = play(["1", "99999", *route, "n", "q"])
        self.assertIn("Last level number", output)
        self.assertNotIn("Level: 100000", output)

    def test_package_counts_and_full_delivery(self):
        for level, packages in ((1, 3), (1000, 4), (10000, 4), (30000, 5), (60000, 5)):
            with self.subTest(level=level):
                route = winning_route(level, 1)
                output = play(["1", str(level), *route, "r", "q"])
                states = [tuple(map(int, match)) for match in HUD.findall(output)]
                self.assertEqual(states[0], (0, 3, packages, 0))
                wins = [state for state in states if state[2] == 0]
                self.assertTrue(wins)
                self.assertTrue(all(state[0] == packages * 100 and state[1] > 0 for state in wins))
                self.assertEqual(states[-1], states[0])


if __name__ == "__main__":
    unittest.main()
