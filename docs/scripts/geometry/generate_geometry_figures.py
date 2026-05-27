#!/usr/bin/env python3
import argparse
import csv
import html
import math
import os
import re
import subprocess
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_DATA_DIR = ROOT / "docs" / "data" / "geometry"
DEFAULT_FIG_DIR = ROOT / "docs" / "figures"

COLORS = {
    "blue": "#2563eb",
    "sky": "#0ea5e9",
    "green": "#16a34a",
    "orange": "#f59e0b",
    "red": "#dc2626",
    "purple": "#7c3aed",
    "slate": "#334155",
    "muted": "#64748b",
    "grid": "#e5e7eb",
    "ink": "#111827",
    "paper": "#ffffff",
}

ALGO_LABELS = {
    "batch_distance_matrix": "distance matrix",
    "nearest_point": "nearest point",
    "range_count_circle": "circle range count",
}

ALGO_COLORS = {
    "batch_distance_matrix": COLORS["blue"],
    "nearest_point": COLORS["green"],
    "range_count_circle": COLORS["orange"],
}


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def esc(s: object) -> str:
    return html.escape(str(s), quote=True)


def write_text(path: Path, text: str) -> None:
    ensure_dir(path.parent)
    path.write_text(text, encoding="utf-8")


def svg_header(w: int, h: int) -> str:
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img">
<style>
text{{font-family:Arial,Helvetica,sans-serif;fill:#111827;}}
.title{{font-size:24px;font-weight:700;}}
.subtitle{{font-size:13px;fill:#475569;}}
.label{{font-size:14px;font-weight:700;}}
.small{{font-size:12px;fill:#475569;}}
.tiny{{font-size:10px;fill:#64748b;}}
.axis{{font-size:12px;fill:#374151;}}
.box{{stroke:#1f2937;stroke-width:1.4;rx:14;ry:14;}}
</style>
<defs>
<marker id="arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
<path d="M0,0 L0,6 L9,3 z" fill="#334155" />
</marker>
<filter id="shadow" x="-10%" y="-10%" width="120%" height="130%">
<feDropShadow dx="0" dy="3" stdDeviation="4" flood-color="#0f172a" flood-opacity="0.12"/>
</filter>
</defs>
'''


def svg_footer() -> str:
    return "</svg>\n"


def text(x: float, y: float, body: str, cls: str = "small", anchor: str = "middle") -> str:
    return f'<text class="{cls}" x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}">{esc(body)}</text>\n'


def multiline_text(x: float, y: float, lines: Sequence[str], cls: str = "small", anchor: str = "middle", line_h: float = 17.0) -> str:
    out = [f'<text class="{cls}" x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}">']
    for i, line in enumerate(lines):
        dy = 0 if i == 0 else line_h
        out.append(f'<tspan x="{x:.2f}" dy="{dy:.2f}">{esc(line)}</tspan>')
    out.append('</text>\n')
    return "".join(out)


def box(x: float, y: float, w: float, h: float, lines: Sequence[str], fill: str, stroke: str = "#1f2937") -> str:
    cx = x + w / 2.0
    start_y = y + h / 2.0 - (len(lines) - 1) * 8.5 + 5
    return (
        f'<rect class="box" x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{fill}" stroke="{stroke}" filter="url(#shadow)"/>\n'
        + multiline_text(cx, start_y, lines, "label" if len(lines) == 1 else "small")
    )


def arrow(x1: float, y1: float, x2: float, y2: float, label: str = "", color: str = "#334155",
          dash: str = "") -> str:
    midx = (x1 + x2) / 2.0
    midy = (y1 + y2) / 2.0
    dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
    out = (
        f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
        f'stroke="{color}" stroke-width="2.2"{dash_attr} marker-end="url(#arrow)"/>\n'
    )
    if label:
        out += text(midx, midy - 10, label, "tiny")
    return out


def draw_system_architecture(out: Path) -> None:
    w, h = 1280, 720
    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, "FTCL geometry system architecture", "title", "start"))
    p.append(text(70, 66, "A script control plane drives a UVec data plane, which selects CPU or CUDA execution backends.", "subtitle", "start"))

    lanes = [
        (80, 120, 1120, 150, "1  Script control plane", "#eff6ff"),
        (80, 305, 1120, 160, "2  UVec data abstraction plane", "#fffbeb"),
        (80, 500, 1120, 135, "3  Execution backend plane", "#f8fafc"),
    ]
    for x, y, bw, bh, name, fill in lanes:
        p.append(f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" rx="22" fill="{fill}" stroke="#e2e8f0"/>\n')
        p.append(text(x + 22, y + 28, name, "label", "start"))

    p.append(box(125, 165, 170, 72, ["FTCL script", "geom ...", "thread channel"], "#dbeafe"))
    p.append(box(355, 165, 180, 72, ["Lexer / Parser", "command + words"], "#e0f2fe"))
    p.append(box(595, 165, 205, 72, ["Interpreter", "substitution", "command dispatch"], "#e0f2fe"))
    p.append(box(875, 165, 190, 72, ["geom command", "decode args", "select device"], "#dcfce7"))

    p.append(arrow(295, 201, 355, 201, "parse"))
    p.append(arrow(535, 201, 595, 201, "evaluate"))
    p.append(arrow(800, 201, 875, 201, "dispatch"))

    p.append(box(180, 350, 220, 76, ["Result value", "usually a UVec handle"], "#fff7ed"))
    p.append(box(480, 330, 360, 116, ["UVec handle manager", "id -> typed vector", "per-handle lock"], "#fef3c7"))
    p.append(box(920, 330, 220, 116, ["UVec object", "CPU/CUDA buffers", "valid flags"], "#fef3c7"))

    p.append(arrow(970, 237, 1030, 330, "lookup handle"))
    p.append(arrow(840, 388, 920, 388, "typed access"))
    p.append(arrow(480, 388, 400, 388, "return handle", COLORS["orange"]))

    p.append(box(190, 545, 220, 72, ["CPU backend", "std::vector view", "scalar loops"], "#f1f5f9"))
    p.append(box(530, 545, 230, 72, ["CUDA backend", "device pointer", "kernel launch"], "#ede9fe"))
    p.append(box(875, 545, 230, 72, ["Multi-GPU workers", "cuda:0 ... cuda:7", "independent handles"], "#ede9fe"))

    p.append(arrow(985, 446, 300, 545, "as_uptr(cpu)", COLORS["slate"]))
    p.append(arrow(1030, 446, 645, 545, "as_uptr(cuda:0)", COLORS["purple"]))
    p.append(arrow(1070, 446, 990, 545, "partitioned handles", COLORS["purple"], "7 5"))

    p.append(text(140, 680, "Key idea:", "label", "start"))
    p.append(text(225, 680, "scripts move small handles; UVec keeps large geometry arrays resident until a final readback is needed.", "subtitle", "start"))
    p.append(svg_footer())
    write_text(out, "".join(p))


def draw_uvec_sync(out: Path) -> None:
    w, h = 1280, 720
    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, "UVec cross-device synchronization", "title", "start"))
    p.append(text(70, 66, "Reads make a target device valid; mutable writes make other copies stale.", "subtitle", "start"))

    panels = [
        (80, 115, 1120, 245, "Read path: make data available without destroying other valid copies"),
        (80, 405, 1120, 220, "Write path: grant one mutable pointer and invalidate stale copies"),
    ]
    for x, y, bw, bh, title in panels:
        p.append(f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" rx="22" fill="#f8fafc" stroke="#e2e8f0"/>\n')
        p.append(text(x + 24, y + 30, title, "label", "start"))

    p.append(box(135, 190, 220, 92, ["Before", "CPU valid", "CUDA invalid"], "#dbeafe"))
    p.append(box(510, 185, 260, 102, ["as_uptr(cuda:0)", "copy CPU -> CUDA", "return const pointer"], "#fef3c7"))
    p.append(box(925, 190, 220, 92, ["After", "CPU valid", "CUDA valid", "both readable"], "#dcfce7"))
    p.append(arrow(355, 236, 510, 236, "request CUDA read", COLORS["slate"]))
    p.append(arrow(770, 236, 925, 236, "synchronized read", COLORS["green"]))
    p.append(arrow(340, 307, 955, 307, "physical copy happens only when target flag is invalid", COLORS["sky"], "8 6"))

    p.append(box(135, 475, 220, 90, ["Before", "CPU valid", "CUDA valid"], "#dcfce7"))
    p.append(box(510, 470, 260, 100, ["as_mut_uptr(cuda:0)", "prepare writable pointer", "latest = cuda:0"], "#fef3c7"))
    p.append(box(925, 475, 220, 90, ["After", "CUDA valid", "CPU invalid"], "#ede9fe"))
    p.append(arrow(355, 520, 510, 520, "request CUDA write", COLORS["slate"]))
    p.append(arrow(770, 520, 925, 520, "invalidate CPU copy", COLORS["red"]))
    p.append(text(600, 665, "The same handle is stable at script level; UVec decides when copy or invalidation is required.", "subtitle"))
    p.append(svg_footer())
    write_text(out, "".join(p))


def draw_geom_flow(out: Path) -> None:
    w, h = 1200, 760
    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, "geom command execution flow", "title", "start"))
    p.append(text(70, 66, "Example: geom nearest_point $dataset $queries cuda:0", "subtitle", "start"))

    nodes = [
        (90, 125, 250, 76, ["FTCL script", "geom nearest_point ..."], "#dbeafe"),
        (90, 255, 250, 76, ["Interpreter", "calls geom dispatcher"], "#e0f2fe"),
        (475, 255, 250, 76, ["Parse arguments", "handles + device"], "#dcfce7"),
        (860, 255, 250, 76, ["Lookup UVec", "typed storage check"], "#fef3c7"),
        (300, 460, 240, 82, ["CPU path", "read CPU buffer", "run scalar algorithm"], "#f1f5f9"),
        (660, 460, 240, 82, ["CUDA path", "sync to device", "launch kernel"], "#ede9fe"),
        (475, 620, 250, 76, ["Create result UVec", "return handle to script"], "#fff7ed"),
    ]
    for x, y, bw, bh, lines, fill in nodes:
        p.append(box(x, y, bw, bh, lines, fill))
    p.append(arrow(215, 201, 215, 255, "eval"))
    p.append(arrow(340, 293, 475, 293, "argv"))
    p.append(arrow(725, 293, 860, 293, "handles"))
    p.append(arrow(985, 331, 420, 460, "device=cpu"))
    p.append(arrow(985, 331, 780, 460, "device=cuda:0"))
    p.append(arrow(420, 542, 535, 620, "values"))
    p.append(arrow(780, 542, 665, 620, "device result"))
    p.append(arrow(475, 658, 340, 293, "FTCL value"))
    p.append(svg_footer())
    write_text(out, "".join(p))


def parse_perf_output(text_output: str) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    header = ["metric", "device", "n", "q", "time_us", "throughput_items_per_s"]
    for raw in text_output.splitlines():
        line = raw.strip()
        if not line or line.startswith("===") or line.startswith("["):
            continue
        if line == ",".join(header):
            continue
        parts = [x.strip() for x in line.split(",")]
        if len(parts) != 6:
            continue
        if parts[0] not in ALGO_LABELS:
            continue
        rows.append(dict(zip(header, parts)))
    return rows


def collect_perf_data(build_dir: Path, data_dir: Path, perf_mode: str) -> List[Dict[str, str]]:
    csv_path = data_dir / "geometry_perf_scale.csv"
    exe = build_dir / "test" / "test_geometry_perf_scale"
    if exe.exists():
        env = os.environ.copy()
        if perf_mode:
            env["FTCL_GEOMETRY_PERF_MODE"] = perf_mode
        proc = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, env=env)
        rows = parse_perf_output(proc.stdout)
        if rows:
            ensure_dir(data_dir)
            with csv_path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=["metric", "device", "n", "q", "time_us", "throughput_items_per_s"])
                writer.writeheader()
                writer.writerows(rows)
            return rows
    if csv_path.exists():
        with csv_path.open("r", newline="", encoding="utf-8") as f:
            return list(csv.DictReader(f))
    raise RuntimeError(f"No performance data found. Build {exe} first or provide {csv_path}.")


def collect_error_data(build_dir: Path, data_dir: Path) -> List[Dict[str, str]]:
    csv_path = data_dir / "geometry_error_samples.csv"
    exe = build_dir / "test" / "test_geometry_equivalence"
    rows: List[Dict[str, str]] = []
    if exe.exists():
        proc = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
        rx = re.compile(r"\[OK\]\s+(.*?)\s+max_err=([0-9eE+\-.]+)")
        for line in proc.stdout.splitlines():
            m = rx.search(line)
            if m:
                rows.append({"check": m.group(1), "max_abs_error": m.group(2)})
        if rows:
            ensure_dir(data_dir)
            with csv_path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=["check", "max_abs_error"])
                writer.writeheader()
                writer.writerows(rows)
            return rows
    if csv_path.exists():
        with csv_path.open("r", newline="", encoding="utf-8") as f:
            return list(csv.DictReader(f))
    rows = [{"check": name, "max_abs_error": "0"} for name in ALGO_LABELS]
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["check", "max_abs_error"])
        writer.writeheader()
        writer.writerows(rows)
    return rows


def nice_step(max_value: float, ticks: int = 6) -> float:
    if max_value <= 0:
        return 1.0
    raw = max_value / float(ticks)
    power = 10 ** math.floor(math.log10(raw))
    for c in [1.0, 2.0, 2.5, 5.0, 10.0]:
        if raw / power <= c:
            return c * power
    return 10.0 * power


def format_axis(v: float) -> str:
    av = abs(v)
    if av >= 1_000_000:
        return f"{v / 1_000_000:.1f}M"
    if av >= 1_000:
        return f"{v / 1_000:.1f}k"
    if av >= 10:
        return f"{v:.0f}"
    if av >= 1:
        return f"{v:.1f}"
    return f"{v:.2f}"


def group_perf(rows: List[Dict[str, str]]) -> Dict[Tuple[str, str, int], Dict[str, float]]:
    out: Dict[Tuple[str, str, int], Dict[str, float]] = {}
    for r in rows:
        key = (r["metric"], r["device"], int(r["n"]))
        out[key] = {
            "q": float(r["q"]),
            "time_us": float(r["time_us"]),
            "throughput": float(r["throughput_items_per_s"]),
        }
    return out


def draw_line_chart(out: Path, title: str, subtitle: str, ylabel: str, series: List[Dict[str, object]], y_min: float = 0.0, ref_line: float = None) -> None:
    w, h = 1200, 700
    ml, mr, mt, mb = 95, 230, 90, 105
    pw, ph = w - ml - mr, h - mt - mb
    xs = sorted({x for s in series for x, _ in s["values"]})
    y_values = [y for s in series for _, y in s["values"]]
    y_max = max(y_values + ([ref_line] if ref_line is not None else []) + [1.0]) * 1.18
    if y_max <= y_min:
        y_max = y_min + 1.0

    x_pos = {x: ml + (pw * i / max(1, len(xs) - 1)) for i, x in enumerate(xs)}

    def y_of(v: float) -> float:
        return mt + ph - (v - y_min) / (y_max - y_min) * ph

    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, title, "title", "start"))
    p.append(text(70, 66, subtitle, "subtitle", "start"))

    step = nice_step(y_max - y_min, 7)
    tick = math.ceil(y_min / step) * step
    while tick <= y_max + 1e-9:
        y = y_of(tick)
        p.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{ml + pw}" y2="{y:.2f}" stroke="{COLORS["grid"]}"/>\n')
        p.append(text(ml - 12, y + 4, format_axis(tick), "axis", "end"))
        tick += step

    p.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')
    p.append(f'<line x1="{ml}" y1="{mt + ph}" x2="{ml + pw}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')

    if ref_line is not None:
        y = y_of(ref_line)
        p.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{ml + pw}" y2="{y:.2f}" stroke="#0f172a" stroke-width="1.5" stroke-dasharray="7 5"/>\n')
        p.append(text(ml + pw + 8, y + 4, f"reference {format_axis(ref_line)}", "tiny", "start"))

    for s in series:
        vals = sorted(s["values"])
        color = str(s["color"])
        dash = ' stroke-dasharray="7 5"' if s.get("dash") else ""
        pts = " ".join(f'{x_pos[x]:.2f},{y_of(y):.2f}' for x, y in vals)
        p.append(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="3"{dash}/>\n')
        for x, y in vals:
            p.append(f'<circle cx="{x_pos[x]:.2f}" cy="{y_of(y):.2f}" r="4.5" fill="{color}"/>\n')

    for x in xs:
        xp = x_pos[x]
        p.append(f'<line x1="{xp:.2f}" y1="{mt + ph}" x2="{xp:.2f}" y2="{mt + ph + 6}" stroke="#111827"/>\n')
        p.append(text(xp, mt + ph + 25, str(x), "axis"))
    p.append(text(ml + pw / 2, h - 34, "Point count N", "axis"))
    p.append(f'<text class="axis" x="28" y="{mt + ph/2:.2f}" transform="rotate(-90 28 {mt + ph/2:.2f})">{esc(ylabel)}</text>\n')

    lx, ly = ml + pw + 34, mt + 10
    for i, s in enumerate(series):
        y = ly + i * 24
        color = str(s["color"])
        dash = ' stroke-dasharray="7 5"' if s.get("dash") else ""
        p.append(f'<line x1="{lx}" y1="{y}" x2="{lx + 32}" y2="{y}" stroke="{color}" stroke-width="3"{dash}/>\n')
        p.append(f'<circle cx="{lx + 16}" cy="{y}" r="4" fill="{color}"/>\n')
        p.append(text(lx + 42, y + 4, str(s["name"]), "tiny", "start"))

    p.append(svg_footer())
    write_text(out, "".join(p))


def draw_speedup(rows: List[Dict[str, str]], out: Path) -> None:
    perf = group_perf(rows)
    series = []
    for metric in ALGO_LABELS:
        vals = []
        ns = sorted({n for (m, _d, n) in perf if m == metric})
        for n in ns:
            cpu = perf.get((metric, "cpu", n))
            cuda = perf.get((metric, "cuda:0", n))
            if cpu and cuda and cuda["time_us"] > 0:
                vals.append((n, cpu["time_us"] / cuda["time_us"]))
        if vals:
            series.append({"name": ALGO_LABELS[metric], "values": vals, "color": ALGO_COLORS[metric]})
    draw_line_chart(out, "CPU vs CUDA speedup", "Prebuilt inputs, median end-to-end FTCL geom command time; higher is better.", "Speedup (CPU time / CUDA time)", series, 0.0, 1.0)


def draw_throughput(rows: List[Dict[str, str]], out: Path) -> None:
    perf = group_perf(rows)
    series = []
    for metric in ALGO_LABELS:
        for device, dash in [("cpu", True), ("cuda:0", False)]:
            vals = []
            ns = sorted({n for (m, d, n) in perf if m == metric and d == device})
            for n in ns:
                vals.append((n, perf[(metric, device, n)]["throughput"]))
            if vals:
                series.append({"name": f"{ALGO_LABELS[metric]} {device}", "values": vals, "color": ALGO_COLORS[metric], "dash": dash})
    draw_line_chart(out, "Geometry throughput curve", "Dashed lines are CPU; solid lines are CUDA. Units are pair/query operations per second.", "Throughput (items/s)", series)


def draw_time_bars(rows: List[Dict[str, str]], out: Path) -> None:
    perf = group_perf(rows)
    ns = sorted({int(r["n"]) for r in rows})
    selected_n = ns[-1]
    algos = [m for m in ALGO_LABELS if (m, "cpu", selected_n) in perf]
    w, h = 1200, 700
    ml, mr, mt, mb = 95, 60, 90, 130
    pw, ph = w - ml - mr, h - mt - mb
    values = []
    for a in algos:
        for d in ["cpu", "cuda:0"]:
            if (a, d, selected_n) in perf:
                values.append(perf[(a, d, selected_n)]["time_us"])
    y_max = max(values + [1.0]) * 1.22

    def y_of(v: float) -> float:
        return mt + ph - v / y_max * ph

    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, "Execution time comparison", "title", "start"))
    p.append(text(70, 66, f"Largest paper-mode scale in this run: N={selected_n}; lower is better.", "subtitle", "start"))

    step = nice_step(y_max, 7)
    tick = 0.0
    while tick <= y_max + 1e-9:
        y = y_of(tick)
        p.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{ml + pw}" y2="{y:.2f}" stroke="{COLORS["grid"]}"/>\n')
        p.append(text(ml - 12, y + 4, format_axis(tick), "axis", "end"))
        tick += step
    p.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')
    p.append(f'<line x1="{ml}" y1="{mt + ph}" x2="{ml + pw}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')

    group_w = pw / max(1, len(algos))
    bar_w = 72
    for i, a in enumerate(algos):
        cx = ml + group_w * i + group_w / 2
        for j, d in enumerate(["cpu", "cuda:0"]):
            v = perf.get((a, d, selected_n), {}).get("time_us")
            if v is None:
                continue
            x = cx + (-bar_w * 0.62 if d == "cpu" else bar_w * 0.62) - bar_w / 2
            y = y_of(v)
            color = "#94a3b8" if d == "cpu" else ALGO_COLORS[a]
            p.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{bar_w}" height="{mt + ph - y:.2f}" fill="{color}" rx="5"/>\n')
            p.append(text(x + bar_w / 2, y - 7, format_axis(v), "tiny"))
        p.append(multiline_text(cx, mt + ph + 28, [ALGO_LABELS[a]], "axis"))
    p.append(f'<text class="axis" x="28" y="{mt + ph/2:.2f}" transform="rotate(-90 28 {mt + ph/2:.2f})">Time (microseconds)</text>\n')
    p.append(f'<rect x="930" y="98" width="18" height="12" fill="#94a3b8"/><text class="tiny" x="956" y="109">CPU</text>\n')
    p.append(f'<rect x="930" y="122" width="18" height="12" fill="{COLORS["blue"]}"/><text class="tiny" x="956" y="133">CUDA color by algorithm</text>\n')
    p.append(svg_footer())
    write_text(out, "".join(p))


def draw_error_hist(rows: List[Dict[str, str]], out: Path) -> None:
    values = [float(r["max_abs_error"]) for r in rows]
    w, h = 1100, 620
    ml, mr, mt, mb = 95, 50, 90, 100
    pw, ph = w - ml - mr, h - mt - mb
    if not values:
        values = [0.0]
    all_same = max(values) == min(values)
    if all_same:
        counts = [len(values)]
        edges = [values[0], values[0]]
    else:
        bins = 16
        lo, hi = min(values), max(values)
        edges = [lo + (hi - lo) * i / bins for i in range(bins + 1)]
        counts = [0 for _ in range(bins)]
        for v in values:
            idx = min(bins - 1, int((v - lo) / (hi - lo) * bins))
            counts[idx] += 1
    max_count = max(counts + [1])

    def x_of(v: float) -> float:
        return ml + (v - edges[0]) / (edges[-1] - edges[0]) * pw if edges[-1] != edges[0] else ml

    def y_of(c: float) -> float:
        return mt + ph - c / max_count * ph

    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, "CPU/CUDA numerical error distribution", "title", "start"))
    p.append(text(70, 66, "Histogram of max absolute error per equivalence check; lower is better.", "subtitle", "start"))
    for i in range(max_count + 1):
        y = y_of(i)
        p.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{ml + pw}" y2="{y:.2f}" stroke="{COLORS["grid"]}"/>\n')
        p.append(text(ml - 12, y + 4, str(i), "axis", "end"))
    p.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')
    p.append(f'<line x1="{ml}" y1="{mt + ph}" x2="{ml + pw}" y2="{mt + ph}" stroke="#111827" stroke-width="1.4"/>\n')

    if all_same:
        bar_w = min(180.0, pw * 0.22)
        x = ml + (pw - bar_w) / 2.0
        y = y_of(counts[0])
        p.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{bar_w:.2f}" height="{mt + ph - y:.2f}" fill="#16a34a" opacity="0.78" rx="6"/>\n')
        p.append(text(x + bar_w / 2, y - 10, str(counts[0]), "small"))
        p.append(text(x + bar_w / 2, mt + ph + 25, format_axis(values[0]), "axis"))
        p.append(
            f'<rect x="{ml + pw - 270}" y="{mt + 22}" width="250" height="62" rx="12" fill="#ecfdf5" stroke="#16a34a" stroke-width="1.2"/>\n'
        )
        p.append(text(ml + pw - 145, mt + 48, "all checks exact match", "label"))
        p.append(text(ml + pw - 145, mt + 70, "max_abs_error = 0 for every sample", "tiny"))
    else:
        for i, c in enumerate(counts):
            x0, x1 = x_of(edges[i]), x_of(edges[i + 1])
            y = y_of(c)
            p.append(f'<rect x="{x0:.2f}" y="{y:.2f}" width="{max(2, x1 - x0 - 2):.2f}" height="{mt + ph - y:.2f}" fill="#16a34a" opacity="0.78"/>\n')

    p.append(text(ml + pw / 2, h - 34, "Max absolute error", "axis"))
    p.append(f'<text class="axis" x="28" y="{mt + ph/2:.2f}" transform="rotate(-90 28 {mt + ph/2:.2f})">Check count</text>\n')
    p.append(text(ml + pw - 8, mt + 20, f"samples={len(values)}, max={max(values):.3g}", "small", "end"))
    p.append(svg_footer())
    write_text(out, "".join(p))


def panel(p: List[str], x: float, y: float, w: float, h: float, title_body: str) -> None:
    p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" fill="#f8fafc" stroke="#cbd5e1"/>\n')
    p.append(text(x + 18, y + 28, title_body, "label", "start"))


def draw_geometry_visualization(out: Path) -> None:
    w, h = 1200, 820
    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>\n')
    p.append(text(70, 44, "Geometry algorithm visualization", "title", "start"))
    p.append(text(70, 66, "Four representative geom commands used by the FTCL script interface.", "subtitle", "start"))

    panels = [(70, 110, 500, 300, "point in polygon"), (630, 110, 500, 300, "nearest point"), (70, 460, 500, 300, "range count circle"), (630, 460, 500, 300, "AABB collision")]
    for args in panels:
        panel(p, *args)

    def map_pt(px: float, py: float, ox: float, oy: float, sw: float, sh: float, xmin: float, xmax: float, ymin: float, ymax: float) -> Tuple[float, float]:
        return (ox + (px - xmin) / (xmax - xmin) * sw, oy + sh - (py - ymin) / (ymax - ymin) * sh)

    # Point in polygon.
    ox, oy, sw, sh = 110, 165, 420, 200
    poly = [(0, 0), (4, 0), (4, 2.6), (2.2, 3.4), (0, 2.5)]
    pts = " ".join(f"{map_pt(x,y,ox,oy,sw,sh,-0.5,4.5,-0.5,3.8)[0]:.1f},{map_pt(x,y,ox,oy,sw,sh,-0.5,4.5,-0.5,3.8)[1]:.1f}" for x, y in poly)
    p.append(f'<polygon points="{pts}" fill="#dbeafe" stroke="#2563eb" stroke-width="3"/>\n')
    for x, y, c, label in [(1, 1, "#16a34a", "inside"), (4, 1, "#f59e0b", "boundary"), (4.4, 3.2, "#dc2626", "outside")]:
        sx, sy = map_pt(x, y, ox, oy, sw, sh, -0.5, 4.5, -0.5, 3.8)
        p.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="7" fill="{c}"/>\n')
        p.append(text(sx + 10, sy - 8, label, "tiny", "start"))

    # Nearest point.
    ox, oy, sw, sh = 670, 165, 420, 200
    data = [(0, 0), (1, 3), (3, 2), (5, 1), (4, 3.5)]
    q = (4.4, 1.1)
    nearest = (5, 1)
    qx, qy = map_pt(*q, ox, oy, sw, sh, -0.5, 5.5, -0.5, 4.0)
    nx, ny = map_pt(*nearest, ox, oy, sw, sh, -0.5, 5.5, -0.5, 4.0)
    p.append(f'<line x1="{qx:.1f}" y1="{qy:.1f}" x2="{nx:.1f}" y2="{ny:.1f}" stroke="#f59e0b" stroke-width="3" stroke-dasharray="6 5"/>\n')
    for x, y in data:
        sx, sy = map_pt(x, y, ox, oy, sw, sh, -0.5, 5.5, -0.5, 4.0)
        p.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="6" fill="#2563eb"/>\n')
    p.append(f'<path d="M{qx:.1f},{qy-10:.1f} L{qx+3:.1f},{qy-3:.1f} L{qx+10:.1f},{qy-3:.1f} L{qx+5:.1f},{qy+2:.1f} L{qx+7:.1f},{qy+9:.1f} L{qx:.1f},{qy+5:.1f} L{qx-7:.1f},{qy+9:.1f} L{qx-5:.1f},{qy+2:.1f} L{qx-10:.1f},{qy-3:.1f} L{qx-3:.1f},{qy-3:.1f} Z" fill="#dc2626"/>\n')
    p.append(text(qx, qy + 24, "query", "tiny"))

    # Range count circle.
    ox, oy, sw, sh = 110, 515, 420, 200
    center = (2.5, 1.8)
    cx, cy = map_pt(*center, ox, oy, sw, sh, 0, 5, 0, 4)
    r = 1.35 / 5 * sw
    p.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="#dcfce7" stroke="#16a34a" stroke-width="3"/>\n')
    points = [(1, 1), (2, 2), (3, 2.5), (4.5, 3.2), (2.8, 0.9), (0.8, 3.4), (3.5, 1.3)]
    for x, y in points:
        sx, sy = map_pt(x, y, ox, oy, sw, sh, 0, 5, 0, 4)
        inside = (x - center[0]) ** 2 + (y - center[1]) ** 2 <= 1.35 ** 2
        p.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="6" fill="{COLORS["green"] if inside else COLORS["muted"]}"/>\n')
    p.append(text(cx, cy - r - 10, "count = 4", "label"))

    # AABB collision.
    p.append(f'<rect x="710" y="545" width="170" height="120" fill="#dbeafe" stroke="#2563eb" stroke-width="3"/>\n')
    p.append(f'<rect x="825" y="595" width="190" height="105" fill="#fee2e2" stroke="#dc2626" stroke-width="3" opacity="0.8"/>\n')
    p.append(f'<rect x="710" y="708" width="110" height="30" fill="#dcfce7" stroke="#16a34a" stroke-width="2"/>\n')
    p.append(text(795, 535, "box A", "tiny"))
    p.append(text(920, 587, "box B", "tiny"))
    p.append(text(876, 675, "overlap", "label"))

    p.append(svg_footer())
    write_text(out, "".join(p))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate paper-ready FTCL geometry figures.")
    parser.add_argument("--build-dir", default="/tmp/ftcl-build-geometry-cuda", help="CMake build dir containing test/test_geometry_perf_scale")
    parser.add_argument("--perf-mode", default="paper", help="Value for FTCL_GEOMETRY_PERF_MODE when collecting timing data")
    parser.add_argument("--data-dir", default=str(DEFAULT_DATA_DIR))
    parser.add_argument("--fig-dir", default=str(DEFAULT_FIG_DIR))
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    data_dir = Path(args.data_dir)
    fig_dir = Path(args.fig_dir)
    arch_fig_dir = fig_dir / "architecture"
    geom_fig_dir = fig_dir / "geometry"
    ensure_dir(data_dir)
    ensure_dir(arch_fig_dir)
    ensure_dir(geom_fig_dir)

    perf_rows = collect_perf_data(build_dir, data_dir, args.perf_mode)
    error_rows = collect_error_data(build_dir, data_dir)

    draw_system_architecture(arch_fig_dir / "ftcl_geometry_system_architecture.svg")
    draw_uvec_sync(arch_fig_dir / "uvec_cross_device_sync.svg")
    draw_geom_flow(arch_fig_dir / "geom_command_execution_flow.svg")
    draw_speedup(perf_rows, geom_fig_dir / "geometry_cpu_cuda_speedup.svg")
    draw_time_bars(perf_rows, geom_fig_dir / "geometry_execution_time_bars.svg")
    draw_throughput(perf_rows, geom_fig_dir / "geometry_throughput_curve.svg")
    draw_error_hist(error_rows, geom_fig_dir / "geometry_error_distribution.svg")
    draw_geometry_visualization(geom_fig_dir / "geometry_algorithm_visualization.svg")

    print(f"Wrote data to {data_dir}")
    print(f"Wrote architecture figures to {arch_fig_dir}")
    print(f"Wrote geometry figures to {geom_fig_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
