import collections
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import stalk


class MeasuredProfileTests(unittest.TestCase):
    def test_measured_button_masks(self):
        for mask, button in [(0x0104, "VOL_PLUS"), (0x2140, "VOL_MINUS"),
                             (0x2001, "VOL_MINUS"), (0x0401, "PHONE"),
                             (0x4001, "SOURCE_AUDIO"), (0x0260, "MODE_OK"),
                             (0x0300, "MODE_OK")]:
            with self.subTest(mask=hex(mask)):
                self.assertEqual(stalk.decode(mask).button, button)

    def test_chord_hides_two_wheel_positions(self):
        edges = stalk.BUTTONS["VOL_BOTH"]
        self.assertEqual(stalk.closure((*edges, (2, 1))),
                         stalk.closure((*edges, (2, 6))))
        state = stalk.decode(0x2155)
        self.assertEqual(state.button, "VOL_BOTH")
        self.assertEqual(state.wheels, frozenset({1, 2}))
        self.assertEqual(stalk.decode(0x4DA0).button, "TOP_BOTH")

    def test_replay_measured_rapid_buttons(self):
        events = stalk.replay(stalk.load_records(ROOT / "captures/rapid-volume-20261108-015829.json"))
        presses = [item["event"] for item in events if item["event"].startswith("PRESS_")]
        self.assertEqual(presses, ["PRESS_VOL_PLUS"] * 10 + ["PRESS_VOL_MINUS"] * 10)

    def test_replay_wheel_counts(self):
        events = stalk.replay(stalk.load_records(ROOT / "captures/wheel-five-each-20261108-015952.json"))
        turns = [item["event"] for item in events if item["event"].startswith("WHEEL_")]
        self.assertEqual(turns, ["WHEEL_UP"] * 5 + ["WHEEL_DOWN"] * 5)

    def test_wheel_alone_has_no_buttons(self):
        events = stalk.replay(stalk.load_records(ROOT / "captures/wheel-only-20261108-015442.json"))
        self.assertFalse([item for item in events if item["event"].startswith("PRESS_")])
        self.assertFalse([item for item in events if item["event"] == "UNKNOWN"])

    def test_initial_hold_is_not_a_press(self):
        decoder = stalk.Decoder()
        self.assertEqual(decoder.update(0x0104), ["INITIAL_VOL_PLUS"])
        self.assertEqual(decoder.update(0x0104), [])
        self.assertEqual(decoder.update(0x0100), ["RELEASE_VOL_PLUS"])

    def test_unknown_does_not_invent_direction(self):
        decoder = stalk.Decoder()
        decoder.update(0x0020)
        self.assertEqual(decoder.update(0xFFFF), ["UNKNOWN"])
        self.assertEqual(decoder.update(0x0001), [])

    def test_replay_chord_and_wheel_has_one_hold(self):
        paths = sorted((ROOT / "captures").glob("volume-chord-wheel-*.json"))
        self.assertTrue(paths)
        events = stalk.replay(stalk.load_records(paths[-1]))
        self.assertEqual([event["event"] for event in events],
                         ["INITIAL_NONE", "PRESS_VOL_BOTH", "RELEASE_VOL_BOTH"])


if __name__ == "__main__":
    unittest.main()
