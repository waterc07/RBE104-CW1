"""Run against a freshly built terminal.exe; no SDL or third-party packages."""
import re
import random
import subprocess
import unittest
from pathlib import Path

EXE = Path(__file__).resolve().parents[1] / "build/windows/terminal.exe"
HUD = re.compile(r"Score: (\d+) \| Lives: (\d+) \| Remaining: (\d+) \| Turns: (\d+)")


def play(lines):
    result = subprocess.run(
        [str(EXE)], input="\n".join(lines) + "\n", text=True,
        capture_output=True, timeout=10, check=True,
    )
    return result.stdout


class TerminalTests(unittest.TestCase):
    def test_delivery_pause_and_restart(self):
        output = play(["1", "42", "P", "s", "P", "w", "x", "ss", "s" * 100,
                       *"sssdawww", "R", *"sssdawww", "Q"])
        states = [tuple(map(int, m)) for m in HUD.findall(output)]
        self.assertIn("PAUSED", output)
        self.assertIn("Enter exactly one command", output)
        self.assertIn("Use W A S D, P, R or Q", output)
        self.assertEqual(states[:6], [(0, 3, 3, 0)] * 6)
        first_delivery = states.index((100, 3, 2, 8))
        self.assertEqual(states[first_delivery + 1], (0, 3, 3, 0))
        self.assertEqual(states[-1], (100, 3, 2, 8))
        self.assertIn("Quit. Score: 100 | Delivered: 1/3", output)

    def test_loss_allows_restart_and_blocks_movement(self):
        rng = random.Random(42)
        route = [rng.choice("wasd") for _ in range(3000)]
        output = play(["4", "42", *route, "p", "s", "r", "q"])
        self.assertTrue("GAME OVER" in output)
        states = [tuple(map(int, m)) for m in HUD.findall(output)]
        first_loss = next(i for i, state in enumerate(states) if state[1] == 0)
        self.assertTrue(all(state == states[first_loss] for state in states[first_loss:-1]))
        self.assertEqual(states[-1], (0, 3, 3, 0))

    def test_win_allows_restart_and_blocks_movement(self):
        rng = random.Random(42)
        route = [rng.choice("wasd") for _ in range(3000)]
        output = play(["1", "42", *route, "p", "s", "r", "q"])
        self.assertTrue("YOU WIN!" in output)
        states = [tuple(map(int, m)) for m in HUD.findall(output)]
        first_win = next(i for i, state in enumerate(states) if state[2] == 0)
        self.assertTrue(all(state == states[first_win] for state in states[first_win:-1]))
        self.assertEqual(states[-1], (0, 3, 3, 0))

    def test_numeric_validation_and_eof(self):
        output = play(["0", "5", "1", "100000", "-1", "42"])
        self.assertEqual(output.count("Enter a whole number from 1 to 4."), 2)
        self.assertEqual(output.count("Enter a whole number from 0 to 99999."), 2)
        self.assertIn("Input ended. Goodbye.", output)
        self.assertIn("Input ended. Goodbye.", play([]))


if __name__ == "__main__":
    unittest.main()
