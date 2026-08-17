#!/usr/bin/env python3
"""metrics_parser.py

Parse Cooja Mote Output logs and compute final metrics.

Supports unified METR format (recommended):
  METR MBL CREATE t=<ms> o=<origin> m=<msg>
  METR RSU RX     t=<ms> o=<origin> m=<msg>
  METR MBL TX     t=<ms> win=<sec> id=<node> tot=<n> d=<n> a=<n> q=<n> e=<n>
  METR MBL BUF    t=<ms> win=<sec> id=<node> last=<n> max=<n> put=<n> rm=<n> ev=<n>
  METR RSU TX     t=<ms> win=<sec> id=<rsu>  tot=<n> a=<n> e=<n>
  METR EMG CREATE t=<ms> o=<origin> m=<msg> a=<ef_code>
  METR EMG RX     t=<ms> o=<origin> m=<msg> a=<ef_code>


Important:
  Cooja often prefixes each line with a timestamp and a mote ID, e.g.:
    00:10.723\tID:1\tMETR MBL CREATE t=... o=... m=...
  This parser strips everything before the first 'METR ' token.

Outputs:
  - Human-readable summary
  - Optional JSON for further processing
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
import sys
from dataclasses import dataclass, field
from typing import Dict, Iterator, List, Optional, Tuple


## ---------------------------------------------------------------------------
## Config
## ---------------------------------------------------------------------------

# Cutoff window for emergency dissemination (ms)
EMG_CUTOFF_MS = 5000

# ---------------------------------------------------------------------------
# Data containers
# ---------------------------------------------------------------------------


@dataclass
class TxAgg:
    """Windowed TX counters.

    What:
        Holds transmission counters aggregated across all windows.
    Methods:
        Integer accumulation.
    Creates:
        tx_tot, tx_d, tx_a, tx_q, tx_e.
    """

    tx_tot: int = 0
    tx_d: int = 0
    tx_a: int = 0
    tx_q: int = 0
    tx_e: int = 0


@dataclass
class BufAgg:
    """Windowed buffer counters.

    What:
        Holds buffer occupancy and event counters (PUT/RM/EVICT).
    Methods:
        Track average last occupancy and max seen.
    Creates:
        last_sum, last_n, max_max, put, rm, ev.
    """

    last_sum: int = 0
    last_n: int = 0
    max_max: int = 0
    put: int = 0
    rm: int = 0
    ev: int = 0


@dataclass
class ParseState:
    """Accumulated parse state.

    What:
        Stores create and receive timestamps and aggregates counters.
    Methods:
        Dict maps for timestamps, integer sums for counters.
    Creates:
        created_ts, delivered_ts, mbl_tx, rsu_tx, buf.
    """

    created_ts: Dict[Tuple[int, int], int]
    delivered_ts: Dict[Tuple[int, int], int]

    mbl_tx: TxAgg
    rsu_tx: TxAgg
    buf: BufAgg

    # Emergency tracking
    emg_create_time: Dict[Tuple[int, int], int] = field(default_factory=dict)
    emg_rx_times: Dict[Tuple[int, int], List[Tuple[int, int]]] = field(default_factory=dict)
    emg_create: int = 0
    emg_rx:     int = 0
    emg_fwd:    int = 0

    # First EMG RX per node: origin_id -> first_rx_time
    emg_first_rx: Dict[int, int] = None

    lines_total: int = 0
    lines_parsed: int = 0


# ---------------------------------------------------------------------------
# Regex helpers
# ---------------------------------------------------------------------------


_METR_RE = re.compile(r"^METR\s+(MBL|RSU|EMG)\s+(CREATE|RX|TX|BUF|FWD)\s+(.*)$")
_KV_RE = re.compile(r"([a-zA-Z_]+)=([^\s]+)")


# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------


def parse_kv_blob(blob: str) -> Dict[str, str]:
    """Parse key=value tokens into a dict."""

    out: Dict[str, str] = {}
    for m in _KV_RE.finditer(blob):
        out[m.group(1)] = m.group(2)
    return out


def safe_int(d: Dict[str, str], key: str, default: int = 0) -> int:
    """Safely convert a dict value to int."""

    try:
        return int(d.get(key, default))
    except Exception:
        return default


def strip_to_metr(line: str) -> str:
    """Strip any Cooja prefix before the first 'METR ' token.

    What:
        Normalizes lines from Cooja Mote Output files which often include
        a timestamp and mote ID columns.
    Methods:
        Find and slice from substring index.
    Creates:
        A line that starts with 'METR ' if the token existed.
    """

    if not line:
        return line
    if line.startswith("METR "):
        return line
    i = line.find("METR ")
    if i >= 0:
        return line[i:]
    return line


def ingest_metr_line(state: ParseState, role: str, kind: str,
                     kv: Dict[str, str]) -> None:
    """Ingest one METR line into state."""

    if role == "MBL" and kind == "CREATE":
        t = safe_int(kv, "t")
        o = safe_int(kv, "o")
        m = safe_int(kv, "m")
        key = (o, m)
        if key not in state.created_ts or t < state.created_ts[key]:
            state.created_ts[key] = t
        return

    if role == "RSU" and kind == "RX":
        t = safe_int(kv, "t")
        o = safe_int(kv, "o")
        m = safe_int(kv, "m")
        key = (o, m)
        if key not in state.delivered_ts or t < state.delivered_ts[key]:
            state.delivered_ts[key] = t
        return

    if role == "EMG" and kind == "CREATE":
        t = safe_int(kv, "t")
        o = safe_int(kv, "o")
        m = safe_int(kv, "m")
        key = (o, m)

        state.emg_create += 1

        if key not in state.emg_create_time:
            state.emg_create_time[key] = t

        if key not in state.emg_rx_times:
            state.emg_rx_times[key] = []

        return

    if role == "EMG" and kind == "RX":
        t = safe_int(kv, "t")
        o = safe_int(kv, "o")
        m = safe_int(kv, "m")
        r = safe_int(kv, "r")
        key = (o, m)

        state.emg_rx += 1

        if key not in state.emg_rx_times:
            state.emg_rx_times[key] = []

        state.emg_rx_times[key].append((t, r))

        return

    if role == "EMG" and kind == "FWD":
        t = safe_int(kv, "t")
        o = safe_int(kv, "o")
        m = safe_int(kv, "m")
        state.emg_fwd += 1
        return

    if kind == "TX":
        tot = safe_int(kv, "tot")
        d = safe_int(kv, "d")
        a = safe_int(kv, "a")
        q = safe_int(kv, "q")
        e = safe_int(kv, "e")

        if role == "MBL":
            state.mbl_tx.tx_tot += tot
            state.mbl_tx.tx_d += d
            state.mbl_tx.tx_a += a
            state.mbl_tx.tx_q += q
            state.mbl_tx.tx_e += e
        else:
            state.rsu_tx.tx_tot += tot
            state.rsu_tx.tx_a += a
            state.rsu_tx.tx_e += e
        return

    if role == "MBL" and kind == "BUF":
        last = safe_int(kv, "last")
        mx = safe_int(kv, "max")
        put = safe_int(kv, "put")
        rm = safe_int(kv, "rm")
        ev = safe_int(kv, "ev")

        state.buf.last_sum += last
        state.buf.last_n += 1
        if mx > state.buf.max_max:
            state.buf.max_max = mx
        state.buf.put += put
        state.buf.rm += rm
        state.buf.ev += ev
        return


def iter_lines(fp) -> Iterator[str]:
    """Iterate over input lines."""

    for raw in fp:
        yield raw.rstrip("\n")


def parse_log_stream(fp) -> ParseState:
    """Parse full stream."""

    state = ParseState(
        created_ts={},
        delivered_ts={},
        mbl_tx=TxAgg(),
        rsu_tx=TxAgg(),
        buf=BufAgg(),
        emg_first_rx={},
        emg_create_time={},
        emg_rx_times={},

    )

    for line in iter_lines(fp):
        state.lines_total += 1
        if not line:
            continue

        line = strip_to_metr(line)

        mm = _METR_RE.match(line)
        if not mm:
            continue

        role, kind, tail = mm.group(1), mm.group(2), mm.group(3)
        kv = parse_kv_blob(tail)
        ingest_metr_line(state, role, kind, kv)
        state.lines_parsed += 1

    return state


# ---------------------------------------------------------------------------
# Metrics computation
# ---------------------------------------------------------------------------


def percentile(values: List[int], p: float) -> Optional[float]:
    """Percentile with interpolation."""

    if not values:
        return None
    if p <= 0:
        return float(min(values))
    if p >= 100:
        return float(max(values))

    xs = sorted(values)
    k = (len(xs) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(xs) - 1)
    if f == c:
        return float(xs[f])
    d0 = xs[f] * (c - k)
    d1 = xs[c] * (k - f)
    return float(d0 + d1)

def group_emg_events(state: ParseState):
    """
    Group EMG messages into logical events by origin and time window.
    """

    events = {}  # origin -> list of events

    for (o, m), t_create in state.emg_create_time.items():
        if o not in events:
            events[o] = []

        placed = False

        for ev in events[o]:
            # if within window → same event
            if abs(t_create - ev["t0"]) <= EMG_CUTOFF_MS:
                ev["msgs"].append((m, t_create))
                placed = True
                break

        if not placed:
            events[o].append({
                "t0": t_create,
                "msgs": [(m, t_create)]
            })

    return events

def compute_final_metrics(state: ParseState) -> Dict[str, object]:
    """Compute final summary metrics."""

    created_n = len(state.created_ts)

    # ------------------------------------------------------------------
    # Emergency dissemination metrics (coverage-based)
    # ------------------------------------------------------------------

    emg_latencies = []
    emg_t50 = []
    emg_t90 = []

    lat_ms: List[int] = []
    matched = 0
    for key, t_del in state.delivered_ts.items():
        t_cre = state.created_ts.get(key)
        if t_cre is None:
            continue

        dt = t_del - t_cre
        if dt >= 0:
            lat_ms.append(dt)
            matched += 1
    events = group_emg_events(state)

    for origin, ev_list in events.items():
        for ev in ev_list:
            delays = []
            best_per_node = {}

            for (m, t_cre_msg) in ev["msgs"]:
                rx_list = state.emg_rx_times.get((origin, m), [])
                for (t_rx, rcv) in rx_list:
                    dt = t_rx - t_cre_msg
                    if 0 <= dt <= EMG_CUTOFF_MS:
                        if (rcv not in best_per_node) or (dt < best_per_node[rcv]):
                            best_per_node[rcv] = dt

            delays = list(best_per_node.values())

            if not delays:
                continue

            delays.sort()

            n = len(delays)
            emg_latencies.extend(delays)

            idx50 = int(0.5 * n)
            idx90 = int(0.9 * n)

            emg_t50.append(delays[min(idx50, n-1)])
            emg_t90.append(delays[min(idx90, n-1)])

    pdr = (matched / created_n) if created_n else 0.0

    tx_total = state.mbl_tx.tx_tot + state.rsu_tx.tx_tot
    overhead = (tx_total / matched) if matched else 0.0

    lat_stats = {
        "count": len(lat_ms),
        "avg_ms": float(statistics.mean(lat_ms)) if lat_ms else None,
        "min_ms": float(min(lat_ms)) if lat_ms else None,
        "max_ms": float(max(lat_ms)) if lat_ms else None,
        "p50_ms": percentile(lat_ms, 50.0),
        "p90_ms": percentile(lat_ms, 90.0),
        "p95_ms": percentile(lat_ms, 95.0),
    }

    buf_avg_last = (state.buf.last_sum / state.buf.last_n) if state.buf.last_n else 0.0

    emg_latencies.sort()
    # ------------------------------------------------------------------
    # EMG forwarding metrics
    # ------------------------------------------------------------------

    emg_efficiency = None
    if state.emg_fwd > 0:
        emg_efficiency = state.emg_rx / state.emg_fwd

    def cov_percentile(values: List[int], p: float) -> Optional[int]:
        if not values:
            return None
        k = int(len(values) * p)
        if k >= len(values):
            k = len(values) - 1
        return values[k]

    t50 = cov_percentile(emg_latencies, 0.5)
    t90 = cov_percentile(emg_latencies, 0.9)
    t95 = cov_percentile(emg_latencies, 0.95)

    return {
        "emg_latency": {
            "count": len(emg_latencies),
            "t50_ms": t50,
            "t90_ms": t90,
            "t95_ms": t95
        },
        "emergency": {
            "created": state.emg_create,
            "rx": state.emg_rx,
            "fwd": state.emg_fwd,
            "efficiency_rx_per_fwd": emg_efficiency

        },
        "created_messages": created_n,
        "delivered_messages": matched,
        "delivery_ratio": pdr,
        "latency_ms": lat_stats,
        "tx": {
            "mobile": {
                "tot": state.mbl_tx.tx_tot,
                "data": state.mbl_tx.tx_d,
                "ack": state.mbl_tx.tx_a,
                "query": state.mbl_tx.tx_q,
                "emg": state.mbl_tx.tx_e,
            },
            "rsu": {
                "tot": state.rsu_tx.tx_tot,
                "ack": state.rsu_tx.tx_a,
                "emg": state.rsu_tx.tx_e,
            },
            "total": tx_total,
            "overhead_tx_per_delivered": overhead,
        },
        "buffer": {
            "avg_last": float(buf_avg_last),
            "max_seen": int(state.buf.max_max),
            "put": int(state.buf.put),
            "rm": int(state.buf.rm),
            "ev": int(state.buf.ev),
        },
        "parser": {
            "lines_total": state.lines_total,
            "lines_parsed": state.lines_parsed,
        },
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_arg_parser() -> argparse.ArgumentParser:
    """Argument parser."""

    ap = argparse.ArgumentParser(description="Parse METR logs and compute metrics")
    ap.add_argument("logfile", nargs="?", default="-",
                    help="Path to log file, or '-' for stdin")
    ap.add_argument("--json", dest="json_out", default=None,
                    help="Write metrics JSON to this file")
    ap.add_argument("--quiet", action="store_true",
                    help="Do not print summary")
    return ap


def print_summary(metrics: Dict[str, object]) -> None:
    """Human-readable summary."""

    lat = metrics["latency_ms"]
    tx = metrics["tx"]
    buf = metrics["buffer"]

    print("=== Final Metrics ===")
    print(f"Created:   {metrics['created_messages']}")
    print(f"Delivered: {metrics['delivered_messages']}")
    print(f"PDR:       {metrics['delivery_ratio']:.4f}")

    emg = metrics.get('emergency', {})
    print(f"EMG:       create={emg.get('created', 0)} rx={emg.get('rx', 0)}")

    emg_lat = metrics.get("emg_latency", {})
    if emg_lat.get("count", 0) > 0:
        print(
            "EMG latency (ms): "
            f"t50={emg_lat['t50_ms']} "
            f"t90={emg_lat['t90_ms']} "
            f"t95={emg_lat['t95_ms']}"
        )
    else:
        print("EMG latency: N/A")

    if lat["avg_ms"] is None:
        print("Latency:   N/A (no matched deliveries)")
    else:
        print(
            "Latency (ms): "
            f"avg={lat['avg_ms']:.1f} "
            f"p50={lat['p50_ms']:.1f} "
            f"p90={lat['p90_ms']:.1f} "
            f"min={lat['min_ms']:.0f} "
            f"max={lat['max_ms']:.0f}"
        )

    print(
        "TX total: "
        f"mbl={tx['mobile']['tot']} "
        f"rsu={tx['rsu']['tot']} "
        f"all={tx['total']}"
    )
    print(f"Overhead (TX/delivered): {tx['overhead_tx_per_delivered']:.2f}")

    print(
        "TX breakdown (MBL): "
        f"d={tx['mobile']['data']} "
        f"a={tx['mobile']['ack']} "
        f"q={tx['mobile']['query']} "
        f"e={tx['mobile']['emg']}"
    )
    print(
        "TX breakdown (RSU): "
        f"a={tx['rsu']['ack']} "
        f"e={tx['rsu']['emg']}"
    )

    print(
        "BUF: "
        f"avg_last={buf['avg_last']:.2f} "
        f"max_seen={buf['max_seen']} "
        f"put={buf['put']} rm={buf['rm']} ev={buf['ev']}"
    )


def main(argv: Optional[List[str]] = None) -> int:
    """Main."""

    ap = build_arg_parser()
    args = ap.parse_args(argv)

    if args.logfile == "-":
        state = parse_log_stream(sys.stdin)
    else:
        with open(args.logfile, "r", encoding="utf-8", errors="replace") as fp:
            state = parse_log_stream(fp)

    metrics = compute_final_metrics(state)

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fp:
            json.dump(metrics, fp, indent=2)

    if not args.quiet:
        print_summary(metrics)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
