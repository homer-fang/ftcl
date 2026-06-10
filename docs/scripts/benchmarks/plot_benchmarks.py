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
        "text{font-family:'AR PL UMing TW MBE','AR PL UMing CN','FandolHei','Microsoft YaHei','SimHei',Arial,sans-serif;fill:#1f2937;}"
        '.title{font-size:38px;font-weight:700;}'
        '.axis{font-size:20px;}'
        '.small{font-size:18px;}'
        '</style>\n'
    )


def svg_footer() -> str:
    return "</svg>\n"


def draw_semantic_pass_rate(csv_path: str, out_path: str) -> None:
    rows = read_csv_rows(csv_path)
    suites = [r["suite"] for r in rows]
    rates = [float(r["pass_rate_pct"]) for r in rows]

    w, h = 1100, 620
    ml, mr, mt, mb = 120, 70, 125, 150
    pw = w - ml - mr
    ph = h - mt - mb
    n = max(1, len(suites))
    gap = 18
    bar_w = max(20, (pw - gap * (n - 1)) / n)

    y_max = 100.0
    y_step = 20.0

    s = [svg_header(w, h)]
    s.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    s.append(f'<text class="title" x="{ml}" y="56">语义通过率（semantic pass rate）</text>\n')
    s.append(f'<text class="small" x="{ml}" y="94">数据来源：semantic_pass_rate.csv</text>\n')

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
        s.append(f'<text class="axis" x="{x + bar_w/2:.2f}" y="{y - 12:.2f}" text-anchor="middle">{rate:.2f}</text>\n')
        s.append(
            f'<text class="axis" x="{x + bar_w/2:.2f}" y="{mt + ph + 34:.2f}" text-anchor="middle">{name}</text>\n'
        )

    s.append(svg_footer())
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("".join(s))


def histogram(values: List[float], bins: int, vmin: float = None, vmax: float = None) -> Tuple[List[int], List[float]]:
    if not values:
        return [0] * bins, [0.0] * (bins + 1)
    vmin = min(values) if vmin is None else vmin
    vmax = max(values) if vmax is None else vmax
    if vmax <= vmin:
        vmax = vmin + 1.0
    edges = [vmin + (vmax - vmin) * i / bins for i in range(bins + 1)]
    counts = [0] * bins
    for v in values:
        if v < vmin or v > vmax:
            continue
        idx = int((v - vmin) / (vmax - vmin) * bins)
        if idx == bins:
            idx = bins - 1
        counts[idx] += 1
    return counts, edges


def parse_summary(summary_path: str) -> dict:
    rows = read_csv_rows(summary_path)
    return rows[0] if rows else {}


def nice_axis_ceiling(value: float, ticks: int = 8) -> float:
    step = nice_tick_step(value, ticks)
    return math.ceil(value / step) * step


def focused_histogram_range(values: List[float], p95: float, p99: float) -> Tuple[float, float, int]:
    if not values:
        return 0.0, 1.0, 0

    raw_max = max(values)
    focus_target = max(p99 * 1.6, p95 * 1.25, 1.0)

    if raw_max > focus_target * 1.35:
        vmax = nice_axis_ceiling(focus_target, 8)
    else:
        vmax = nice_axis_ceiling(raw_max, 8)

    vmax = max(vmax, 1.0)
    clipped = sum(1 for v in values if v > vmax)
    return 0.0, vmax, clipped


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
    vmin, vmax, clipped = focused_histogram_range(values, p95, p99)
    counts, edges = histogram(values, bins, vmin, vmax)
    max_count = max(counts) if counts else 1
    max_count = max(max_count, 1)

    w, h = 1100, 620
    ml, mr, mt, mb = 120, 70, 125, 150
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
    s.append(f'<text class="title" x="{ml}" y="56">{title}</text>\n')
    subtitle = "直方图包含分位数标记（P50/P95/P99）"
    if clipped:
        subtitle += f"；横轴聚焦主体分布，尾部截断样本数：{clipped}"
    s.append(f'<text class="small" x="{ml}" y="94">{subtitle}</text>\n')

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
        s.append(f'<text class="axis" x="{x:.2f}" y="{mt + ph + 38}" text-anchor="middle">{x_tick:.1f}</text>\n')
        x_tick += x_step

    # percentile lines.  Keep the markers on the plot, but put the text in a
    # small stacked legend so close percentile values do not overlap.
    percentile_marks = [
        (p50, "P50", "#16a34a"),
        (p95, "P95", "#f59e0b"),
        (p99, "P99", "#dc2626"),
    ]
    for val, _label, clr in percentile_marks:
        if val > vmax:
            continue
        x = x_of(val)
        s.append(f'<line x1="{x:.2f}" y1="{mt}" x2="{x:.2f}" y2="{mt + ph}" stroke="{clr}" stroke-width="2"/>\n')

    legend_x = w - mr - 315
    legend_y = mt + 30
    legend_w = 300
    legend_h = 100
    s.append(
        f'<rect x="{legend_x - 18:.2f}" y="{legend_y - 34:.2f}" width="{legend_w:.2f}" height="{legend_h:.2f}" '
        f'rx="10" fill="#ffffff" opacity="0.92" stroke="#e5e7eb" stroke-width="1"/>\n'
    )
    for idx, (val, label, clr) in enumerate(percentile_marks):
        y = legend_y + idx * 30
        s.append(f'<line x1="{legend_x:.2f}" y1="{y - 7:.2f}" x2="{legend_x + 34:.2f}" y2="{y - 7:.2f}" stroke="{clr}" stroke-width="3"/>\n')
        s.append(
            f'<text class="axis" x="{legend_x + 46:.2f}" y="{y:.2f}" text-anchor="start" fill="{clr}">{label} {val:.2f} us</text>\n'
        )

    if clipped:
        s.append(
            f'<text class="small" x="{legend_x - 18:.2f}" y="{legend_y + legend_h + 22:.2f}" text-anchor="start">最大值 {max(values):.2f} us；{clipped} 个样本 &gt; {vmax:.1f} us</text>\n'
        )

    s.append(f'<text class="axis" x="{ml + pw / 2:.2f}" y="{h - 44}" text-anchor="middle">{x_label}</text>\n')
    s.append(f'<text class="axis" x="34" y="{mt + ph / 2:.2f}" transform="rotate(-90 34 {mt + ph / 2:.2f})">计数</text>\n')
    s.append(svg_footer())

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("".join(s))


def main() -> int:
    docs_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if len(sys.argv) >= 3:
        data_dir = sys.argv[1]
        fig_dir = sys.argv[2]
    else:
        data_dir = os.path.join(docs_dir, "data", "benchmarks")
        fig_dir = os.path.join(docs_dir, "figures", "benchmarks")

    ensure_dir(fig_dir)

    draw_semantic_pass_rate(
        os.path.join(data_dir, "semantic_pass_rate.csv"),
        os.path.join(fig_dir, "semantic_pass_rate.svg"),
    )
    draw_distribution(
        os.path.join(data_dir, "channel_latency_us.csv"),
        os.path.join(data_dir, "channel_latency_summary.csv"),
        os.path.join(fig_dir, "channel_latency_distribution.svg"),
        "线程通道单向延迟分布（thread channel latency）",
        "延迟（微秒，单向）",
        "one_way_latency_us",
        "#0ea5e9",
    )
    draw_distribution(
        os.path.join(data_dir, "frame_time_us.csv"),
        os.path.join(data_dir, "frame_time_summary.csv"),
        os.path.join(fig_dir, "frame_time_distribution.svg"),
        "帧耗时分布（frame time）",
        "帧耗时（微秒）",
        "frame_time_us",
        "#7c3aed",
    )

    print(f"Generated SVG figures in: {fig_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

