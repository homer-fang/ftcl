#!/usr/bin/env python3
import csv
import math
import os
import sys
from typing import List, Tuple


def read_csv_rows(path: str) -> List[dict]:
    with open(path, "r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def nice_tick_step(max_value: float, ticks: int = 5) -> float:
    if max_value <= 0:
        return 1.0
    raw = max_value / float(ticks)
    power = 10 ** math.floor(math.log10(raw))
    candidates = [1.0, 2.0, 2.5, 5.0, 10.0]
    scaled = raw / power
    for c in candidates:
        if scaled <= c:
            return c * power
    return 10.0 * power


def svg_header(w: int, h: int) -> str:
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}" role="img">\n'
        '<style>'
        'text{font-family:Arial,sans-serif;fill:#1f2937;}'
        '.title{font-size:22px;font-weight:700;}'
        '.axis{font-size:12px;}'
        '.small{font-size:11px;}'
        '</style>\n'
    )


def svg_footer() -> str:
    return "</svg>\n"


def draw_semantic_pass_rate(csv_path: str, out_path: str) -> None:
    rows = read_csv_rows(csv_path)
    suites = [r["suite"] for r in rows]
    rates = [float(r["pass_rate_pct"]) for r in rows]

    w, h = 1100, 620
    ml, mr, mt, mb = 90, 40, 80, 120
    pw = w - ml - mr
    ph = h - mt - mb
    n = max(1, len(suites))
    gap = 18
    bar_w = max(20, (pw - gap * (n - 1)) / n)

    y_max = 100.0
    y_step = 20.0

    s = [svg_header(w, h)]
    s.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    s.append(f'<text class="title" x="{ml}" y="42">Semantic Pass Rate by Suite (%)</text>\n')
    s.append(f'<text class="small" x="{ml}" y="62">Source: semantic_pass_rate.csv</text>\n')

    # grid + y ticks
    for i in range(int(y_max / y_step) + 1):
        y_val = i * y_step
        y = mt + ph - (y_val / y_max) * ph
        s.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{w - mr}" y2="{y:.2f}" stroke="#e5e7eb" stroke-width="1"/>\n')
        s.append(f'<text class="axis" x="{ml - 10}" y="{y + 4:.2f}" text-anchor="end">{int(y_val)}</text>\n')

    s.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="#374151" stroke-width="1.5"/>\n')
    s.append(
        f'<line x1="{ml}" y1="{mt + ph}" x2="{w - mr}" y2="{mt + ph}" stroke="#374151" stroke-width="1.5"/>\n'
    )

    for i, (name, rate) in enumerate(zip(suites, rates)):
        x = ml + i * (bar_w + gap)
        bh = (rate / y_max) * ph
        y = mt + ph - bh
        s.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{bar_w:.2f}" height="{bh:.2f}" fill="#2563eb"/>\n')
        s.append(f'<text class="axis" x="{x + bar_w/2:.2f}" y="{y - 6:.2f}" text-anchor="middle">{rate:.2f}</text>\n')
        s.append(
            f'<text class="axis" x="{x + bar_w/2:.2f}" y="{mt + ph + 18:.2f}" text-anchor="middle">{name}</text>\n'
        )

    s.append(svg_footer())
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("".join(s))


def histogram(values: List[float], bins: int) -> Tuple[List[int], List[float]]:
    if not values:
        return [0] * bins, [0.0] * (bins + 1)
    vmin = min(values)
    vmax = max(values)
    if vmax <= vmin:
        vmax = vmin + 1.0
    edges = [vmin + (vmax - vmin) * i / bins for i in range(bins + 1)]
    counts = [0] * bins
    for v in values:
        idx = int((v - vmin) / (vmax - vmin) * bins)
        if idx == bins:
            idx = bins - 1
        counts[idx] += 1
    return counts, edges


def parse_summary(summary_path: str) -> dict:
    rows = read_csv_rows(summary_path)
    return rows[0] if rows else {}


def draw_distribution(
    samples_csv: str,
    summary_csv: str,
    out_path: str,
    title: str,
    x_label: str,
    sample_key: str,
    color: str,
) -> None:
    rows = read_csv_rows(samples_csv)
    values = [float(r[sample_key]) for r in rows]
    summary = parse_summary(summary_csv)
    p50 = float(summary.get("p50_us", 0.0))
    p95 = float(summary.get("p95_us", 0.0))
    p99 = float(summary.get("p99_us", 0.0))

    bins = 40
    counts, edges = histogram(values, bins)
    max_count = max(counts) if counts else 1
    max_count = max(max_count, 1)

    vmin = edges[0] if edges else 0.0
    vmax = edges[-1] if edges else 1.0

    w, h = 1100, 620
    ml, mr, mt, mb = 90, 40, 80, 120
    pw = w - ml - mr
    ph = h - mt - mb

    def x_of(v: float) -> float:
        if vmax <= vmin:
            return ml
        return ml + (v - vmin) / (vmax - vmin) * pw

    def y_of(c: float) -> float:
        return mt + ph - (c / max_count) * ph

    s = [svg_header(w, h)]
    s.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    s.append(f'<text class="title" x="{ml}" y="42">{title}</text>\n')
    s.append(f'<text class="small" x="{ml}" y="62">Histogram with percentile markers (P50/P95/P99)</text>\n')

    # grid y
    y_step = nice_tick_step(float(max_count), 6)
    ticks = int(math.ceil(max_count / y_step))
    for i in range(ticks + 1):
        yv = i * y_step
        y = y_of(yv)
        s.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{w - mr}" y2="{y:.2f}" stroke="#e5e7eb" stroke-width="1"/>\n')
        s.append(f'<text class="axis" x="{ml - 10}" y="{y + 4:.2f}" text-anchor="end">{int(yv)}</text>\n')

    # axes
    s.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="#374151" stroke-width="1.5"/>\n')
    s.append(
        f'<line x1="{ml}" y1="{mt + ph}" x2="{w - mr}" y2="{mt + ph}" stroke="#374151" stroke-width="1.5"/>\n'
    )

    # bars
    for i, c in enumerate(counts):
        x0 = x_of(edges[i])
        x1 = x_of(edges[i + 1])
        y = y_of(c)
        s.append(
            f'<rect x="{x0:.2f}" y="{y:.2f}" width="{max(1.0, x1 - x0 - 0.6):.2f}" height="{mt + ph - y:.2f}" fill="{color}" opacity="0.75"/>\n'
        )

    # x ticks
    x_step = nice_tick_step(vmax - vmin, 8)
    if x_step <= 0:
        x_step = 1.0
    x_tick = math.floor(vmin / x_step) * x_step
    while x_tick <= vmax + 1e-9:
        x = x_of(x_tick)
        s.append(f'<line x1="{x:.2f}" y1="{mt + ph}" x2="{x:.2f}" y2="{mt + ph + 6}" stroke="#374151"/>\n')
        s.append(f'<text class="axis" x="{x:.2f}" y="{mt + ph + 22}" text-anchor="middle">{x_tick:.1f}</text>\n')
        x_tick += x_step

    # percentile lines
    for val, label, clr in [
        (p50, "P50", "#16a34a"),
        (p95, "P95", "#f59e0b"),
        (p99, "P99", "#dc2626"),
    ]:
        x = x_of(val)
        s.append(f'<line x1="{x:.2f}" y1="{mt}" x2="{x:.2f}" y2="{mt + ph}" stroke="{clr}" stroke-width="2"/>\n')
        s.append(
            f'<text class="axis" x="{x + 4:.2f}" y="{mt + 14:.2f}" text-anchor="start" fill="{clr}">{label} {val:.2f} us</text>\n'
        )

    s.append(f'<text class="axis" x="{ml + pw / 2:.2f}" y="{h - 30}" text-anchor="middle">{x_label}</text>\n')
    s.append(f'<text class="axis" x="26" y="{mt + ph / 2:.2f}" transform="rotate(-90 26 {mt + ph / 2:.2f})">Count</text>\n')
    s.append(svg_footer())

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("".join(s))


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: plot_benchmarks.py <benchmark_data_dir> <figures_out_dir>")
        return 2

    data_dir = sys.argv[1]
    fig_dir = sys.argv[2]
    ensure_dir(fig_dir)

    draw_semantic_pass_rate(
        os.path.join(data_dir, "semantic_pass_rate.csv"),
        os.path.join(fig_dir, "semantic_pass_rate.svg"),
    )
    draw_distribution(
        os.path.join(data_dir, "channel_latency_us.csv"),
        os.path.join(data_dir, "channel_latency_summary.csv"),
        os.path.join(fig_dir, "channel_latency_distribution.svg"),
        "Thread Channel One-Way Latency Distribution",
        "Latency (microseconds, one-way)",
        "one_way_latency_us",
        "#0ea5e9",
    )
    draw_distribution(
        os.path.join(data_dir, "frame_time_us.csv"),
        os.path.join(data_dir, "frame_time_summary.csv"),
        os.path.join(fig_dir, "frame_time_distribution.svg"),
        "Frame Time Distribution",
        "Frame time (microseconds)",
        "frame_time_us",
        "#7c3aed",
    )

    print(f"Generated SVG figures in: {fig_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

