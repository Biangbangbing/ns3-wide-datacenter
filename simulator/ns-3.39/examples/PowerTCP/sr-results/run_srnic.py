#!/usr/bin/env python3
"""Drive SRNIC sweeps that mirror exp1/exp2/exp3 and merge into the
existing comparison CSVs.

Usage (from ns-3.39):
    python3 examples/PowerTCP/sr-results/run_srnic.py
"""
import csv
import os
import subprocess
import sys
from pathlib import Path

NS3_ROOT = Path(__file__).resolve().parents[3]   # .../ns-3.39
EX_DIR   = NS3_ROOT / "examples" / "PowerTCP"
FCT      = EX_DIR / "mix" / "fct.txt"
RESULTS  = EX_DIR / "sr-results"
SIM_SEC  = 0.05                                  # SIMULATOR_STOP_TIME (s)

NS3      = str(NS3_ROOT / "ns3")

ALG_DCTCP = 8


def run(cmd):
    """Run an ns-3 command, raise on failure."""
    res = subprocess.run(cmd, cwd=NS3_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stdout[-2000:] + "\n" + res.stderr[-2000:])
        raise RuntimeError(f"ns-3 returned {res.returncode}")


def metrics():
    """Parse fct.txt -> (flows, avg_fct_us, agg_mbps, max_fct_us)."""
    flows = 0
    sum_fct_ns = 0
    sum_bytes  = 0
    max_fct_ns = 0
    with open(FCT) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 7:
                continue
            size_b   = int(parts[4])
            fct_ns   = int(parts[6])
            flows     += 1
            sum_fct_ns += fct_ns
            sum_bytes  += size_b
            if fct_ns > max_fct_ns:
                max_fct_ns = fct_ns
    if flows == 0:
        return 0, 0.0, 0, 0.0
    avg_fct_us = sum_fct_ns / 1000.0 / flows
    agg_mbps   = int(sum_bytes * 8 / (SIM_SEC * 1e6))
    max_fct_us = max_fct_ns / 1000.0
    return flows, round(avg_fct_us, 1), agg_mbps, round(max_fct_us, 1)


def srnic_cli(conf, K, loss_model=0, burst_min=2, burst_max=8,
              srnic_async_ns=1000):
    """Build the ns-3 invocation list."""
    args = [
        f"powertcp-evaluation-burst "
        f"--conf={conf} --algorithm={ALG_DCTCP} --windowCheck=1 "
        f"--srMode=2 --srK={K} --srnicAsyncNs={srnic_async_ns} "
        f"--lossModel={loss_model} "
        f"--burstSizeMin={burst_min} --burstSizeMax={burst_max}"
    ]
    return [NS3, "run", "--quiet", *args]


def already(rows, key_cols, key_vals):
    return any(all(str(r[c]) == str(v) for c, v in zip(key_cols, key_vals))
               for r in rows)


# ---------------------------------------------------------------- exp1
def do_exp1():
    csv_path = RESULTS / "exp1_loss_vs_K.csv"
    rows = list(csv.DictReader(open(csv_path)))
    fieldnames = rows[0].keys() if rows else \
        ["loss_rate", "K", "mode", "flows", "avg_fct_us", "agg_mbps"]
    losses = ["0.0001", "0.001", "0.01"]
    Ks     = [4, 8, 16, 32, 64]
    for rate in losses:
        conf = EX_DIR / f"config-loss-{rate}.txt"
        for K in Ks:
            if already(rows, ["loss_rate", "K", "mode"], [rate, K, "SRNIC"]):
                continue
            print(f"[exp1] SRNIC loss={rate} K={K}", flush=True)
            run(srnic_cli(str(conf), K, loss_model=0))
            f, fct, mb, _ = metrics()
            rows.append(dict(loss_rate=rate, K=K, mode="SRNIC",
                             flows=f, avg_fct_us=fct, agg_mbps=mb))
    rows.sort(key=lambda r: (float(r["loss_rate"]), int(r["K"]),
                             {"RaLL": 0, "IRN": 1, "SRNIC": 2}[r["mode"]]))
    with open(csv_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(fieldnames))
        w.writeheader()
        w.writerows(rows)


# ---------------------------------------------------------------- exp2
def do_exp2():
    csv_path = RESULTS / "exp2_loss_model.csv"
    rows = list(csv.DictReader(open(csv_path)))
    fieldnames = rows[0].keys() if rows else \
        ["loss_model", "K", "mode", "flows", "avg_fct_us", "agg_mbps"]
    Ks = [16, 32, 64, 128, 256]
    conf = EX_DIR / "config-loss-0.01.txt"
    for K in Ks:
        for lm_name, lm in [("independent", 0), ("burst", 1)]:
            if already(rows, ["loss_model", "K", "mode"],
                       [lm_name, K, "SRNIC"]):
                continue
            print(f"[exp2] SRNIC loss_model={lm_name} K={K}", flush=True)
            run(srnic_cli(str(conf), K, loss_model=lm))
            f, fct, mb, _ = metrics()
            rows.append(dict(loss_model=lm_name, K=K, mode="SRNIC",
                             flows=f, avg_fct_us=fct, agg_mbps=mb))
    rows.sort(key=lambda r: (r["loss_model"], int(r["K"]),
                             {"RaLL": 0, "IRN": 1, "SRNIC": 2}[r["mode"]]))
    with open(csv_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(fieldnames))
        w.writeheader()
        w.writerows(rows)


# ---------------------------------------------------------------- exp3
def do_exp3():
    csv_path = RESULTS / "exp3_incast.csv"
    rows = list(csv.DictReader(open(csv_path)))
    fieldnames = rows[0].keys() if rows else \
        ["incast_n", "K", "mode", "flows", "avg_fct_us", "agg_mbps", "max_fct_us"]
    Ks = [8, 16, 32, 64]
    Ns = [8, 16, 32, 64]
    for N in Ns:
        conf = EX_DIR / f"config-incast-{N}.txt"
        for K in Ks:
            if already(rows, ["incast_n", "K", "mode"], [N, K, "SRNIC"]):
                continue
            print(f"[exp3] SRNIC N={N} K={K}", flush=True)
            run(srnic_cli(str(conf), K, loss_model=0))
            f, fct, mb, mx = metrics()
            rows.append(dict(incast_n=N, K=K, mode="SRNIC",
                             flows=f, avg_fct_us=fct, agg_mbps=mb,
                             max_fct_us=mx))
    rows.sort(key=lambda r: (int(r["incast_n"]), int(r["K"]),
                             {"RaLL": 0, "IRN": 1, "SRNIC": 2}[r["mode"]]))
    with open(csv_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(fieldnames))
        w.writeheader()
        w.writerows(rows)


if __name__ == "__main__":
    which = sys.argv[1:] or ["1", "2", "3"]
    if "1" in which: do_exp1()
    if "2" in which: do_exp2()
    if "3" in which: do_exp3()
    print("done.")
