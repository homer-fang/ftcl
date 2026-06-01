#!/usr/bin/env python3
import argparse
import csv
import html
import math
import os
import subprocess
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_BUILD_DIR = Path('/tmp/ftcl-build-geometry-cuda')
DEFAULT_DATA_DIR = ROOT / 'docs' / 'data' / 'geometry'
DEFAULT_FIG_DIR = ROOT / 'docs' / 'figures'

COLORS = {
    'ink': '#111827',
    'muted': '#64748b',
    'grid': '#e5e7eb',
    'blue': '#2563eb',
    'green': '#16a34a',
    'orange': '#f59e0b',
    'purple': '#7c3aed',
    'red': '#dc2626',
    'paper': '#ffffff',
}

ALGO_LABELS = {
    'batch_distance_matrix': '距离矩阵（distance matrix）',
    'nearest_point': '最近点（nearest point）',
    'range_count_circle': '圆形范围计数（circle range count）',
}

ALGO_COLORS = {
    'batch_distance_matrix': COLORS['blue'],
    'nearest_point': COLORS['green'],
    'range_count_circle': COLORS['orange'],
}


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def write_text(path: Path, content: str) -> None:
    ensure_dir(path.parent)
    path.write_text(content, encoding='utf-8')


def esc(value: object) -> str:
    return html.escape(str(value), quote=True)


def svg_header(w: int, h: int) -> str:
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img">
<style>
text{{font-family:'AR PL UMing TW MBE','AR PL UMing CN','FandolHei','Microsoft YaHei','SimHei',Arial,Helvetica,sans-serif;fill:{COLORS['ink']};}}
.title{{font-size:28px;font-weight:700;}}
.subtitle{{font-size:15px;fill:#475569;}}
.axis{{font-size:15px;fill:#374151;}}
.small{{font-size:14px;fill:#475569;}}
.tiny{{font-size:13px;fill:#64748b;}}
.label{{font-size:16px;font-weight:700;}}
.code{{font-family:Consolas,Monaco,'Courier New',monospace;font-size:10px;fill:#e2e8f0;}}
.box{{stroke:#1f2937;stroke-width:1.4;rx:14;ry:14;}}
</style>
<defs>
<marker id="arrow" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto" markerUnits="strokeWidth">
<path d="M0,0 L0,6 L9,3 z" fill="#334155" />
</marker>
</defs>
'''


def svg_footer() -> str:
    return '</svg>\n'


def text(x: float, y: float, body: str, cls: str = 'small', anchor: str = 'middle') -> str:
    return f'<text class="{cls}" x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}">{esc(body)}</text>\n'


def multiline_text(x: float,
                   y: float,
                   lines: Sequence[str],
                   cls: str = 'small',
                   anchor: str = 'middle',
                   line_h: float = 16.0) -> str:
    out = [f'<text class="{cls}" x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}">']
    for i, line in enumerate(lines):
        dy = 0 if i == 0 else line_h
        out.append(f'<tspan x="{x:.2f}" dy="{dy:.2f}">{esc(line)}</tspan>')
    out.append('</text>\n')
    return ''.join(out)


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
        return f'{v/1_000_000:.1f}M'
    if av >= 1_000:
        return f'{v/1_000:.1f}k'
    if av >= 10:
        return f'{v:.0f}'
    if av >= 1:
        return f'{v:.1f}'
    return f'{v:.2f}'


def line_chart(out: Path,
               title: str,
               subtitle: str,
               xlabel: str,
               ylabel: str,
               series: List[Dict[str, object]],
               y_min: float = 0.0,
               y_ref: float = None) -> None:
    w, h = 1450, 760
    ml, mr, mt, mb = 120, 380, 100, 120
    pw, ph = w - ml - mr, h - mt - mb

    xs = sorted({x for s in series for x, _ in s['values']})
    ys = [y for s in series for _, y in s['values']]
    y_max = max(ys + ([y_ref] if y_ref is not None else []) + [1.0]) * 1.18
    if y_max <= y_min:
        y_max = y_min + 1.0

    def y_of(v: float) -> float:
        return mt + ph - (v - y_min) / (y_max - y_min) * ph

    x_pos = {x: ml + pw * i / max(1, len(xs) - 1) for i, x in enumerate(xs)}

    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="{COLORS["paper"]}"/>\n')
    p.append(text(70, 44, title, 'title', 'start'))
    p.append(text(70, 66, subtitle, 'subtitle', 'start'))

    step = nice_step(y_max - y_min, 7)
    tick = math.ceil(y_min / step) * step
    while tick <= y_max + 1e-9:
        y = y_of(tick)
        p.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{ml + pw}" y2="{y:.2f}" stroke="{COLORS["grid"]}"/>\n')
        p.append(text(ml - 12, y + 4, format_axis(tick), 'axis', 'end'))
        tick += step

    p.append(f'<line x1="{ml}" y1="{mt}" x2="{ml}" y2="{mt + ph}" stroke="{COLORS["ink"]}" stroke-width="1.4"/>\n')
    p.append(f'<line x1="{ml}" y1="{mt + ph}" x2="{ml + pw}" y2="{mt + ph}" stroke="{COLORS["ink"]}" stroke-width="1.4"/>\n')

    if y_ref is not None:
        y = y_of(y_ref)
        p.append(f'<line x1="{ml}" y1="{y:.2f}" x2="{ml + pw}" y2="{y:.2f}" stroke="#0f172a" stroke-width="1.5" stroke-dasharray="7 5"/>\n')
        p.append(text(ml + pw + 8, y + 4, f'参考线 {format_axis(y_ref)}', 'tiny', 'start'))

    for s in series:
        vals = sorted(s['values'])
        color = str(s['color'])
        pts = ' '.join(f'{x_pos[x]:.2f},{y_of(y):.2f}' for x, y in vals)
        p.append(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="3"/>\n')
        for x, y in vals:
            p.append(f'<circle cx="{x_pos[x]:.2f}" cy="{y_of(y):.2f}" r="4.5" fill="{color}"/>\n')

    for x in xs:
        xp = x_pos[x]
        p.append(f'<line x1="{xp:.2f}" y1="{mt + ph}" x2="{xp:.2f}" y2="{mt + ph + 6}" stroke="{COLORS["ink"]}"/>\n')
        p.append(text(xp, mt + ph + 25, str(x), 'axis'))

    p.append(text(ml + pw / 2, h - 34, xlabel, 'axis'))
    p.append(text(ml, mt - 16, ylabel, 'axis', 'start'))

    lx, ly = ml + pw + 34, mt + 12
    for i, s in enumerate(series):
        y = ly + i * 30
        color = str(s['color'])
        p.append(f'<line x1="{lx}" y1="{y}" x2="{lx + 36}" y2="{y}" stroke="{color}" stroke-width="4"/>\n')
        p.append(f'<circle cx="{lx + 18}" cy="{y}" r="5.5" fill="{color}"/>\n')
        p.append(text(lx + 50, y + 5, str(s['name']), 'tiny', 'start'))

    p.append(svg_footer())
    write_text(out, ''.join(p))


def parse_perf_output(text_output: str) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    header = ['metric', 'device', 'n', 'q', 'time_us', 'throughput_items_per_s']
    for raw in text_output.splitlines():
        line = raw.strip()
        if not line or line.startswith('===') or line.startswith('#'):
            continue
        if line == ','.join(header):
            continue
        parts = [x.strip() for x in line.split(',')]
        if len(parts) != 6:
            continue
        if parts[0] not in ALGO_LABELS:
            continue
        rows.append(dict(zip(header, parts)))
    return rows


def load_or_collect_perf(build_dir: Path,
                         data_dir: Path,
                         perf_mode: str,
                         csv_name: str,
                         fallback_mode: str = '') -> List[Dict[str, str]]:
    csv_path = data_dir / csv_name
    exe = build_dir / 'test' / 'test_geometry_perf_scale'

    if exe.exists():
        env = os.environ.copy()
        env['FTCL_GEOMETRY_PERF_MODE'] = perf_mode
        proc = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, env=env)
        rows = parse_perf_output(proc.stdout)
        if not rows and fallback_mode:
            env['FTCL_GEOMETRY_PERF_MODE'] = fallback_mode
            proc = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, env=env)
            rows = parse_perf_output(proc.stdout)
        if rows:
            ensure_dir(data_dir)
            with csv_path.open('w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(
                    f,
                    fieldnames=['metric', 'device', 'n', 'q', 'time_us', 'throughput_items_per_s'],
                    lineterminator='\n',
                )
                writer.writeheader()
                writer.writerows(rows)
            return rows

    if csv_path.exists():
        with csv_path.open('r', newline='', encoding='utf-8') as f:
            return list(csv.DictReader(f))

    raise RuntimeError(f'Missing performance source: {csv_path}')


def parse_study_output(raw: str) -> Tuple[List[Dict[str, str]], List[Dict[str, str]]]:
    ablation: List[Dict[str, str]] = []
    concurrency: List[Dict[str, str]] = []
    section = ''

    for line in raw.splitlines():
        text_line = line.strip()
        if not text_line:
            continue
        if text_line == '[ABLATION]':
            section = 'ablation'
            continue
        if text_line == '[CONCURRENCY]':
            section = 'concurrency'
            continue
        if text_line.startswith('ablation_case,') or text_line.startswith('device,'):
            continue

        parts = [x.strip() for x in text_line.split(',')]
        if section == 'ablation' and len(parts) == 6:
            ablation.append({
                'ablation_case': parts[0],
                'metric': parts[1],
                'device': parts[2],
                'n': parts[3],
                'q': parts[4],
                'median_us': parts[5],
            })
        elif section == 'concurrency' and len(parts) == 8:
            concurrency.append({
                'device': parts[0],
                'workers': parts[1],
                'requests': parts[2],
                'elapsed_us': parts[3],
                'throughput_req_per_s': parts[4],
                'p50_us': parts[5],
                'p95_us': parts[6],
                'p99_us': parts[7],
            })

    return ablation, concurrency


def collect_study_data(build_dir: Path, data_dir: Path) -> Tuple[List[Dict[str, str]], List[Dict[str, str]]]:
    ablation_csv = data_dir / 'geometry_ablation.csv'
    concurrency_csv = data_dir / 'geometry_concurrency.csv'
    exe = build_dir / 'test' / 'bench_geometry_studies'

    if exe.exists():
        proc = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
        ablation, concurrency = parse_study_output(proc.stdout)
        if ablation:
            with ablation_csv.open('w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(
                    f,
                    fieldnames=['ablation_case', 'metric', 'device', 'n', 'q', 'median_us'],
                    lineterminator='\n',
                )
                writer.writeheader()
                writer.writerows(ablation)
        if concurrency:
            with concurrency_csv.open('w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(
                    f,
                    fieldnames=['device', 'workers', 'requests', 'elapsed_us', 'throughput_req_per_s', 'p50_us', 'p95_us', 'p99_us'],
                    lineterminator='\n',
                )
                writer.writeheader()
                writer.writerows(concurrency)
        return ablation, concurrency

    if not ablation_csv.exists() or not concurrency_csv.exists():
        raise RuntimeError('Missing bench_geometry_studies executable and cached study CSV files.')

    with ablation_csv.open('r', newline='', encoding='utf-8') as f:
        ablation = list(csv.DictReader(f))
    with concurrency_csv.open('r', newline='', encoding='utf-8') as f:
        concurrency = list(csv.DictReader(f))
    return ablation, concurrency


def parse_multi_gpu_output(raw: str) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    header = [
        'metric',
        'gpu_count',
        'n',
        'total_q',
        'per_gpu_q',
        'rounds',
        'time_us',
        'total_work_items',
        'throughput_items_per_s',
        'speedup_vs_1gpu',
        'parallel_efficiency',
    ]

    for line in raw.splitlines():
        text_line = line.strip()
        if not text_line or text_line.startswith('===') or text_line.startswith('#'):
            continue
        if text_line == ','.join(header):
            continue
        parts = [x.strip() for x in text_line.split(',')]
        if len(parts) != len(header):
            continue
        if parts[0] not in ALGO_LABELS:
            continue
        rows.append(dict(zip(header, parts)))

    return rows


def load_or_collect_multi_gpu(build_dir: Path, data_dir: Path, mode: str, scaling: str) -> List[Dict[str, str]]:
    csv_path = data_dir / 'geometry_multi_gpu_scaling.csv'
    exe = build_dir / 'test' / 'bench_geometry_multi_gpu'

    if exe.exists():
        env = os.environ.copy()
        env['FTCL_MULTI_GPU_MODE'] = mode
        env['FTCL_MULTI_GPU_SCALING'] = scaling
        proc = subprocess.run([str(exe)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, env=env)
        rows = parse_multi_gpu_output(proc.stdout)
        if rows:
            with csv_path.open('w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(
                    f,
                    fieldnames=[
                        'metric',
                        'gpu_count',
                        'n',
                        'total_q',
                        'per_gpu_q',
                        'rounds',
                        'time_us',
                        'total_work_items',
                        'throughput_items_per_s',
                        'speedup_vs_1gpu',
                        'parallel_efficiency',
                    ],
                    lineterminator='\n',
                )
                writer.writeheader()
                writer.writerows(rows)
            return rows

    if csv_path.exists():
        with csv_path.open('r', newline='', encoding='utf-8') as f:
            return list(csv.DictReader(f))

    return []


def compute_break_even(perf_rows: List[Dict[str, str]]) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    grouped: Dict[str, Dict[int, Dict[str, float]]] = {}
    meta: Dict[int, Tuple[int, int]] = {}
    for row in perf_rows:
        metric = row['metric']
        n = int(row['n'])
        q = int(row['q'])
        work_items = n * q
        grouped.setdefault(metric, {}).setdefault(work_items, {})[row['device']] = float(row['time_us'])
        meta[work_items] = (n, q)

    series = []
    break_points = []

    for metric in ALGO_LABELS:
        speed = []
        points = grouped.get(metric, {})
        for work_items in sorted(points.keys()):
            cpu = points[work_items].get('cpu')
            cuda = points[work_items].get('cuda:0')
            if cpu is not None and cuda is not None and cuda > 0:
                speed.append((work_items, cpu / cuda))

        if not speed:
            continue

        series.append({'metric': metric, 'name': ALGO_LABELS[metric], 'values': speed, 'color': ALGO_COLORS[metric]})

        cross = None
        if speed[0][1] >= 1.0:
            cross = float(speed[0][0])
        else:
            for i in range(1, len(speed)):
                x0, s0 = speed[i - 1]
                x1, s1 = speed[i]
                if s0 < 1.0 <= s1 and s1 != s0:
                    cross = x0 + (1.0 - s0) * (x1 - x0) / (s1 - s0)
                    break

        min_n, min_q = meta[speed[0][0]]
        max_n, max_q = meta[speed[-1][0]]
        break_points.append({
            'metric': metric,
            'label': ALGO_LABELS[metric],
            'break_even_work_items': cross,
            'min_n': min_n,
            'min_q': min_q,
            'max_n': max_n,
            'max_q': max_q,
            'speedup_at_min_work_items': speed[0][1],
            'speedup_at_max_work_items': speed[-1][1],
        })

    return series, break_points

def draw_break_even(series: List[Dict[str, object]], out: Path) -> None:
    # Focus on the crossing region so the break-even trend is readable.
    plot_series = []
    for item in series:
        values = [(work_items, speed) for work_items, speed in item['values'] if work_items <= 4096 * 512]
        if not values:
            values = item['values']
        plot_series.append({'name': item['name'], 'values': values, 'color': item['color']})

    line_chart(
        out,
        'CUDA 盈亏平衡点（break-even point）',
        '按工作项数量 N x Q 密集扫描；加速比超过 1.0 表示 CUDA 快于 CPU。',
        '工作项数量（work items = N x Q）',
        '加速比（CPU 时间 / CUDA 时间）',
        plot_series,
        0.0,
        1.0,
    )


def write_break_even_csv(rows: List[Dict[str, object]], out: Path) -> None:
    ensure_dir(out.parent)
    with out.open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow([
            'metric',
            'label',
            'break_even_work_items',
            'min_n',
            'min_q',
            'max_n',
            'max_q',
            'speedup_at_min_work_items',
            'speedup_at_max_work_items',
        ])
        for r in rows:
            be = '' if r['break_even_work_items'] is None else f"{r['break_even_work_items']:.2f}"
            writer.writerow([
                r['metric'],
                r['label'],
                be,
                r['min_n'],
                r['min_q'],
                r['max_n'],
                r['max_q'],
                f"{r['speedup_at_min_work_items']:.6f}",
                f"{r['speedup_at_max_work_items']:.6f}",
            ])

def write_ablation_table(ablation_rows: List[Dict[str, str]], out: Path) -> None:
    grouped: Dict[Tuple[str, str], Dict[str, float]] = {}
    meta: Dict[Tuple[str, str], Tuple[str, str]] = {}

    for row in ablation_rows:
        key = (row['metric'], row['device'])
        grouped.setdefault(key, {})[row['ablation_case']] = float(row['median_us'])
        meta[key] = (row['n'], row['q'])

    lines = [
        '# Geometry Ablation Table',
        '',
        'Median command latency in microseconds (lower is better).',
        '',
        '| Metric | Device | N | Q | Inline literal | Prebuilt no readback | Prebuilt with readback | Inline->Prebuilt gain | Readback overhead |',
        '|---|---:|---:|---:|---:|---:|---:|---:|---:|',
    ]

    for metric in ALGO_LABELS:
        for device in ['cpu', 'cuda:0']:
            key = (metric, device)
            if key not in grouped:
                continue
            values = grouped[key]
            inline = values.get('inline_literal', float('nan'))
            prebuilt = values.get('prebuilt_no_readback', float('nan'))
            readback = values.get('prebuilt_with_readback', float('nan'))
            n, q = meta[key]

            gain = (inline / prebuilt) if inline > 0 and prebuilt > 0 else float('nan')
            overhead = (readback / prebuilt) if readback > 0 and prebuilt > 0 else float('nan')

            lines.append(
                f"| {ALGO_LABELS[metric]} | {device} | {n} | {q} | {inline:.2f} | {prebuilt:.2f} | {readback:.2f} | {gain:.2f}x | {overhead:.2f}x |"
            )

    write_text(out, '\n'.join(lines) + '\n')


def draw_uvec_state_machine(out: Path) -> None:
    w, h = 1280, 760
    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="{COLORS["paper"]}"/>\n')
    p.append(text(70, 44, 'UVec 状态机（state machine）', 'title', 'start'))
    p.append(text(70, 66, '读取可进入共享可读状态；可变写入会回到单一最新写入者。', 'subtitle', 'start'))

    states = {
        'cpu': (155, 230, 260, 96, ['CPU 最新', 'CPU valid = 1', 'CUDA valid = 0'], '#dbeafe'),
        'both': (510, 465, 260, 96, ['共享可读', 'CPU valid = 1', 'CUDA valid = 1'], '#dcfce7'),
        'cuda': (865, 230, 260, 96, ['CUDA 最新', 'CUDA valid = 1', 'CPU valid = 0'], '#ede9fe'),
    }

    for x, y, bw, bh, lines, fill in states.values():
        p.append(f'<rect class="box" x="{x}" y="{y}" width="{bw}" height="{bh}" fill="{fill}" filter="url(#shadow)"/>\n')
        p.append(multiline_text(x + bw / 2, y + bh / 2 - 18, lines, 'small', 'middle', 18))

    def arr(x1: float, y1: float, x2: float, y2: float, label: str = '', color: str = '#334155',
            dash: str = '') -> str:
        midx = (x1 + x2) / 2.0
        midy = (y1 + y2) / 2.0
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ''
        out = (
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="2.4"{dash_attr} marker-end="url(#arrow)"/>\n'
        )
        if label:
            out += text(midx, midy - 12, label, 'tiny')
        return out

    p.append(arr(310, 326, 545, 465, color=COLORS['green']))
    p.append(arr(610, 465, 375, 326, color=COLORS['red']))
    p.append(arr(970, 326, 735, 465, color=COLORS['green']))
    p.append(arr(670, 465, 905, 326, color=COLORS['red']))
    p.append(arr(415, 260, 865, 260, color=COLORS['purple'], dash='7 5'))
    p.append(arr(865, 296, 415, 296, color=COLORS['blue'], dash='7 5'))

    p.append(f'<rect x="145" y="610" width="990" height="92" rx="18" fill="#f8fafc" stroke="#e2e8f0"/>\n')
    legend = [
        (190, 642, COLORS['green'], '', '在过期设备上读取：复制数据并进入共享可读状态。'),
        (190, 672, COLORS['red'], '', '从共享可读状态可变写入：使另一个设备副本失效。'),
        (690, 642, COLORS['purple'], '7 5', '从 CPU 最新状态直接 CUDA 写入：CUDA 成为唯一最新写入者。'),
        (690, 672, COLORS['blue'], '7 5', '从 CUDA 最新状态直接 CPU 写入：CPU 成为唯一最新写入者。'),
    ]
    for x, y, color, dash, label in legend:
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ''
        p.append(f'<line x1="{x}" y1="{y}" x2="{x + 42}" y2="{y}" stroke="{color}" stroke-width="3"{dash_attr}/>\n')
        p.append(text(x + 54, y + 5, label, 'tiny', 'start'))
    p.append(text(190, 695, '不变量（Invariant）：至少一个物理缓冲区有效；读取可能扩大有效集合，写入会收缩为一个最新写入者。', 'tiny', 'start'))
    p.append(svg_footer())
    write_text(out, ''.join(p))


def draw_real_demo(out: Path) -> None:
    w, h = 1300, 780
    p = [svg_header(w, h)]
    p.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="{COLORS["paper"]}"/>\n')
    p.append(text(70, 44, 'FTCL 海洋空间计算演示（ocean spatial computing demo）', 'title', 'start'))
    p.append(text(70, 66, '海洋牧场感知 + 智慧港口安全 + 无人机巡检被组织到同一条脚本流水线中。', 'subtitle', 'start'))

    x0, y0, cw, ch = 90, 120, 620, 560
    p.append(f'<rect x="{x0}" y="{y0}" width="{cw}" height="{ch}" fill="#eff6ff" stroke="#94a3b8" stroke-width="2"/>\n')
    p.append(f'<path d="M{x0},{y0 + 92} C230,160 315,188 410,148 C510,112 590,132 {x0 + cw},{y0 + 95} L{x0 + cw},{y0} L{x0},{y0} Z" fill="#e2e8f0"/>\n')
    p.append(f'<path d="M{x0 + 430},{y0 + 70} L{x0 + cw},{y0 + 70} L{x0 + cw},{y0 + 185} L{x0 + 500},{y0 + 185} L{x0 + 500},{y0 + 130} L{x0 + 430},{y0 + 130} Z" fill="#cbd5e1"/>\n')
    p.append(text(x0 + 540, y0 + 55, '智慧港口（smart port）', 'tiny'))

    for i in range(11):
        gx = x0 + i * cw / 10
        gy = y0 + i * ch / 10
        p.append(f'<line x1="{gx:.1f}" y1="{y0}" x2="{gx:.1f}" y2="{y0 + ch}" stroke="#dbeafe"/>\n')
        p.append(f'<line x1="{x0}" y1="{gy:.1f}" x2="{x0 + cw}" y2="{gy:.1f}" stroke="#dbeafe"/>\n')

    coverage = [(250, 420, 108, '声呐 A（sonar A）'), (530, 455, 128, '声呐 B（sonar B）'), (620, 220, 86, '港口雷达（radar）')]
    for cx, cy, r, label in coverage:
        p.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="#dcfce7"/>\n')
        p.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="#22c55e" stroke-width="2"/>\n')
        p.append(text(cx, cy - r - 8, label, 'tiny'))

    cages = [(185, 330, 90, 60), (300, 330, 90, 60), (185, 415, 90, 60), (300, 415, 90, 60)]
    for x, y, bw, bh in cages:
        p.append(f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" fill="#bae6fd" stroke="#0284c7" stroke-width="2" rx="8"/>\n')
    p.append(text(287, 315, '海洋牧场网箱（cages）', 'tiny'))

    obstacles = [(505, 165, 74, 52), (600, 150, 58, 90), (450, 565, 88, 36)]
    for ox, oy, ow, oh in obstacles:
        p.append(f'<rect x="{ox}" y="{oy}" width="{ow}" height="{oh}" fill="#334155" opacity="0.86" rx="7"/>\n')
    p.append(text(614, 145, '泊位/障碍物（obstacles）', 'tiny'))

    ship_path = [(145, 615), (250, 570), (370, 530), (500, 500), (645, 475)]
    path_pts = ' '.join(f'{x},{y}' for x, y in ship_path)
    p.append(f'<polyline points="{path_pts}" fill="none" stroke="#f59e0b" stroke-width="4" stroke-dasharray="9 6"/>\n')
    p.append(f'<polygon points="640,463 665,475 640,487" fill="#f59e0b"/>\n')
    p.append(text(378, 516, '船舶规划路径（route）', 'tiny'))

    uav = (155, 235)
    p.append(f'<path d="M{uav[0]},{uav[1] - 13} L{uav[0] + 18},{uav[1] + 10} L{uav[0]},{uav[1] + 4} L{uav[0] - 18},{uav[1] + 10} Z" fill="#2563eb"/>\n')
    p.append(text(uav[0] + 34, uav[1] + 6, '无人机（UAV）', 'tiny', 'start'))

    sensors = [(220, 250), (340, 235), (255, 520), (535, 325)]
    for sx, sy in sensors:
        p.append(f'<circle cx="{sx}" cy="{sy}" r="7" fill="#0ea5e9" stroke="#075985" stroke-width="1.5"/>\n')
    p.append(text(300, 220, '传感浮标（sensor buoys）', 'tiny'))

    floats = [(440, 265), (575, 360), (615, 512), (350, 600), (465, 455), (690, 300)]
    for fx, fy in floats:
        p.append(f'<circle cx="{fx}" cy="{fy}" r="8" fill="#dc2626"/>\n')
    nearest = min(floats, key=lambda pt: (pt[0] - uav[0]) ** 2 + (pt[1] - uav[1]) ** 2)
    p.append(f'<line x1="{uav[0]}" y1="{uav[1]}" x2="{nearest[0]}" y2="{nearest[1]}" stroke="#ef4444" stroke-width="2.5" stroke-dasharray="5 5"/>\n')
    p.append(text((uav[0] + nearest[0]) / 2 + 4, (uav[1] + nearest[1]) / 2 - 8, '最近异常目标（nearest anomaly）', 'tiny', 'start'))

    p.append(text(760, 150, 'FTCL 脚本片段（script excerpt）', 'label', 'start'))
    script_lines = [
        'set sensors [geom uvec_points $buoys cuda:0]',
        'set targets [geom uvec_points $floating_objects cuda:0]',
        'set uav [geom uvec_points {{$ux $uy}} cuda:0]',
        'set near [geom nearest_point $targets $uav cuda:0]',
        'set cover [geom range_count_circle $targets $sonar 120 cuda:0]',
        'set risk [geom collision_aabb $ship_box $restricted cuda:0]',
        'set dist [geom batch_distance_matrix $sensors $targets cuda:0]',
    ]
    p.append('<rect x="740" y="160" width="540" height="378" rx="14" fill="#0f172a"/>\n')
    for i, line in enumerate(script_lines):
        p.append(f'<text class="code" x="762" y="{196 + i * 44}">{esc(line)}</text>\n')

    p.append(multiline_text(1010, 572, [
        '输出语义（Output semantics）：',
        'near -> 异常目标句柄',
        'cover -> 每个声呐的目标计数',
        'risk -> 船舶/障碍物碰撞标记',
        'dist -> 大规模传感器-目标距离矩阵',
    ], 'small', 'middle'))

    p.append(svg_footer())
    write_text(out, ''.join(p))


def draw_concurrency_throughput(rows: List[Dict[str, str]], out: Path) -> None:
    series = []
    for device, color in [('cpu', COLORS['blue']), ('cuda:0', COLORS['purple'])]:
        values = []
        for r in sorted(rows, key=lambda x: (x['device'], int(x['workers']))):
            if r['device'] != device:
                continue
            values.append((int(r['workers']), float(r['throughput_req_per_s'])))
        if values:
            device_label = 'CPU' if device == 'cpu' else 'CUDA cuda:0'
            series.append({'name': device_label, 'values': values, 'color': color})

    line_chart(
        out,
        '并发几何吞吐量（concurrent throughput）',
        '负载：thread-channel 流水线 + geom nearest_point 工作任务。',
        '工作线程数（worker threads）',
        '吞吐量（requests/s）',
        series,
        0.0,
        None,
    )


def draw_concurrency_latency(rows: List[Dict[str, str]], out: Path) -> None:
    series = []
    for device, color in [('cpu', COLORS['green']), ('cuda:0', COLORS['red'])]:
        values = []
        for r in sorted(rows, key=lambda x: (x['device'], int(x['workers']))):
            if r['device'] != device:
                continue
            values.append((int(r['workers']), float(r['p95_us'])))
        if values:
            device_label = 'CPU' if device == 'cpu' else 'CUDA cuda:0'
            series.append({'name': f'{device_label} P95', 'values': values, 'color': color})

    line_chart(
        out,
        '并发几何尾延迟（latency tail）',
        '同一 worker/channel 实验中的单请求 P95 延迟；数值越低越好。',
        '工作线程数（worker threads）',
        '延迟（微秒，us）',
        series,
        0.0,
        None,
    )


def draw_multi_gpu_scaling(rows: List[Dict[str, str]], out: Path) -> None:
    if not rows:
        return

    gpu_counts = sorted({int(r['gpu_count']) for r in rows})
    series: List[Dict[str, object]] = []

    if gpu_counts:
        series.append({
            'name': '理想线性扩展（ideal）',
            'values': [(count, float(count)) for count in gpu_counts],
            'color': COLORS['muted'],
        })

    for metric in ALGO_LABELS:
        values = []
        for r in sorted(rows, key=lambda x: int(x['gpu_count'])):
            if r['metric'] != metric:
                continue
            values.append((int(r['gpu_count']), float(r['speedup_vs_1gpu'])))
        if values:
            series.append({'name': ALGO_LABELS[metric], 'values': values, 'color': ALGO_COLORS[metric]})

    line_chart(
        out,
        '多 GPU 几何扩展性（Multi-GPU scaling）',
        '弱扩展实验：每张 GPU 保持固定工作量，查询被分配到 CUDA 0 到 7 号设备。',
        'GPU 数量（devices）',
        '相对 1 GPU 的加速比（x）',
        series,
        0.0,
        1.0,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description='Generate FTCL geometry study add-on figures and tables.')
    parser.add_argument('--build-dir', default=str(DEFAULT_BUILD_DIR))
    parser.add_argument('--data-dir', default=str(DEFAULT_DATA_DIR))
    parser.add_argument('--fig-dir', default=str(DEFAULT_FIG_DIR))
    parser.add_argument('--break-even-mode', default='break_even')
    parser.add_argument('--break-even-fallback-mode', default='smoke')
    parser.add_argument('--multi-gpu-mode', default='paper')
    parser.add_argument('--multi-gpu-scaling', default='weak')
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    data_dir = Path(args.data_dir)
    fig_dir = Path(args.fig_dir)
    geom_fig_dir = fig_dir / 'geometry'
    arch_fig_dir = fig_dir / 'architecture'

    ensure_dir(data_dir)
    ensure_dir(geom_fig_dir)
    ensure_dir(arch_fig_dir)

    perf_rows = load_or_collect_perf(
        build_dir,
        data_dir,
        args.break_even_mode,
        'geometry_break_even_perf.csv',
        args.break_even_fallback_mode,
    )
    speed_series, break_even_rows = compute_break_even(perf_rows)
    draw_break_even(speed_series, geom_fig_dir / 'geometry_break_even_point.svg')
    write_break_even_csv(break_even_rows, data_dir / 'geometry_break_even_points.csv')

    ablation_rows, concurrency_rows = collect_study_data(build_dir, data_dir)
    write_ablation_table(ablation_rows, data_dir / 'geometry_ablation_table.md')

    draw_real_demo(geom_fig_dir / 'geometry_real_application_demo.svg')
    draw_uvec_state_machine(arch_fig_dir / 'uvec_state_machine.svg')
    draw_concurrency_throughput(concurrency_rows, geom_fig_dir / 'geometry_concurrency_throughput.svg')
    draw_concurrency_latency(concurrency_rows, geom_fig_dir / 'geometry_concurrency_latency_p95.svg')

    multi_gpu_rows = load_or_collect_multi_gpu(build_dir, data_dir, args.multi_gpu_mode, args.multi_gpu_scaling)
    draw_multi_gpu_scaling(multi_gpu_rows, geom_fig_dir / 'geometry_multi_gpu_scaling.svg')

    print(f'Wrote add-on study data to {data_dir}')
    print(f'Wrote add-on study figures to {geom_fig_dir} and {arch_fig_dir}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())










