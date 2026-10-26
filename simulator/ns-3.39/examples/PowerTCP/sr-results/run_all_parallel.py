#!/usr/bin/env python3
"""Parallel multi-seed SR comparison runner.

Each (config, mode, K, seed) triple becomes one ns3 invocation with a unique
fct output path so they can run concurrently. We then aggregate per
(config, mode, K) across seeds and write the same CSVs as run_all_multiseed.py.
"""
import csv
import os
import subprocess
import sys
import statistics
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed
import multiprocessing

NS3_ROOT = Path(__file__).resolve().parents[3]   # .../ns-3.39
EX_DIR   = NS3_ROOT / "examples" / "PowerTCP"
PAR_DIR  = EX_DIR / "mix" / "parallel"
RESULTS  = EX_DIR / "sr-results"
SIM_SEC  = 0.05
NS3      = str(NS3_ROOT / "ns3")
ALG_DCTCP = 8

N_SEEDS_EXP1 = 5
N_SEEDS_EXP2 = 5
N_SEEDS_EXP3 = 3
SEEDS = list(range(1, max(N_SEEDS_EXP1, N_SEEDS_EXP2, N_SEEDS_EXP3) + 1))

# BDP cross-pod (28us RTT)
BDP = {"25Gb": 88, "100Gb": 350}
K_BASE          = [4, 8, 16, 32, 64, 128]
K_IRN_EXTRA     = {"25Gb": [BDP["25Gb"], 5 * BDP["25Gb"]],
                   "100Gb": [BDP["100Gb"], 5 * BDP["100Gb"]]}

MODE_TO_SR = {"RaLL": 1, "SRNIC": 2, "IRN": 3, "CX6": 4}
MODE_ORDER = {"RaLL": 0, "IRN": 1, "SRNIC": 2, "CX6": 3}

PAR_DIR.mkdir(parents=True, exist_ok=True)

def conf_loss(topo, rate):
    return EX_DIR / (f"config-100g-loss-{rate}.txt" if topo == "100Gb"
                     else f"config-loss-{rate}.txt")

def conf_incast(topo, n):
    return EX_DIR / (f"config-100g-incast-{n}.txt" if topo == "100Gb"
                     else f"config-incast-{n}.txt")

def cli(conf, mode, K, fct_out, loss_model=0, burst_min=2, burst_max=8,
        rall_async=500, srnic_async=1000, seed=1):
    sr_mode = MODE_TO_SR[mode]
    return (
        f"powertcp-evaluation-burst "
        f"--conf={conf} --algorithm={ALG_DCTCP} --windowCheck=1 "
        f"--srMode={sr_mode} --srK={K} "
        f"--rallAsyncNs={rall_async} --srnicAsyncNs={srnic_async} "
        f"--lossModel={loss_model} "
        f"--burstSizeMin={burst_min} --burstSizeMax={burst_max} "
        f"--RngRun={seed} --fctOut={fct_out}"
    )

def metrics(fct_path):
    flows, sum_fct, sum_b, max_fct = 0, 0, 0, 0
    if not os.path.exists(fct_path):
        return 0, 0.0, 0, 0.0
    with open(fct_path) as f:
        for line in f:
            p = line.split()
            if len(p) < 7:
                continue
            sz, fct = int(p[4]), int(p[6])
            flows += 1
            sum_fct += fct
            sum_b += sz
            if fct > max_fct:
                max_fct = fct
    if flows == 0:
        return 0, 0.0, 0, 0.0
    return (flows,
            round(sum_fct / 1000.0 / flows, 1),
            int(sum_b * 8 / (SIM_SEC * 1e6)),
            round(max_fct / 1000.0, 1))

def run_one(args):
    """Worker: run a single ns3 invocation and return (key, metrics)."""
    key, conf, mode, K, seed, loss_model, burst_min, burst_max = args
    fct_path = PAR_DIR / f"fct_{os.getpid()}_{key.replace('|','_')}_s{seed}.txt"
    cmd = cli(conf, mode, K, str(fct_path),
              loss_model=loss_model, burst_min=burst_min, burst_max=burst_max,
              seed=seed)
    res = subprocess.run([NS3, "run", "--quiet", cmd],
                         cwd=NS3_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(f"[FAIL {key} seed={seed}] {res.stderr[-300:]}\n")
        return (key, seed, None)
    m = metrics(str(fct_path))
    try:
        os.remove(fct_path)
    except OSError:
        pass
    return (key, seed, m)

def aggregate(values):
    if not values:
        return 0, 0
    return (statistics.mean(values),
            statistics.stdev(values) if len(values) > 1 else 0)

def already(rows, keys, vals):
    return any(all(str(r[k]) == str(v) for k, v in zip(keys, vals))
               for r in rows)

def write_csv(path, rows, fieldnames, sort_key):
    rows.sort(key=sort_key)
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(rows)

def gather_jobs():
    """Build the full list of jobs to run."""
    jobs = []   # (job_args, output_spec)

    # exp1: loss-rate sweep (independent)
    losses = ["0.0", "0.0001", "0.001", "0.01"]
    for topo in ["25Gb", "100Gb"]:
        for rate in losses:
            cf = str(conf_loss(topo, rate))
            for mode in ["RaLL", "IRN", "SRNIC", "CX6"]:
                Ks = K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])
                for K in Ks:
                    key = f"exp1|{topo}|{rate}|{mode}|{K}"
                    for seed in SEEDS[:N_SEEDS_EXP1]:
                        jobs.append((key, cf, mode, K, seed, 0, 2, 8))

    # exp2: loss model comparison (independent vs burst)
    for topo in ["25Gb", "100Gb"]:
        cf = str(conf_loss(topo, "0.01"))
        for loss_model, label in [(0, "Independent"), (1, "Burst")]:
            for mode in ["RaLL", "IRN", "SRNIC", "CX6"]:
                Ks = K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])
                for K in Ks:
                    key = f"exp2|{topo}|{label}|{mode}|{K}"
                    for seed in SEEDS[:N_SEEDS_EXP2]:
                        jobs.append((key, cf, mode, K, seed, loss_model, 2, 8))

    # exp3: incast scaling
    incasts = [8, 16, 32, 64]
    for topo in ["25Gb", "100Gb"]:
        for n in incasts:
            cf = str(conf_incast(topo, n))
            for mode in ["RaLL", "IRN", "SRNIC", "CX6"]:
                Ks = K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])
                for K in Ks:
                    key = f"exp3|{topo}|{n}|{mode}|{K}"
                    for seed in SEEDS[:N_SEEDS_EXP3]:
                        jobs.append((key, cf, mode, K, seed, 0, 2, 8))

    return jobs

def write_results(by_key):
    """Demultiplex aggregated by_key dict into the three CSVs."""
    exp1_rows, exp2_rows, exp3_rows = [], [], []
    for key, seed_metrics in by_key.items():
        parts = key.split("|")
        valid = [m for s, m in seed_metrics.items() if m is not None]
        if not valid:
            continue
        flows = [m[0] for m in valid]
        fcts  = [m[1] for m in valid]
        mbps  = [m[2] for m in valid]
        max_fcts = [m[3] for m in valid]
        flows_mean, flows_std = aggregate(flows)
        fct_mean, _ = aggregate(fcts)
        mbps_mean, _ = aggregate(mbps)
        max_fct_mean, _ = aggregate(max_fcts)

        if parts[0] == "exp1":
            _, topo, rate, mode, K = parts
            exp1_rows.append(dict(
                topology=topo, loss_rate=rate, K=int(K), mode=mode,
                flows_mean=round(flows_mean, 1),
                flows_std=round(flows_std, 2),
                avg_fct_us=round(fct_mean, 1),
                agg_mbps=int(mbps_mean)))
        elif parts[0] == "exp2":
            _, topo, label, mode, K = parts
            exp2_rows.append(dict(
                topology=topo, loss_model=label, K=int(K), mode=mode,
                flows_mean=round(flows_mean, 1),
                flows_std=round(flows_std, 2),
                avg_fct_us=round(fct_mean, 1),
                agg_mbps=int(mbps_mean)))
        elif parts[0] == "exp3":
            _, topo, n, mode, K = parts
            exp3_rows.append(dict(
                topology=topo, incast_n=int(n), K=int(K), mode=mode,
                flows_mean=round(flows_mean, 1),
                flows_std=round(flows_std, 2),
                avg_fct_us=round(fct_mean, 1),
                agg_mbps=int(mbps_mean),
                max_fct_us=round(max_fct_mean, 1)))

    write_csv(RESULTS / "exp1_loss_vs_K.csv", exp1_rows,
              ["topology","loss_rate","K","mode","flows_mean","flows_std","avg_fct_us","agg_mbps"],
              lambda r: (r["topology"], float(r["loss_rate"]), int(r["K"]), MODE_ORDER[r["mode"]]))
    write_csv(RESULTS / "exp2_loss_model.csv", exp2_rows,
              ["topology","loss_model","K","mode","flows_mean","flows_std","avg_fct_us","agg_mbps"],
              lambda r: (r["topology"], r["loss_model"], int(r["K"]), MODE_ORDER[r["mode"]]))
    write_csv(RESULTS / "exp3_incast.csv", exp3_rows,
              ["topology","incast_n","K","mode","flows_mean","flows_std","avg_fct_us","agg_mbps","max_fct_us"],
              lambda r: (r["topology"], int(r["incast_n"]), int(r["K"]), MODE_ORDER[r["mode"]]))

def main():
    nworkers = int(sys.argv[1]) if len(sys.argv) > 1 else min(32, multiprocessing.cpu_count())
    jobs = gather_jobs()
    print(f"Total jobs: {len(jobs)}, workers: {nworkers}", flush=True)

    by_key = {}  # key -> {seed: metrics}
    done = 0
    failed = 0
    with ProcessPoolExecutor(max_workers=nworkers) as ex:
        futs = {ex.submit(run_one, j): j for j in jobs}
        for fut in as_completed(futs):
            key, seed, m = fut.result()
            by_key.setdefault(key, {})[seed] = m
            done += 1
            if m is None:
                failed += 1
            if done % 50 == 0 or done == len(jobs):
                pct = done * 100 // len(jobs)
                print(f"[{done}/{len(jobs)} {pct}%] failed={failed}", flush=True)
                # incremental write so we have partial data if interrupted
                write_results(by_key)

    write_results(by_key)
    print(f"DONE: {done}/{len(jobs)} (failed={failed})", flush=True)

if __name__ == "__main__":
    main()
