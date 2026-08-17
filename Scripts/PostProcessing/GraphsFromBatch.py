#!/usr/bin/env python3
"""
GraphsFromBatch_v2.py

Parse a batch metrics log and generate graphs + a CSV summary.

This version is hardened against:
- escaped underscores in FILE names (\\_),
- FILE containing a path, not just a basename,
- missing 'scenario' column (fallback to --scenario).
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# --- Plot style: readable fonts for 2-up / thesis layouts ---
def apply_plot_style(fontscale: float = 1.6) -> None:
    """
    Increase fonts/line widths while keeping the same figure size/layout.
    fontscale=1.0 corresponds roughly to Matplotlib defaults.
    """
    base = 7 * fontscale  # base font size in points

    plt.rcParams.update({
        # Fonts
        "font.size": base,
        "axes.titlesize": base * 1.15,
        "axes.labelsize": base * 1.05,
        "xtick.labelsize": base * 0.95,
        "ytick.labelsize": base * 0.95,
        "legend.fontsize": base * 0.95,

        # Lines/markers (helps readability too)
        "lines.linewidth": 2.4,
        "lines.markersize": 7.0,
    })


@dataclass
class MetricsRow:
    """
    Container for one parsed run.

    Uses:
        dataclass for structured storage.

    Creates:
        scenario/protocol/vehicles + parsed metrics.

    Notes:
        Values can be NaN when not available in the log.
    """
    scenario: str
    protocol: str
    vehicles: int
    created: int
    delivered: int
    pdr: float
    lat_avg_ms: float
    overhead: float
    emg_t50: float
    emg_t90: float
    emg_t99: float


def safe_int(value: str) -> int:
    """
    Safely convert a string to int.

    Uses:
        int()

    Creates:
        An integer or 0 on failure.
    """
    try:
        return int(value)
    except Exception:
        return 0


def safe_float(value: str) -> float:
    """
    Safely convert a string to float.

    Uses:
        float()

    Creates:
        A float or NaN on failure.
    """
    try:
        return float(value)
    except Exception:
        return float("nan")


def normalize_filename(raw: str) -> str:
    """
    Normalize FILE field for robust parsing.

    Uses:
        - Replace escaped underscores (\\_) with (_)
        - Keep only the basename (drop directories)

    Creates:
        Normalized filename string.
    """
    s = raw.strip().replace("\\_", "_")
    return Path(s).name


def parse_batch_log(text: str) -> List[MetricsRow]:
    """
    Parse a batch log text into a list of MetricsRow entries.

    Uses:
        Regular expressions to extract FILE blocks and metric lines.

    Creates:
        A list of MetricsRow entries.

    Notes:
        The function ignores blocks whose filenames do not match the
        pattern '<Scenario>_<Protocol>_<N>_output.txt'.
    """
    block_re = re.compile(
        r"FILE:\s+(?P<file>.+?)\n"
        r"TIME:\s+.*?\n"
        r"CMD\s*:\s+.*?\n"
        r"-+\n"
        r"===\s+Final\s+Metrics\s+===\n"
        r"(?P<body>.*?)(?=\n-+\nFILE:|\n=+\s*\nBatch run finished:)",
        re.S,
    )

    # More tolerant filename matcher:
    #   Urban_BGKP_10_output.txt
    name_re = re.compile(
        r"(?P<scenario>[A-Za-z]+)_(?P<protocol>[A-Za-z0-9]+)_(?P<n>\d+)_output\.txt",
        re.I,
    )

    rows: List[MetricsRow] = []

    for m in block_re.finditer(text):
        raw_file = m.group("file")
        body = m.group("body")

        fname = normalize_filename(raw_file)
        nm = name_re.search(fname)
        if not nm:
            continue

        scenario = nm.group("scenario")
        protocol = nm.group("protocol").upper()
        vehicles = int(nm.group("n"))

        created = safe_int(
            re.search(r"Created:\s*(\d+)", body).group(1)
            if re.search(r"Created:\s*(\d+)", body) else "0"
        )

        delivered = safe_int(
            re.search(r"Delivered:\s*(\d+)", body).group(1)
            if re.search(r"Delivered:\s*(\d+)", body) else "0"
        )

        pdr = safe_float(
            re.search(r"PDR:\s*([0-9.]+)", body).group(1)
            if re.search(r"PDR:\s*([0-9.]+)", body) else "nan"
        )

        lat_avg_ms = safe_float(
            re.search(r"Latency\s+\(ms\):\s+avg=([0-9.]+)", body).group(1)
            if re.search(r"Latency\s+\(ms\):\s+avg=([0-9.]+)", body) else "nan"
        )

        overhead = safe_float(
            re.search(r"Overhead\s+\(TX/delivered\):\s*([0-9.]+)", body).group(1)
            if re.search(r"Overhead\s+\(TX/delivered\):\s*([0-9.]+)", body) else "nan"
        )

        emg = re.search(
            r"EMG latency\s+\(ms\):\s+t50=(\d+)\s+t90=(\d+)\s+t99=(\d+)",
            body,
        )
        if emg:
            emg_t50 = float(emg.group(1))
            emg_t90 = float(emg.group(2))
            emg_t99 = float(emg.group(3))
        else:
            emg_t50 = float("nan")
            emg_t90 = float("nan")
            emg_t99 = float("nan")

        rows.append(
            MetricsRow(
                scenario=scenario,
                protocol=protocol,
                vehicles=vehicles,
                created=created,
                delivered=delivered,
                pdr=pdr,
                lat_avg_ms=lat_avg_ms,
                overhead=overhead,
                emg_t50=emg_t50,
                emg_t90=emg_t90,
                emg_t99=emg_t99,
            )
        )

    return rows


def to_dataframe(rows: List[MetricsRow], default_scenario: str) -> pd.DataFrame:
    """
    Convert MetricsRow list to a DataFrame and ensure required columns exist.

    Uses:
        pandas.DataFrame

    Creates:
        A DataFrame with normalized column set and sorting applied.

    Notes:
        If parsing failed to produce 'scenario', the function injects
        default_scenario.
    """
    df = pd.DataFrame([r.__dict__ for r in rows])

    if df.empty:
        return df

    # Ensure the 'scenario' column exists.
    if "scenario" not in df.columns:
        df["scenario"] = default_scenario

    df["vehicles"] = df["vehicles"].astype(int)
    df = df.sort_values(["scenario", "protocol", "vehicles"]).reset_index(drop=True)
    return df


def plot_lines(
    df: pd.DataFrame,
    scenario: str,
    metric: str,
    title: str,
    ylabel: str,
    out_path: Path,
    protocols: Optional[List[str]] = None,
) -> None:
    """
    Plot metric vs vehicles as a line chart for each protocol.

    Uses:
        matplotlib

    Creates:
        A PNG plot saved into out_path.
    """
    sub = df[df["scenario"].str.lower() == scenario.lower()].copy()
    if protocols:
        keep = {p.upper() for p in protocols}
        sub = sub[sub["protocol"].isin(keep)]

    if sub.empty:
        return

    plt.figure(figsize=(9, 5))
    for prot in sorted(sub["protocol"].unique()):
        g = sub[sub["protocol"] == prot].sort_values("vehicles")
        x = g["vehicles"].to_numpy()
        y = g[metric].to_numpy(dtype=float)
        mask = np.isfinite(y)
        if mask.sum() == 0:
            continue
        plt.plot(x[mask], y[mask], marker="o", linewidth=2, label=prot)

    plt.title(title)
    plt.xlabel("Vehicles")
    plt.ylabel(ylabel)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)
    plt.close()

def plot_emg_9lines(
    df: pd.DataFrame,
    scenario: str,
    out_path: Path,
    protocols: Optional[List[str]] = None,
) -> None:
    """
    Plot EMG convergence (t50/t90/t99) on a single axes as 9 lines.

    Encoding:
        - Color family = metric:
            t50 -> Blues, t90 -> Reds, t99 -> Greens
        - Marker shape = protocol:
            BGKP -> circle, DUMB -> square, FLUD -> triangle
        - Shade inside a family = protocol index (consistent across metrics)

    Saves:
        A single PNG with 9 series (up to 9; fewer if some are missing).
    """
    sub = df[df["scenario"].str.lower() == scenario.lower()].copy()
    if protocols:
        keep = {p.upper() for p in protocols}
        sub = sub[sub["protocol"].isin(keep)]

    if sub.empty:
        return

    protocol_order = sorted(sub["protocol"].unique().tolist())

    # Marker mapping by protocol.
    marker_map = {
        "BGKP": "o",  # circle
        "DUMB": "s",  # square
        "FLUD": "^",  # triangle
    }

    # Metric definitions: (column, label_suffix, colormap_name)
    metrics = [
        ("emg_t50", "t50", "Blues"),
        ("emg_t90", "t90", "Reds"),
        ("emg_t99", "t99", "Greens"),
    ]

    # Choose distinct shades per protocol (same shade index used across metrics).
    # Keep shades mid-to-dark so they print well.
    shade_positions = np.linspace(0.45, 0.85, max(3, len(protocol_order)))

    def color_for(cmap_name: str, prot_idx: int):
        cmap = plt.get_cmap(cmap_name)
        pos = shade_positions[prot_idx] if prot_idx < len(shade_positions) else 0.7
        return cmap(pos)

    plt.figure(figsize=(10, 5.5))

    for m_col, m_suffix, cmap_name in metrics:
        for i, prot in enumerate(protocol_order):
            g = sub[sub["protocol"] == prot].sort_values("vehicles")
            x = g["vehicles"].to_numpy()
            y = g[m_col].to_numpy(dtype=float)

            mask = np.isfinite(y)
            if mask.sum() == 0:
                continue

            plt.plot(
                x[mask],
                y[mask],
                label=f"{prot} {m_suffix}",
                color=color_for(cmap_name, i),
                marker=marker_map.get(prot, "o"),
                markersize=6,
                linewidth=2,
            )

    plt.title(f"Emergency convergence vs vehicle count in {scenario} scenario")
    plt.xlabel("Vehicles")
    plt.ylabel("Time (ms)")
    plt.grid(True, alpha=0.3)

    # Put legend below to reduce clutter.
    plt.legend(
        loc="upper center",
        bbox_to_anchor=(0.5, -0.18),
        ncol=3,
        frameon=False,
        fontsize=plt.rcParams["legend.fontsize"],
    )

    plt.tight_layout()
    plt.savefig(out_path, dpi=220, bbox_inches="tight")
    plt.close()

def main() -> None:
    """
    CLI entry point.

    Uses:
        argparse

    Creates:
        - output directory
        - CSV summary
        - multiple PNG plots
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", default="metrics_batch_run.log")
    parser.add_argument("--scenario", default="Urban")
    parser.add_argument("--outdir", default="plots")
    parser.add_argument("--protocols", default="")
    parser.add_argument("--fontscale", type=float, default=1.6,
                    help="Scale all plot fonts/lines for readability.")
    args = parser.parse_args()

    log_path = Path(args.log)
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    text = log_path.read_text(encoding="utf-8", errors="ignore")
    rows = parse_batch_log(text)
    df = to_dataframe(rows, default_scenario=args.scenario)

    # Debug: show what we parsed.
    print(f"Parsed runs: {len(rows)}")
    print(f"DataFrame columns: {list(df.columns)}")

    if df.empty:
        raise SystemExit("No parsable runs found. Check FILE name pattern.")

    prot_list: Optional[List[str]] = None
    if args.protocols.strip():
        prot_list = [p.strip().upper() for p in args.protocols.split(",") if p.strip()]

    scenario_df = df[df["scenario"].str.lower() == args.scenario.lower()].copy()
    csv_path = outdir / f"{args.scenario.lower()}_metrics_summary.csv"
    scenario_df.to_csv(csv_path, index=False)
    
    apply_plot_style(args.fontscale)

    plot_lines(
        df, args.scenario, "pdr",
        f"{args.scenario}: Packet Delivery Ratio (PDR)",
        "PDR", outdir / f"{args.scenario.lower()}_pdr.png",
        protocols=prot_list,
    )

    plot_lines(
        df, args.scenario, "lat_avg_ms",
        f"{args.scenario}: Average Data Latency",
        "Latency (ms)", outdir / f"{args.scenario.lower()}_latency_avg.png",
        protocols=prot_list,
    )

    plot_lines(
        df, args.scenario, "overhead",
        f"{args.scenario}: Transmission Overhead (TX/Delivered)",
        "TX per delivered message", outdir / f"{args.scenario.lower()}_overhead.png",
        protocols=prot_list,
    )

    plot_lines(
        df, args.scenario, "emg_t50",
        f"{args.scenario}: Emergency Convergence (t50)",
        "t50 (ms)", outdir / f"{args.scenario.lower()}_emg_t50.png",
        protocols=prot_list,
    )

    plot_lines(
        df, args.scenario, "emg_t90",
        f"{args.scenario}: Emergency Convergence (t90)",
        "t90 (ms)", outdir / f"{args.scenario.lower()}_emg_t90.png",
        protocols=prot_list,
    )

    plot_lines(
        df, args.scenario, "emg_t99",
        f"{args.scenario}: Emergency Convergence (t99)",
        "t99 (ms)", outdir / f"{args.scenario.lower()}_emg_t99.png",
        protocols=prot_list,
    )
    
    plot_emg_9lines(
        df=df,
        scenario=args.scenario,
        out_path=outdir / f"{args.scenario.lower()}_emg_9lines.png",
        protocols=prot_list,
    )

    print(f"Saved CSV: {csv_path}")
    print(f"Saved plots to: {outdir.resolve()}")


if __name__ == "__main__":
    main()
