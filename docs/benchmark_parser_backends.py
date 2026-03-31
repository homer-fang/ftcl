#!/usr/bin/env python3
import argparse
import csv
import math
import os
import random
import statistics
import subprocess
import time
from datetime import datetime, timezone
from typing import Dict, List


def percentile(values: List[float], p: float) -> float:
    if not values:
        return 0.0
    xs = sorted(values)
    if len(xs) == 1:
        return xs[0]
    rank = (p / 100.0) * (len(xs) - 1)
    lo = int(math.floor(rank))
    hi = int(math.ceil(rank))
    t = rank - lo
    return xs[lo] * (1.0 - t) + xs[hi] * t


def run_once(binary: str, tests_dir: str, backend: str) -> float:
    env = os.environ.copy()
    env["FTCL_PARSER_BACKEND"] = backend

    t0 = time.perf_counter()
    proc = subprocess.run(
        [binary, tests_dir],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
        check=False,
    )
    elapsed_ms = (time.perf_counter() - t0) * 1000.0
    if proc.returncode != 0:
        tail = (proc.stdout + "\n" + proc.stderr)[-3000:]
        raise RuntimeError(
            f"benchmark failed for backend={backend}, returncode={proc.returncode}\n{tail}"
        )
    return elapsed_ms


def summarize(samples: List[float]) -> Dict[str, float]:
    n = len(samples)
    mean = statistics.fmean(samples) if samples else 0.0
    std = statistics.stdev(samples) if n >= 2 else 0.0
    half_ci = 1.96 * std / math.sqrt(n) if n >= 2 else 0.0
    return {
        "n": float(n),
        "min_ms": min(samples) if samples else 0.0,
        "max_ms": max(samples) if samples else 0.0,
        "mean_ms": mean,
        "median_ms": statistics.median(samples) if samples else 0.0,
        "p95_ms": percentile(samples, 95.0),
        "std_ms": std,
        "ci95_low_ms": mean - half_ci,
        "ci95_high_ms": mean + half_ci,
    }


def ensure_parent(path: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)


def write_raw_csv(path: str, rows: List[Dict[str, str]]) -> None:
    ensure_parent(path)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["sample_id", "backend", "elapsed_ms"])
        writer.writeheader()
        writer.writerows(rows)


def write_summary_csv(path: str, rows: List[Dict[str, str]]) -> None:
    ensure_parent(path)
    fieldnames = [
        "backend",
        "n",
        "min_ms",
        "max_ms",
        "mean_ms",
        "median_ms",
        "p95_ms",
        "std_ms",
        "ci95_low_ms",
        "ci95_high_ms",
    ]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def svg_header(w: int, h: int) -> str:
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}" role="img">\n'
        "<style>"
        "text{font-family:Arial,sans-serif;fill:#111827;}"
        ".title{font-size:24px;font-weight:700;}"
        ".subtitle{font-size:13px;fill:#4b5563;}"
        ".axis{font-size:12px;}"
        ".tick{font-size:11px;fill:#374151;}"
        "</style>\n"
    )


def draw_figure(path: str, raw_rows: List[Dict[str, str]], summary: Dict[str, Dict[str, float]], rounds: int) -> None:
    ensure_parent(path)

    backends = ["legacy", "token_stream"]
    colors = {
        "legacy": "#2563eb",
        "token_stream": "#f59e0b",
    }

    raw_by_backend: Dict[str, List[float]] = {b: [] for b in backends}
    for r in raw_rows:
        raw_by_backend[r["backend"]].append(float(r["elapsed_ms"]))

    ymax = 0.0
    for b in backends:
        ymax = max(ymax, max(raw_by_backend[b], default=0.0))
        ymax = max(ymax, summary[b]["ci95_high_ms"])
    ymax *= 1.18
    if ymax <= 0:
        ymax = 1.0

    w, h = 1100, 680
    ml, mr, mt, mb = 100, 40, 90, 110
    pw, ph = w - ml - mr, h - mt - mb

    def y_of(v: float) -> float:
        return mt + ph - (v / ymax) * ph

    x_centers = {
        "legacy": ml + pw * 0.32,
        "token_stream": ml + pw * 0.68,
    }
    bar_width = 180.0

    parts = [svg_header(w, h)]
    parts.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    parts.append('<text class="title" x="100" y="44">Parser Backend Runtime Comparison</text>\n')
    parts.append(
        f'<text class="subtitle" x="100" y="66">Workload: test_ftcl_subset; rounds per backend: {rounds}; lower is better</text>\n'
    )

    # Grid and y-axis ticks.
    tick_count = 8
    for i in range(tick_count + 1):
        v = ymax * i / tick_count
        y = y_of(v)
        parts.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{w - mr}" y2="{y:.2f}" stroke="#e5e7eb" stroke-width="1"/>\n')
        parts.append(f'<text class="tick" x="{ml - 12}" y="{y + 4:.2f}" text-anchor="end">{v:.0f}</text>\n')

    parts.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')
    parts.append(
        f'<line x1="{ml}" y1="{mt + ph}" x2="{w - mr}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n'
    )
    parts.append(
        f'<text class="axis" x="26" y="{mt + ph/2:.2f}" transform="rotate(-90 26 {mt + ph/2:.2f})">Elapsed time (ms)</text>\n'
    )

    rnd = random.Random(20260331)

    for backend in backends:
        cx = x_centers[backend]
        mean = summary[backend]["mean_ms"]
        lo = summary[backend]["ci95_low_ms"]
        hi = summary[backend]["ci95_high_ms"]

        y_bar = y_of(mean)
        bar_h = mt + ph - y_bar
        parts.append(
            f'<rect x="{cx - bar_width/2:.2f}" y="{y_bar:.2f}" width="{bar_width:.2f}" height="{bar_h:.2f}" '
            f'fill="{colors[backend]}" opacity="0.86"/>\n'
        )

        # 95% CI error bar.
        y_lo = y_of(lo)
        y_hi = y_of(hi)
        parts.append(
            f'<line x1="{cx:.2f}" y1="{y_lo:.2f}" x2="{cx:.2f}" y2="{y_hi:.2f}" stroke="#111827" stroke-width="2"/>\n'
        )
        parts.append(
            f'<line x1="{cx - 12:.2f}" y1="{y_lo:.2f}" x2="{cx + 12:.2f}" y2="{y_lo:.2f}" stroke="#111827" stroke-width="2"/>\n'
        )
        parts.append(
            f'<line x1="{cx - 12:.2f}" y1="{y_hi:.2f}" x2="{cx + 12:.2f}" y2="{y_hi:.2f}" stroke="#111827" stroke-width="2"/>\n'
        )

        # Raw samples as jittered points.
        for v in raw_by_backend[backend]:
            x = cx + rnd.uniform(-bar_width * 0.30, bar_width * 0.30)
            y = y_of(v)
            parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3.2" fill="#0f172a" opacity="0.45"/>\n')

        parts.append(
            f'<text class="axis" x="{cx:.2f}" y="{mt + ph + 28:.2f}" text-anchor="middle">{backend}</text>\n'
        )
        parts.append(
            f'<text class="tick" x="{cx:.2f}" y="{y_bar - 8:.2f}" text-anchor="middle">mean={mean:.2f} ms</text>\n'
        )

    # Legend
    lx, ly = w - 320, mt + 12
    parts.append(f'<rect x="{lx}" y="{ly}" width="16" height="12" fill="#2563eb" opacity="0.86"/>\n')
    parts.append(f'<text class="tick" x="{lx + 24}" y="{ly + 11}">legacy</text>\n')
    parts.append(f'<rect x="{lx}" y="{ly + 20}" width="16" height="12" fill="#f59e0b" opacity="0.86"/>\n')
    parts.append(f'<text class="tick" x="{lx + 24}" y="{ly + 31}">token_stream</text>\n')
    parts.append(f'<text class="tick" x="{lx}" y="{ly + 52}">error bar: 95% CI</text>\n')
    parts.append(f'<text class="tick" x="{lx}" y="{ly + 69}">dot: one trial</text>\n')

    parts.append("</svg>\n")

    with open(path, "w", encoding="utf-8") as f:
        f.write("".join(parts))


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark legacy vs token_stream parser runtime.")
    parser.add_argument("--binary", default="./build/test/test_ftcl_subset")
    parser.add_argument("--tests-dir", default="./test/tests")
    parser.add_argument("--rounds", type=int, default=20)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--raw-csv", default="./docs/benchmark_data/parser_backend_timing_raw.csv")
    parser.add_argument("--summary-csv", default="./docs/benchmark_data/parser_backend_timing_summary.csv")
    parser.add_argument("--figure", default="./docs/figures/parser_backend_timing.svg")
    args = parser.parse_args()

    backends = ["legacy", "token_stream"]

    for _ in range(args.warmup):
        for b in backends:
            _ = run_once(args.binary, args.tests_dir, b)

    raw_rows: List[Dict[str, str]] = []
    samples: Dict[str, List[float]] = {b: [] for b in backends}
    sample_id = 0

    for i in range(args.rounds):
        order = backends if i % 2 == 0 else list(reversed(backends))
        for b in order:
            ms = run_once(args.binary, args.tests_dir, b)
            samples[b].append(ms)
            raw_rows.append(
                {
                    "sample_id": str(sample_id),
                    "backend": b,
                    "elapsed_ms": f"{ms:.6f}",
                }
            )
            sample_id += 1

    summary: Dict[str, Dict[str, float]] = {b: summarize(samples[b]) for b in backends}
    summary_rows: List[Dict[str, str]] = []
    for b in backends:
        s = summary[b]
        summary_rows.append(
            {
                "backend": b,
                "n": f"{int(s['n'])}",
                "min_ms": f"{s['min_ms']:.6f}",
                "max_ms": f"{s['max_ms']:.6f}",
                "mean_ms": f"{s['mean_ms']:.6f}",
                "median_ms": f"{s['median_ms']:.6f}",
                "p95_ms": f"{s['p95_ms']:.6f}",
                "std_ms": f"{s['std_ms']:.6f}",
                "ci95_low_ms": f"{s['ci95_low_ms']:.6f}",
                "ci95_high_ms": f"{s['ci95_high_ms']:.6f}",
            }
        )

    write_raw_csv(args.raw_csv, raw_rows)
    write_summary_csv(args.summary_csv, summary_rows)
    draw_figure(args.figure, raw_rows, summary, args.rounds)

    print("Parser backend timing benchmark complete.")
    print(f"raw_csv={args.raw_csv}")
    print(f"summary_csv={args.summary_csv}")
    print(f"figure={args.figure}")
    print(f"timestamp_utc={datetime.now(timezone.utc).isoformat()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

