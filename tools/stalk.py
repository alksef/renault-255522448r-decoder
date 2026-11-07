"""Measured Renault 255522448R profile: live CDC capture and offline replay.

Pin numbers are project-defined, viewed INTO the stalk socket. No KEY output.
Wheel directions are observations between reported states, not guaranteed detents.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import itertools
import json
from pathlib import Path
import re
import time

PAIRS = tuple(itertools.combinations(range(1, 7), 2))
BUTTONS = {
    "VOL_PLUS": ((1, 4),),
    "VOL_MINUS": ((4, 6),),
    "PHONE": ((3, 5),),
    "SOURCE_AUDIO": ((5, 6),),
    "MODE_OK": ((3, 4),),
    "VOL_BOTH": ((1, 4), (4, 6)),
    "TOP_BOTH": ((3, 5), (5, 6)),
}
# Observed upward sequence: 2-3 -> 2-6 -> 2-1 -> 2-3.
WHEELS = ((2, 3), (2, 6), (2, 1))


def closure(edges):
    groups = [{i} for i in range(7)]
    for a, b in edges:
        merged = groups[a] | groups[b]
        for i in merged:
            groups[i] = merged
    return sum(1 << bit for bit, (a, b) in enumerate(PAIRS) if b in groups[a])


PROFILES = {}
for _button, _edges in {"NONE": (), **BUTTONS}.items():
    for _wheel, _edge in [(-1, None), *enumerate(WHEELS)]:
        _mask = closure((*_edges, *((_edge,) if _edge else ())))
        PROFILES.setdefault(_mask, set()).add((_button, _wheel))


@dataclass(frozen=True)
class State:
    button: str
    wheels: frozenset[int]


def decode(mask: int) -> State | None:
    options = PROFILES.get(mask, set())
    buttons = {button for button, _ in options}
    if len(buttons) != 1:
        return None
    return State(next(iter(buttons)), frozenset(wheel for _, wheel in options))


def parse_mask(line: str) -> int | None:
    match = re.fullmatch(r"RAW ([0-9a-fA-F]{4})", line.strip())
    if not match and line.startswith("STATE "):
        match = re.search(r"\braw=([0-9a-fA-F]{4})\b", line)
    return int(match[1], 16) if match else None


class Decoder:
    def __init__(self):
        self.button = None
        self.wheel = None

    def update(self, mask: int) -> list[str]:
        state = decode(mask)
        if state is None:
            # Do not invent a release or bridge direction across an unknown graph.
            self.wheel = None
            return ["UNKNOWN"]
        events = []
        if self.button is None:
            # Opening the port during a hold is a baseline, not a new press.
            events.append("INITIAL_" + state.button)
        elif state.button != self.button:
            if self.button != "NONE":
                events.append("RELEASE_" + self.button)
            if state.button != "NONE":
                events.append("PRESS_" + state.button)
        self.button = state.button
        if state.button in ("VOL_BOTH", "TOP_BOTH"):
            # Chords merge wheel contacts: do not emit wheel movement across them.
            self.wheel = None
        elif len(state.wheels) == 1:
            wheel = next(iter(state.wheels))
            if wheel >= 0:
                if self.wheel is not None and wheel != self.wheel:
                    events.append("WHEEL_UP" if (wheel - self.wheel) % 3 == 1 else "WHEEL_DOWN")
                self.wheel = wheel
            # -1 means the contacts are open. Preserve the last seated wheel state.
        else:
            self.wheel = None
        return events


def replay(records):
    decoder = Decoder()
    events = []
    for record in records:
        mask = parse_mask(record["line"])
        if mask is None:
            continue
        for event in decoder.update(mask):
            events.append({"t": record["t"], "event": event, "mask": f"{mask:04X}"})
    return events


def load_records(path):
    data = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    return data if isinstance(data, list) else data["records"]


def show(events):
    for item in events:
        print(f'{item["t"]:8.3f} {item["event"]:28} RAW={item["mask"]}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="COM5 or /dev/ttyACM0")
    source.add_argument("--replay", type=Path, help="Replay one supplied JSON capture")
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--output", type=Path, help="Save live capture; existing files are not overwritten")
    args = parser.parse_args()
    if args.replay:
        show(replay(load_records(args.replay)))
        return
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    # Open the output before touching hardware; fail early for existing files.
    output = args.output.open("x", encoding="utf-8") if args.output else None
    records = []
    try:
        import serial
        decoder = Decoder()
        with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=1) as port:
            time.sleep(0.3)
            # Compatible with the original bench firmware and the standalone scanner.
            # DISARM / PROFILE OFF disable legacy output and the unverified old profile.
            # The standalone scanner responds ERR COMMAND to these harmless commands.
            port.write(b"DISARM\nPROFILE OFF\nRAW ON\nSTATE\n")
            start = time.monotonic()
            while time.monotonic() - start < args.seconds:
                line = port.readline().decode("ascii", errors="replace").strip()
                if not line:
                    continue
                elapsed = round(time.monotonic() - start, 3)
                records.append({"t": elapsed, "line": line})
                mask = parse_mask(line)
                if mask is not None:
                    show([{"t": elapsed, "event": event, "mask": f"{mask:04X}"}
                          for event in decoder.update(mask)])
                elif line.startswith("ERR"):
                    print(line, flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        if output:
            json.dump({"raw_filter_ms": 20, "records": records}, output, indent=2)
            output.write("\n")
            output.close()


if __name__ == "__main__":
    main()
