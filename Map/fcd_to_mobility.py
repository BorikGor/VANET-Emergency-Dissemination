#!/usr/bin/env python3
"""
Convert SUMO FCD XML to Cooja mobility.dat.

Output format per line:
    moteID time_seconds x y

Formatting constraints:
- time:    1 digit after decimal (%.1f)
- x, y:    2 digits after decimal (%.2f)

Mapping rules:
- Static motes 0..5 are written at time 0.0 with fixed coordinates.
- Moving motes start at ID 6 and map to SUMO vehicle IDs.
- Time is re-based so first timestep in the XML becomes time 0.0.
"""

from __future__ import annotations

import argparse
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Optional, Tuple


def parse_args() -> argparse.Namespace:
    """
    Parse CLI arguments.

    Methods used:
    - argparse.ArgumentParser

    Variables created:
    - parser: argument parser
    - args: parsed arguments

    Returns:
    - argparse.Namespace with input/output paths and options
    """
    parser = argparse.ArgumentParser(
        description="Convert SUMO FCD XML to Cooja mobility.dat."
    )
    parser.add_argument(
        "--in",
        dest="in_path",
        required=True,
        help="Input SUMO FCD XML file path.",
    )
    parser.add_argument(
        "--out",
        dest="out_path",
        required=True,
        help="Output mobility.dat file path.",
    )
    parser.add_argument(
        "--moving-start-id",
        type=int,
        default=6,
        help="First mote ID for moving vehicles (default: 6).",
    )
    parser.add_argument(
        "--repeat-static",
        action="store_true",
        help=(
            "If set, repeats static mote coordinates at every timestep "
            "(some mobility plugins require this)."
        ),
    )
    return parser.parse_args()


def get_default_static_motes() -> Dict[int, Tuple[float, float]]:
    """
    Return the default 6 static mote coordinates.

    Variables created:
    - dictionary: mote_id -> (x, y)

    Returns:
    - dict of static mote coordinates
    """
    return {
        0: (210.0, -440.0),
        1: (380.0, -670.0),
        2: (430.0, -820.0),
        3: (520.0, -350.0),
        4: (550.0, -470.0),
        5: (850.0, -530.0),
    }


def iter_fcd_timesteps(
    xml_path: Path,
) -> Iterator[Tuple[float, List[Tuple[str, float, float]]]]:
    """
    Stream-parse SUMO FCD XML and yield timesteps.

    Methods used:
    - xml.etree.ElementTree.iterparse for streaming

    Variables created:
    - context: streaming parser events
    - root: root element handle for memory clearing
    - current_time: timestep time as float
    - vehicles: list of (vehicle_id, x, y) for current timestep

    Yields:
    - (time_seconds, [(veh_id, x, y), ...]) for each timestep
    """
    context = ET.iterparse(xml_path, events=("start", "end"))
    _, root = next(context)

    current_time: Optional[float] = None
    vehicles: List[Tuple[str, float, float]] = []

    for event, elem in context:
        tag = elem.tag

        if event == "start" and tag.endswith("timestep"):
            current_time = float(elem.attrib.get("time", "0"))
            vehicles = []

        elif event == "end" and tag.endswith("vehicle"):
            vid = elem.attrib.get("id")
            if vid is None:
                elem.clear()
                continue
            x = float(elem.attrib.get("x", "0"))
            y = float(elem.attrib.get("y", "0"))
            vehicles.append((vid, x, y))
            elem.clear()

        elif event == "end" and tag.endswith("timestep"):
            if current_time is None:
                current_time = 0.0
            yield current_time, vehicles
            elem.clear()
            root.clear()


def build_vehicle_mapping(
    first_vehicles: Iterable[Tuple[str, float, float]],
    moving_start_id: int,
) -> Dict[str, int]:
    """
    Build deterministic mapping from SUMO vehicle IDs to mote IDs.

    Methods used:
    - sorting vehicle IDs

    Variables created:
    - ids: sorted list of vehicle IDs
    - mapping: vehicle_id -> mote_id

    Returns:
    - mapping dict
    """
    ids = sorted(v[0] for v in first_vehicles)
    return {vid: moving_start_id + i for i, vid in enumerate(ids)}


def write_static_motes(
    f,
    static_motes: Dict[int, Tuple[float, float]],
    t: float,
) -> None:
    """
    Write static mote positions at time t.

    Methods used:
    - file write

    Variables created:
    - none (iteration only)

    Notes:
    - time uses 1 decimal, x/y use 2 decimals
    """
    for mote_id in sorted(static_motes.keys()):
        x, y = static_motes[mote_id]
        f.write(f"{mote_id} {t:.1f} {x:.2f} {y:.2f}\n")


def convert_fcd_to_mobility(
    in_path: Path,
    out_path: Path,
    static_motes: Dict[int, Tuple[float, float]],
    moving_start_id: int,
    repeat_static: bool,
) -> None:
    """
    Convert a SUMO FCD XML file to a Cooja mobility.dat file.

    Methods used:
    - iter_fcd_timesteps for streaming parse
    - build_vehicle_mapping for stable mote numbering

    Variables created:
    - t0: first timestep time (for re-basing to 0.0)
    - mapping: SUMO vehicle id -> mote id
    - line_items: list of (mote_id, t, x, y) per timestep

    Output:
    - mobility.dat with required formatting:
      moteID time(1dp) x(2dp) y(2dp)
    """
    ts_iter = iter_fcd_timesteps(in_path)

    try:
        first_time, first_vehicles = next(ts_iter)
    except StopIteration:
        raise RuntimeError("Input XML has no timesteps/vehicles.")

    t0 = first_time
    mapping = build_vehicle_mapping(first_vehicles, moving_start_id)

    out_path.parent.mkdir(parents=True, exist_ok=True)

    with out_path.open("w", encoding="utf-8") as f:
        # Static motes appear at t=0.0
        write_static_motes(f, static_motes, 0.0)

        # Helper to write one timestep worth of moving motes
        def write_moving(t_abs: float,
                         vehicles: List[Tuple[str, float, float]]) -> None:
            """
            Write moving mote lines for one timestep.

            Methods used:
            - mapping lookup
            - sorting by mote id

            Variables created:
            - t_rel: time relative to t0
            - line_items: list of (mote_id, t_rel, x, y)
            """
            t_rel = t_abs - t0
            line_items: List[Tuple[int, float, float, float]] = []

            for vid, x, y in vehicles:
                mote_id = mapping.get(vid)
                if mote_id is None:
                    mote_id = moving_start_id + len(mapping)
                    mapping[vid] = mote_id
                # Y axis in Cooja is upside-down relative to SUMO coordinates. -y compensates for that.
                line_items.append((mote_id, t_rel, x, -y))

            line_items.sort(key=lambda r: r[0])

            for mote_id, t_rel, x, y in line_items:
                f.write(f"{mote_id} {t_rel:.1f} {x:.2f} {y:.2f}\n")

        # First timestep
        if repeat_static:
            write_static_motes(f, static_motes, 0.0)
        write_moving(first_time, first_vehicles)

        # Remaining timesteps
        for t, vehicles in ts_iter:
            t_rel = t - t0
            if repeat_static:
                write_static_motes(f, static_motes, t_rel)
            write_moving(t, vehicles)


def main() -> None:
    """
    Program entry point.

    Methods used:
    - parse_args
    - convert_fcd_to_mobility

    Variables created:
    - args: CLI arguments
    - static_motes: default static mote positions
    """
    args = parse_args()
    in_path = Path(args.in_path)
    out_path = Path(args.out_path)

    static_motes = get_default_static_motes()

    convert_fcd_to_mobility(
        in_path=in_path,
        out_path=out_path,
        static_motes=static_motes,
        moving_start_id=args.moving_start_id,
        repeat_static=args.repeat_static,
    )


if __name__ == "__main__":
    main()