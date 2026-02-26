#!/usr/bin/env python3
# SPDX-FileCopyrightText: (c) 2025-2026 Trailblaze Software, all rights reserved
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; version 2.1.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# For LGPL2 incompatible licensing or development requests, please contact
# trailblaze.software@gmail.com

"""
benchmark.py – Multi-tool LAS/LAZ performance comparison

Compares LAS++ (at varying thread counts) against:
  • laszip   (via the C++ benchmark binary, single-threaded reference)
  • laspy    (Python, both lazrs-parallel and laszip backends)
  • laz-rs   (via lazrs Python package, if installed separately)
  • PDAL     (via CLI, if pdal is on PATH)

Usage:
  python benchmark.py --file /path/to/cloud.laz
                      [--threads 1,2,4,8]
                      [--iterations 3]
                      [--warmup 1]
                      [--operation read|write|both]
                      [--chunk-size 50000]
                      [--benchmark-binary /path/to/benchmark]
                      [--tools laspp,laszip,laspy,lazrs,pdal]
                      [--plot]
                      [--output results.json]
"""

from __future__ import annotations

import argparse
import json
import math
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# ── Optional third-party imports ──────────────────────────────────────────────
# Note: lazrs (used by laspy) reads RAYON_NUM_THREADS at library initialization.
# We'll set it per-benchmark run, but it may only take effect if set before
# the first import. For accurate thread control, set RAYON_NUM_THREADS before
# running the benchmark script.

try:
    import laspy  # type: ignore
    _HAS_LASPY = True
except ImportError:
    _HAS_LASPY = False

try:
    import lazrs  # type: ignore
    _HAS_LAZRS = True
except ImportError:
    _HAS_LAZRS = False

_HAS_PDAL = shutil.which("pdal") is not None

try:
    import matplotlib.pyplot as plt  # type: ignore
    import matplotlib.ticker as mticker  # type: ignore
    _HAS_MATPLOTLIB = True
except ImportError:
    _HAS_MATPLOTLIB = False


# ── Data structures ────────────────────────────────────────────────────────────

@dataclass
class BenchResult:
    tool: str
    operation: str   # "read" or "write"
    threads: int     # 0 = "OS default / not controlled"
    iteration: int
    time_s: float


@dataclass
class BenchSummary:
    """Aggregated statistics for one (tool, operation, threads) combination."""
    tool: str
    operation: str
    threads: int
    n: int
    mean_s: float
    min_s: float
    max_s: float
    num_points: int
    file_size_bytes: int

    @property
    def pts_per_s(self) -> float:
        return self.num_points / self.mean_s if self.mean_s > 0 else 0.0

    @property
    def mb_per_s(self) -> float:
        return (self.file_size_bytes / 1e6) / self.mean_s if self.mean_s > 0 else 0.0

    @property
    def thread_label(self) -> str:
        return "default" if self.threads == 0 else str(self.threads)


# ── Tool discovery ─────────────────────────────────────────────────────────────

def find_benchmark_binary(hint: Optional[str]) -> Optional[Path]:
    """Locate the C++ benchmark binary."""
    candidates: List[Path] = []

    if hint:
        candidates.append(Path(hint))

    script_dir = Path(__file__).parent
    build_root = script_dir.parent

    # Common build directory layouts
    for build_dir in ["build", "linux-build", "build_release", "cmake-build-release"]:
        candidates.append(build_root / build_dir / "apps" / "benchmark")
        candidates.append(build_root / build_dir / "apps" / "benchmark.exe")
        # Also check directly in the build dir (some generators flatten it)
        candidates.append(build_root / build_dir / "benchmark")
        candidates.append(build_root / build_dir / "benchmark.exe")

    # Check PATH
    which = shutil.which("benchmark")
    if which:
        candidates.append(Path(which))

    for p in candidates:
        if p.exists() and os.access(p, os.X_OK):
            return p

    return None


def detect_laspy_backends() -> Dict[str, bool]:
    """Return which laspy LAZ backends are available."""
    backends: Dict[str, bool] = {}
    if not _HAS_LASPY:
        return backends

    for name, backend in [
        ("laspy_lazrs_parallel", laspy.LazBackend.LazrsParallel),
        ("laspy_lazrs", laspy.LazBackend.Lazrs),
        ("laspy_laszip", laspy.LazBackend.Laszip),
    ]:
        try:
            avail = laspy.LazBackend.detect_available()
            backends[name] = backend in avail
        except Exception:
            backends[name] = False

    return backends


# ── Benchmarking helpers ───────────────────────────────────────────────────────

def timed(fn) -> float:
    """Return wall-clock seconds taken by fn()."""
    t0 = time.perf_counter()
    fn()
    return time.perf_counter() - t0


def run_cpp_benchmark(
    binary: Path,
    file: Path,
    thread_counts: List[int],
    iterations: int,
    warmup: int,
    operation: str,
    chunk_size: Optional[int],
    include_laszip: bool,
    include_lazperf: bool = True,
) -> Tuple[dict, List[BenchResult]]:
    """
    Invoke the C++ benchmark binary and parse its JSON output.

    Returns (metadata_dict, list_of_BenchResult).
    """
    cmd = [
        str(binary),
        "--file", str(file),
        "--threads", ",".join(str(t) for t in thread_counts),
        "--iterations", str(iterations),
        "--warmup", str(warmup),
        "--operation", operation,
    ]
    if chunk_size is not None:
        cmd += ["--chunk-size", str(chunk_size)]
    if not include_laszip:
        cmd.append("--no-laszip")
    if not include_lazperf:
        cmd.append("--no-lazperf")

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"benchmark binary failed (exit {proc.returncode}):\n{proc.stderr}"
        )

    data = json.loads(proc.stdout)

    metadata = {
        k: data[k]
        for k in ("platform", "has_thread_control", "hardware_threads",
                  "file", "num_points", "file_size_bytes",
                  "point_format", "is_compressed")
        if k in data
    }

    results = [
        BenchResult(
            tool=r["tool"],
            operation=r["operation"],
            threads=r["threads"],
            iteration=r["iteration"],
            time_s=r["time_s"],
        )
        for r in data.get("results", [])
    ]
    return metadata, results


# ── laspy benchmarks ───────────────────────────────────────────────────────────

def _laspy_read(file: Path, backend) -> None:
    with laspy.open(str(file), laz_backend=backend) as f:
        f.read()


def benchmark_laspy_read(
    file: Path,
    backend,
    backend_name: str,
    warmup: int,
    iterations: int,
    num_threads: Optional[int] = None,
) -> List[BenchResult]:
    """
    Benchmark laspy read. For lazrs-parallel backend, uses subprocess to ensure
    RAYON_NUM_THREADS is respected (lazrs initializes thread pool at import time).
    """
    # lazrs reads RAYON_NUM_THREADS at library initialization, so we need
    # to run in a subprocess with the env var set before importing
    if backend == laspy.LazBackend.LazrsParallel and num_threads is not None:
        # Use subprocess to ensure fresh Python process with correct env var
        script = f"""
import os
os.environ['RAYON_NUM_THREADS'] = '{num_threads}'
import laspy
import time
import sys

file_path = r'{file}'
warmup = {warmup}
iterations = {iterations}

# Warmup
for _ in range(warmup):
    with laspy.open(file_path, laz_backend=laspy.LazBackend.LazrsParallel) as f:
        f.read()

# Timed iterations
for it in range(iterations):
    t0 = time.perf_counter()
    with laspy.open(file_path, laz_backend=laspy.LazBackend.LazrsParallel) as f:
        f.read()
    elapsed = time.perf_counter() - t0
    print(f"{{it}},{{elapsed}}", flush=True)
"""
        results: List[BenchResult] = []
        env = os.environ.copy()
        env["RAYON_NUM_THREADS"] = str(num_threads)

        proc = subprocess.run(
            [sys.executable, "-c", script],
            env=env,
            capture_output=True,
            text=True,
            cwd=os.getcwd(),
        )

        if proc.returncode != 0:
            raise RuntimeError(f"laspy subprocess failed: {proc.stderr}")

        for line in proc.stdout.strip().split('\n'):
            if ',' in line:
                it_str, time_str = line.split(',')
                results.append(BenchResult(backend_name, "read", num_threads, int(it_str), float(time_str)))

        return results
    else:
        # For non-lazrs backends or default thread count, run in-process
        results: List[BenchResult] = []
        old_rayon_threads = os.environ.get("RAYON_NUM_THREADS")
        try:
            if num_threads is not None and backend == laspy.LazBackend.LazrsParallel:
                os.environ["RAYON_NUM_THREADS"] = str(num_threads)

            for _ in range(warmup):
                _laspy_read(file, backend)
            for it in range(iterations):
                t = timed(lambda: _laspy_read(file, backend))
                results.append(BenchResult(backend_name, "read", num_threads or 0, it, t))
        finally:
            if old_rayon_threads is None:
                os.environ.pop("RAYON_NUM_THREADS", None)
            else:
                os.environ["RAYON_NUM_THREADS"] = old_rayon_threads

        return results


def _laspy_write(points, file: Path, backend, chunk_size: int) -> None:
    with laspy.LasWriter(
        str(file),
        header=points.header,
        laz_backend=backend,
        do_compress=True,
    ) as writer:
        writer.write_points(points.points)


def benchmark_laspy_write(
    file: Path,
    backend,
    backend_name: str,
    warmup: int,
    iterations: int,
    chunk_size: int,
    num_threads: Optional[int] = None,
) -> List[BenchResult]:
    # Pre-read once (not timed)
    with laspy.open(str(file), laz_backend=backend) as f:
        points = f.read()

    results: List[BenchResult] = []
    with tempfile.NamedTemporaryFile(suffix=".laz", delete=False) as tmp:
        tmp_path = Path(tmp.name)

    # lazrs (used by laspy) respects RAYON_NUM_THREADS environment variable
    old_rayon_threads = os.environ.get("RAYON_NUM_THREADS")
    try:
        if num_threads is not None:
            os.environ["RAYON_NUM_THREADS"] = str(num_threads)
        elif old_rayon_threads is None:
            # If not set, lazrs uses all available threads
            pass

        for _ in range(warmup):
            _laspy_write(points, tmp_path, backend, chunk_size)
        for it in range(iterations):
            t = timed(lambda: _laspy_write(points, tmp_path, backend, chunk_size))
            results.append(BenchResult(backend_name, "write", num_threads or 0, it, t))
    finally:
        if old_rayon_threads is None:
            os.environ.pop("RAYON_NUM_THREADS", None)
        else:
            os.environ["RAYON_NUM_THREADS"] = old_rayon_threads
        tmp_path.unlink(missing_ok=True)

    return results


# ── lazrs benchmarks ──────────────────────────────────────────────────────────

def benchmark_lazrs_read(
    file: Path,
    warmup: int,
    iterations: int,
) -> List[BenchResult]:
    """Benchmark the lazrs Python package directly (if available)."""
    if not _HAS_LAZRS:
        return []

    def _read() -> None:
        with open(file, "rb") as f:
            lazrs.decompress_selective_laz_into(  # type: ignore[attr-defined]
                f.read(), lazrs.SelectiveDecompression.All)

    # Fallback: lazrs API varies across versions; try a simple read
    def _read_fallback() -> None:
        data = file.read_bytes()
        lazrs.decompress_laz_buffer(data)  # type: ignore[attr-defined]

    read_fn = _read
    try:
        _read()
    except AttributeError:
        try:
            _read_fallback()
            read_fn = _read_fallback
        except Exception:
            return []
    except Exception:
        return []

    results: List[BenchResult] = []
    for _ in range(warmup):
        read_fn()
    for it in range(iterations):
        t = timed(read_fn)
        results.append(BenchResult("lazrs", "read", 0, it, t))
    return results


# ── PDAL benchmarks ────────────────────────────────────────────────────────────

def benchmark_pdal_read(
    file: Path,
    warmup: int,
    iterations: int,
) -> List[BenchResult]:
    """Time `pdal translate <input.laz> <output.las>` wall-clock."""
    if not _HAS_PDAL:
        return []

    with tempfile.NamedTemporaryFile(suffix=".las", delete=False) as tmp:
        out_path = Path(tmp.name)

    def _run() -> None:
        subprocess.run(
            ["pdal", "translate", str(file), str(out_path)],
            check=True,
            capture_output=True,
        )

    try:
        results: List[BenchResult] = []
        for _ in range(warmup):
            _run()
        for it in range(iterations):
            t = timed(_run)
            results.append(BenchResult("pdal", "read", 0, it, t))
    finally:
        out_path.unlink(missing_ok=True)

    return results


def benchmark_pdal_write(
    file: Path,
    warmup: int,
    iterations: int,
) -> List[BenchResult]:
    """Time `pdal translate <input.laz> <output.laz>` (compress) wall-clock."""
    if not _HAS_PDAL:
        return []

    with tempfile.NamedTemporaryFile(suffix=".laz", delete=False) as tmp:
        out_path = Path(tmp.name)

    def _run() -> None:
        subprocess.run(
            ["pdal", "translate", str(file), str(out_path)],
            check=True,
            capture_output=True,
        )

    try:
        results: List[BenchResult] = []
        for _ in range(warmup):
            _run()
        for it in range(iterations):
            t = timed(_run)
            results.append(BenchResult("pdal", "write", 0, it, t))
    finally:
        out_path.unlink(missing_ok=True)

    return results


# ── Aggregation ───────────────────────────────────────────────────────────────

def aggregate(
    raw: List[BenchResult],
    num_points: int,
    file_size_bytes: int,
) -> List[BenchSummary]:
    """Group raw timings by (tool, operation, threads) and compute stats."""
    groups: Dict[Tuple[str, str, int], List[float]] = {}
    for r in raw:
        key = (r.tool, r.operation, r.threads)
        groups.setdefault(key, []).append(r.time_s)

    summaries = []
    for (tool, op, threads), times in groups.items():
        summaries.append(BenchSummary(
            tool=tool,
            operation=op,
            threads=threads,
            n=len(times),
            mean_s=sum(times) / len(times),
            min_s=min(times),
            max_s=max(times),
            num_points=num_points,
            file_size_bytes=file_size_bytes,
        ))
    return summaries


# ── Display ───────────────────────────────────────────────────────────────────

_COL_TOOL = 26
_COL_THREADS = 8
_COL_MEAN = 9
_COL_MIN = 9
_COL_MAX = 9
_COL_PTS = 14
_COL_MB = 8


def _header_line() -> str:
    return (
        f"{'Tool':<{_COL_TOOL}}"
        f"{'Threads':>{_COL_THREADS}}"
        f"{'Mean(s)':>{_COL_MEAN}}"
        f"{'Min(s)':>{_COL_MIN}}"
        f"{'Max(s)':>{_COL_MAX}}"
        f"{'MPts/s':>{_COL_PTS}}"
        f"{'MB/s':>{_COL_MB}}"
    )


def _summary_line(s: BenchSummary) -> str:
    mpts = s.pts_per_s / 1e6
    return (
        f"{s.tool:<{_COL_TOOL}}"
        f"{s.thread_label:>{_COL_THREADS}}"
        f"{s.mean_s:>{_COL_MEAN}.3f}"
        f"{s.min_s:>{_COL_MIN}.3f}"
        f"{s.max_s:>{_COL_MAX}.3f}"
        f"{mpts:>{_COL_PTS}.2f}"
        f"{s.mb_per_s:>{_COL_MB}.1f}"
    )


def print_results(
    summaries: List[BenchSummary],
    metadata: dict,
    has_thread_control: bool,
) -> None:
    num_points = metadata.get("num_points", 0)
    file_size_mb = metadata.get("file_size_bytes", 0) / 1e6
    hw_threads = metadata.get("hardware_threads", multiprocessing.cpu_count())
    platform = metadata.get("platform", sys.platform)

    print()
    print("=" * 76)
    print("  LAS/LAZ Performance Benchmark")
    print("=" * 76)
    print(f"  File        : {metadata.get('file', '?')}")
    print(f"  Points      : {num_points:,}")
    print(f"  File size   : {file_size_mb:.1f} MB")
    print(f"  Point fmt   : {metadata.get('point_format', '?')}"
          f"  compressed={metadata.get('is_compressed', '?')}")
    print(f"  Platform    : {platform}  HW threads={hw_threads}")
    if not has_thread_control:
        print("  NOTE: Thread count control is not available on this platform.")
        print("        laspp results reflect the system default (all cores).")
    print()

    for op in ("read", "write"):
        op_results = [s for s in summaries if s.operation == op]
        if not op_results:
            continue

        print(f"  {'READ' if op == 'read' else 'WRITE'} PERFORMANCE")
        print("  " + "-" * 74)
        print("  " + _header_line())
        print("  " + "-" * 74)

        # Sort: laspp first (ascending threads), then alphabetical
        def sort_key(s: BenchSummary):
            return (0 if s.tool == "laspp" else 1, s.tool, s.threads)

        for s in sorted(op_results, key=sort_key):
            print("  " + _summary_line(s))

        print()


# ── Plotting ──────────────────────────────────────────────────────────────────

def plot_results(
    summaries: List[BenchSummary],
    metadata: dict,
    output_path: Optional[Path],
) -> None:
    if not _HAS_MATPLOTLIB:
        print("matplotlib not available – skipping plot.")
        return

    for op in ("read", "write"):
        op_results = [s for s in summaries if s.operation == op]
        if not op_results:
            continue

        fig, ax = plt.subplots(figsize=(12, 6))

        # Tools that support thread scaling (show as lines with markers)
        scaling_tools = ["laspp", "laspy (lazrs-parallel)"]
        colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
        tool_colors = {}

        # Plot thread-scaling tools as lines
        for tool in scaling_tools:
            tool_pts = sorted(
                [(s.threads, s.mb_per_s)
                 for s in op_results if s.tool == tool and s.threads > 0],
                key=lambda x: x[0],
            )
            if tool_pts:
                xs, ys = zip(*tool_pts)
                # Use consistent color for each tool
                if tool not in tool_colors:
                    tool_colors[tool] = colors[len(tool_colors) % len(colors)]
                col = tool_colors[tool]
                label = "LAS++" if tool == "laspp" else "lazrs (laspy)"
                ax.plot(xs, ys, "o-", label=label, linewidth=2, markersize=6, color=col)

        # Single-threaded tools as horizontal dashed lines
        single_threaded_tools = ["laszip", "lazperf"]

        for tool in single_threaded_tools:
            tool_results = [s for s in op_results if s.tool == tool]
            if not tool_results:
                continue
            # Get the single-threaded result
            single_thread_result = next((s for s in tool_results if s.threads <= 1), tool_results[0])
            if tool not in tool_colors:
                tool_colors[tool] = colors[len(tool_colors) % len(colors)]
            col = tool_colors[tool]
            ax.axhline(single_thread_result.mb_per_s, linestyle="--", color=col,
                       label=tool, linewidth=1.5)

        ax.set_xlabel("Thread count", fontsize=11)
        ax.set_ylabel("Throughput (MB/s – compressed file size)", fontsize=11)
        ax.set_title(
            f"LAZ {op.capitalize()} Throughput Comparison  –  "
            f"{Path(metadata.get('file', '')).name}",
            fontsize=12
        )
        ax.legend(loc='best', fontsize=9)
        ax.xaxis.set_major_locator(mticker.MaxNLocator(integer=True))
        ax.grid(True, alpha=0.3)
        fig.tight_layout()

        if output_path:
            stem = output_path.stem
            suffix = output_path.suffix or ".png"
            out = output_path.with_name(f"{stem}_{op}{suffix}")
            fig.savefig(out, dpi=150)
            print(f"Plot saved: {out}")
        else:
            plt.show()

        plt.close(fig)


# ── main ──────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    hw = multiprocessing.cpu_count()
    default_threads = ",".join(
        str(t) for t in
        sorted({1, 2, 4, hw} | {2 ** k for k in range(int(math.log2(hw)) + 1)
                                  if 2 ** k <= hw})
        if t >= 1
    )

    p = argparse.ArgumentParser(
        description="Compare LAS/LAZ read/write performance across tools."
    )
    p.add_argument("--file", "-f", required=True,
                   help="Input .las or .laz file to benchmark")
    p.add_argument("--threads", "-t", default=default_threads,
                   help=f"Comma-separated thread counts for LAS++ "
                        f"(default: {default_threads})")
    p.add_argument("--iterations", "-n", type=int, default=3,
                   help="Timed iterations per configuration (default: 3)")
    p.add_argument("--warmup", "-w", type=int, default=1,
                   help="Warm-up iterations (untimed, default: 1)")
    p.add_argument("--operation", choices=["read", "write", "both"],
                   default="read",
                   help="Operation to benchmark (default: read)")
    p.add_argument("--chunk-size", type=int, default=50000,
                   help="Points per LAZ chunk for write benchmarks "
                        "(default: 50000)")
    p.add_argument("--benchmark-binary",
                   help="Path to the C++ benchmark binary")
    p.add_argument("--tools",
                   default="laspp,laszip,laspy,lazrs,pdal,lazperf",
                   help="Comma-separated list of tools to benchmark")
    p.add_argument("--plot", action="store_true",
                   help="Generate a throughput plot (requires matplotlib)")
    p.add_argument("--plot-output",
                   help="Save plot to this path instead of displaying it")
    p.add_argument("--output", "-o",
                   help="Save aggregated results as JSON to this path")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    file = Path(args.file)
    if not file.exists():
        print(f"Error: file not found: {file}", file=sys.stderr)
        return 1

    tools = {t.strip() for t in args.tools.split(",")}
    thread_counts = [int(t) for t in args.threads.split(",")]
    do_read = args.operation in ("read", "both")
    do_write = args.operation in ("write", "both")
    op = args.operation

    all_results: List[BenchResult] = []
    metadata: dict = {
        "file": str(file),
        "num_points": 0,
        "file_size_bytes": file.stat().st_size,
        "hardware_threads": multiprocessing.cpu_count(),
        "platform": sys.platform,
        "has_thread_control": False,
    }

    # ── C++ benchmark (laspp + laszip) ────────────────────────────────────────
    needs_cpp = bool(tools & {"laspp", "laszip"})
    binary: Optional[Path] = None
    if needs_cpp:
        binary = find_benchmark_binary(args.benchmark_binary)
        if binary is None:
            print(
                "WARNING: C++ benchmark binary not found. "
                "Skipping laspp and laszip.\n"
                "Build the project first, or pass --benchmark-binary <path>.",
                file=sys.stderr,
            )
        else:
            print(f"Using C++ benchmark binary: {binary}")
            include_laszip = "laszip" in tools
            include_laspp_threads = thread_counts if "laspp" in tools else [1]
            try:
                meta, cpp_results = run_cpp_benchmark(
                    binary=binary,
                    file=file,
                    thread_counts=include_laspp_threads,
                    iterations=args.iterations,
                    warmup=args.warmup,
                    operation=op,
                    chunk_size=args.chunk_size if do_write else None,
                    include_laszip=include_laszip,
                )
                metadata.update(meta)
                all_results.extend(cpp_results)
            except Exception as e:
                print(f"WARNING: C++ benchmark failed: {e}", file=sys.stderr)

    # ── laspy benchmarks ──────────────────────────────────────────────────────
    laspy_backends = detect_laspy_backends() if _HAS_LASPY else {}

    if "laspy" in tools and _HAS_LASPY:
        for backend_key, backend_enum, label in [
            ("laspy_lazrs_parallel", laspy.LazBackend.LazrsParallel,
             "laspy (lazrs-parallel)"),
            ("laspy_laszip", laspy.LazBackend.Laszip, "laspy (laszip)"),
        ]:
            if not laspy_backends.get(backend_key, False):
                continue
            print(f"Benchmarking {label}...")
            try:
                # lazrs-parallel respects RAYON_NUM_THREADS, so test with different thread counts
                # laszip backend is single-threaded, so only test once
                laspy_thread_counts = thread_counts if backend_key == "laspy_lazrs_parallel" else [1]

                for n_threads in laspy_thread_counts:
                    if do_read:
                        res = benchmark_laspy_read(
                            file, backend_enum, label,
                            args.warmup, args.iterations, n_threads,
                        )
                        all_results.extend(res)
                    if do_write:
                        res = benchmark_laspy_write(
                            file, backend_enum, label,
                            args.warmup, args.iterations, args.chunk_size, n_threads,
                        )
                        all_results.extend(res)
            except Exception as e:
                print(f"WARNING: {label} benchmark failed: {e}",
                      file=sys.stderr)
    elif "laspy" in tools:
        print("laspy not installed – skipping laspy benchmarks.")

    # ── lazrs direct benchmarks ───────────────────────────────────────────────
    if "lazrs" in tools and _HAS_LAZRS:
        print("Benchmarking lazrs (direct)...")
        try:
            if do_read:
                res = benchmark_lazrs_read(file, args.warmup, args.iterations)
                all_results.extend(res)
        except Exception as e:
            print(f"WARNING: lazrs benchmark failed: {e}", file=sys.stderr)
    elif "lazrs" in tools and not _HAS_LASPY:
        print("lazrs not installed – skipping lazrs benchmarks.")

    # ── PDAL benchmarks ───────────────────────────────────────────────────────
    if "pdal" in tools and _HAS_PDAL:
        print("Benchmarking pdal...")
        try:
            if do_read:
                res = benchmark_pdal_read(file, args.warmup, args.iterations)
                all_results.extend(res)
            if do_write:
                res = benchmark_pdal_write(file, args.warmup, args.iterations)
                all_results.extend(res)
        except Exception as e:
            print(f"WARNING: PDAL benchmark failed: {e}", file=sys.stderr)
    elif "pdal" in tools:
        print("pdal not found on PATH – skipping PDAL benchmarks.")

    # ── Aggregate & display ───────────────────────────────────────────────────
    if not all_results:
        print("No benchmark results collected.", file=sys.stderr)
        return 1

    num_points = metadata.get("num_points", 0)
    file_size_bytes = metadata.get("file_size_bytes", file.stat().st_size)
    has_thread_control = bool(metadata.get("has_thread_control", False))

    summaries = aggregate(all_results, num_points, file_size_bytes)
    print_results(summaries, metadata, has_thread_control)

    # ── Optional JSON output ──────────────────────────────────────────────────
    if args.output:
        out_path = Path(args.output)
        payload = {
            "metadata": metadata,
            "summaries": [
                {
                    "tool": s.tool,
                    "operation": s.operation,
                    "threads": s.threads,
                    "mean_s": s.mean_s,
                    "min_s": s.min_s,
                    "max_s": s.max_s,
                    "pts_per_s": s.pts_per_s,
                    "mb_per_s": s.mb_per_s,
                }
                for s in summaries
            ],
            "raw": [
                {
                    "tool": r.tool,
                    "operation": r.operation,
                    "threads": r.threads,
                    "iteration": r.iteration,
                    "time_s": r.time_s,
                }
                for r in all_results
            ],
        }
        out_path.write_text(json.dumps(payload, indent=2))
        print(f"Results saved to: {out_path}")

    # ── Optional plot ─────────────────────────────────────────────────────────
    if args.plot or args.plot_output:
        plot_output = Path(args.plot_output) if args.plot_output else None
        plot_results(summaries, metadata, plot_output)

    return 0


if __name__ == "__main__":
    sys.exit(main())
