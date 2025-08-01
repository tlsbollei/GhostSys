#!/usr/bin/env python3
from __future__ import annotations
import argparse
import json
import math
import random
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

@dataclass
class CalibConfig:
    samples: int = 20000
    warmup: int = 2000
    buf_mb: int = 64
    stride: int = 512 * 1024
    seed: int = 1337
    outfile: Path = Path("config/spectre_threshold.json")
    csvfile: Optional[Path] = None

@dataclass
class CalibResult:
    hit_ns: List[int]
    miss_ns: List[int]
    hit_p50: float
    hit_p90: float
    miss_p50: float
    miss_p10: float
    threshold_ns: float

def _now_ns() -> int:
    return time.perf_counter_ns()

def _touch(memory: memoryview, idx: int) -> int:
    return memory[idx]

def _measure_hits(mem: memoryview, index: int, samples: int) -> List[int]:
    out: List[int] = []
    for _ in range(samples):
        t0 = _now_ns()
        v = _touch(mem, index)
        t1 = _now_ns()
        _ = v
        out.append(t1 - t0)
    return out

def _measure_misses(mem: memoryview, buf_len: int, stride: int, samples: int) -> List[int]:
    out: List[int] = []
    idx = 0
    for _ in range(samples):
        idx = (idx + stride) % buf_len
        t0 = _now_ns()
        v = _touch(mem, idx)
        t1 = _now_ns()
        _ = v
        out.append(t1 - t0)
    return out

def _warmup(mem: memoryview, rounds: int, step: int) -> None:
    L = len(mem)
    idx = 0
    for _ in range(rounds):
        idx = (idx + step) % L
        _ = mem[idx]

def _percentile(xs: List[int], p: float) -> float:
    if not xs:
        return float("nan")
    ys = sorted(xs)
    if p <= 0:
        return float(ys[0])
    if p >= 100:
        return float(ys[-1])
    k = (p / 100.0) * (len(ys) - 1)
    lo = math.floor(k)
    hi = math.ceil(k)
    if lo == hi:
        return float(ys[int(k)])
    frac = k - lo
    return ys[lo] * (1.0 - frac) + ys[hi] * frac

def _choose_threshold(hit_ns: List[int], miss_ns: List[int]) -> float:
    hit_p90 = _percentile(hit_ns, 90.0)
    miss_p10 = _percentile(miss_ns, 10.0)
    return (hit_p90 + miss_p10) / 2.0

def calibrate(cfg: CalibConfig) -> CalibResult:
    random.seed(cfg.seed)
    buf_bytes = cfg.buf_mb * 1024 * 1024
    backing = bytearray(buf_bytes)
    mem = memoryview(backing)
    _warmup(mem, rounds=max(4096, cfg.warmup), step=4096)
    hit_index = 1234 % len(mem)
    for _ in range(2048):
        _ = mem[hit_index]
    hit_ns = _measure_hits(mem, hit_index, cfg.samples)
    miss_ns = _measure_misses(mem, len(mem), cfg.stride, cfg.samples)
    hit_p50 = _percentile(hit_ns, 50.0)
    hit_p90 = _percentile(hit_ns, 90.0)
    miss_p50 = _percentile(miss_ns, 50.0)
    miss_p10 = _percentile(miss_ns, 10.0)
    threshold = _choose_threshold(hit_ns, miss_ns)
    return CalibResult(
        hit_ns=hit_ns,
        miss_ns=miss_ns,
        hit_p50=hit_p50,
        hit_p90=hit_p90,
        miss_p50=miss_p50,
        miss_p10=miss_p10,
        threshold_ns=threshold,
    )

def _save_json(cfg: CalibConfig, res: CalibResult) -> None:
    cfg.outfile.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "meta": {
            "tool": "calibrate_spectre.py",
            "version": "1.0",
            "timestamp_ns": _now_ns(),
            "python": sys.version.split()[0],
            "platform": sys.platform,
            "seed": cfg.seed,
            "params": {
                "samples": cfg.samples,
                "warmup": cfg.warmup,
                "buf_mb": cfg.buf_mb,
                "stride": cfg.stride,
            },
        },
        "results": {
            "hit_ns_p50": res.hit_p50,
            "hit_ns_p90": res.hit_p90,
            "miss_ns_p10": res.miss_p10,
            "miss_ns_p50": res.miss_p50,
            "threshold_ns": res.threshold_ns,
        },
        "advice": {"use_threshold_ns": res.threshold_ns},
    }
    with open(cfg.outfile, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)

def _save_csv(csv_path: Path, hit_ns: List[int], miss_ns: List[int]) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with open(csv_path, "w", encoding="utf-8") as f:
        f.write("sample,hit_ns,miss_ns\n")
        for i in range(max(len(hit_ns), len(miss_ns))):
            h = str(hit_ns[i]) if i < len(hit_ns) else ""
            m = str(miss_ns[i]) if i < len(miss_ns) else ""
            f.write(f"{i},{h},{m}\n")

def parse_args(argv: List[str]) -> CalibConfig:
    p = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("--samples", type=int, default=20000)
    p.add_argument("--warmup", type=int, default=2000)
    p.add_argument("--buf-mb", type=int, default=64)
    p.add_argument("--stride", type=int, default=512 * 1024)
    p.add_argument("--seed", type=int, default=1337)
    p.add_argument("--outfile", type=Path, default=Path("config/spectre_threshold.json"))
    p.add_argument("--csv", dest="csvfile", type=Path, default=None)
    args = p.parse_args(argv)
    return CalibConfig(
        samples=args.samples,
        warmup=args.warmup,
        buf_mb=args.buf_mb,
        stride=args.stride,
        seed=args.seed,
        outfile=args.outfile,
        csvfile=args.csvfile,
    )

def main(argv: List[str]) -> int:
    cfg = parse_args(argv)
    print(f"samples={cfg.samples} warmup={cfg.warmup} buf={cfg.buf_mb}MiB stride={cfg.stride} seed={cfg.seed}")
    res = calibrate(cfg)
    print(f"hit_p50={res.hit_p50:.1f}ns hit_p90={res.hit_p90:.1f}ns miss_p10={res.miss_p10:.1f}ns miss_p50={res.miss_p50:.1f}ns")
    print(f"threshold_ns={res.threshold_ns:.1f}")
    _save_json(cfg, res)
    print(f"json={cfg.outfile}")
    if cfg.csvfile:
        _save_csv(cfg.csvfile, res.hit_ns, res.miss_ns)
        print(f"csv={cfg.csvfile}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
