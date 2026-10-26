#!/usr/bin/env python3
"""Run the full SR comparison matrix with multiple seeds for statistical stability.

Each configuration is run N_SEEDS times with different random seeds, and results
are aggregated (mean ± std).

Usage (from ns-3.39):
    python3 examples/PowerTCP/sr-results/run_all_multiseed.py [1|2|3 ...]
"""
import csv
import os
import subprocess
import sys
from pathlib import Path
import statistics

NS3_ROOT = Path(__file__).resolve().parents[3]   # .../ns-3.39
EX_DIR   = NS3_ROOT / "examples" / "PowerTCP"
FCT      = EX_DIR / "mix" / "fct.txt"
RESULTS  = EX_DIR / "sr-results"
SIM_SEC  = 0.05
NS3      = str(NS3_ROOT / "ns3")
ALG_DCTCP = 8

# Number of seeds per configuration
N_SEEDS_EXP1 = 5
N_SEEDS_EXP2 = 5
N_SEEDS_EXP3 = 3
SEEDS = list(range(1, max(N_SEEDS_EXP1, N_SEEDS_EXP2, N_SEEDS_EXP3) + 1))

# BDP in 1KB packets. The simulator's printed maxRtt is ~28us in this 3-tier
# topology because the Agg<->Spine links are 5us each (host->ToR=1us,
# ToR->Agg=1us, Agg->Spine=5us; cross-pod RTT = 4*1us + 2*5us = 14us one-way,
# 28us round-trip). With cross-pod traffic (flow-64node-crosspod.txt):
#   25Gb  : 25e9 * 28e-6 / 8 / 1024 ~= 85 packets
#   100Gb : 100e9 * 28e-6 / 8 / 1024 ~= 342 packets
# We round to nice numbers below.
BDP = {"25Gb": 88, "100Gb": 350}
# Standard K sweep + IRN-paper recommended K (K=BDP and K=5xBDP)
K_BASE          = [4, 8, 16, 32, 64, 128]
K_IRN_EXTRA     = {"25Gb": [BDP["25Gb"], 5 * BDP["25Gb"]],
                   "100Gb": [BDP["100Gb"], 5 * BDP["100Gb"]]}

MODE_TO_SR = {"RaLL": 1, "SRNIC": 2, "IRN": 3}
MODE_ORDER = {"RaLL": 0, "IRN": 1, "SRNIC": 2}

# Configuration files (already exist in the repo)
def conf_loss(topo, rate):
    return EX_DIR / (f"config-100g-loss-{rate}.txt" if topo == "100Gb"
                     else f"config-loss-{rate}.txt")

def conf_incast(topo, n):
    return EX_DIR / (f"config-100g-incast-{n}.txt" if topo == "100Gb"
                     else f"config-incast-{n}.txt")


def run(args):
    res = subprocess.run([NS3, "run", "--quiet", args],
                         cwd=NS3_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stdout[-1500:] + "\n" + res.stderr[-1500:])
        raise RuntimeError("ns-3 failed")


def metrics():
    flows, sum_fct, sum_b, max_fct = 0, 0, 0, 0
    with open(FCT) as f:
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


def cli(conf, mode, K, loss_model=0, burst_min=2, burst_max=8,
        rall_async=500, srnic_async=1000, seed=1):
    sr_mode = MODE_TO_SR[mode]
    return (
        f"powertcp-evaluation-burst "
        f"--conf={conf} --algorithm={ALG_DCTCP} --windowCheck=1 "
        f"--srMode={sr_mode} --srK={K} "
        f"--rallAsyncNs={rall_async} --srnicAsyncNs={srnic_async} "
        f"--lossModel={loss_model} "
        f"--burstSizeMin={burst_min} --burstSizeMax={burst_max} "
        f"--RngRun={seed}"
    )


def aggregate_stats(values):
    """Compute mean and std from list of values"""
    if not values:
        return 0, 0
    mean = statistics.mean(values)
    std = statistics.stdev(values) if len(values) > 1 else 0
    return mean, std


def already(rows, keys, vals):
    return any(all(str(r[k]) == str(v) for k, v in zip(keys, vals))
               for r in rows)


def write_csv(path, rows, fieldnames, sort_key):
    rows.sort(key=sort_key)
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(rows)


# ---------------------------------------------------------------- exp1
def do_exp1():
    p = RESULTS / "exp1_loss_vs_K.csv"
    rows = list(csv.DictReader(open(p))) if p.exists() else []
    fieldnames = ["topology", "loss_rate", "K", "mode",
                  "flows_mean", "flows_std", "avg_fct_us", "agg_mbps"]
    losses = ["0.0", "0.0001", "0.001", "0.01"]

    for topo in ["25Gb", "100Gb"]:
        for rate in losses:
            cf = str(conf_loss(topo, rate))
            for mode in ["RaLL", "IRN", "SRNIC"]:
                Ks = K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])
                for K in Ks:
                    if already(rows, ["topology", "loss_rate", "K", "mode"],
                               [topo, rate, K, mode]):
                        continue

                    # Run N_SEEDS_EXP1 times
                    flows_list, fct_list, mbps_list = [], [], []
                    for seed in SEEDS[:N_SEEDS_EXP1]:
                        print(f"[exp1] {topo} {mode} loss={rate} K={K} seed={seed}",
                              flush=True)
                        run(cli(cf, mode, K, loss_model=0, seed=seed))
                        f, fct, mb, _ = metrics()
                        flows_list.append(f)
                        fct_list.append(fct)
                        mbps_list.append(mb)

                    # Aggregate
                    flows_mean, flows_std = aggregate_stats(flows_list)
                    fct_mean, _ = aggregate_stats(fct_list)
                    mbps_mean, _ = aggregate_stats(mbps_list)

                    rows.append(dict(
                        topology=topo, loss_rate=rate, K=K, mode=mode,
                        flows_mean=round(flows_mean, 1),
                        flows_std=round(flows_std, 2),
                        avg_fct_us=round(fct_mean, 1),
                        agg_mbps=int(mbps_mean)
                    ))

    write_csv(p, rows, fieldnames,
              lambda r: (r["topology"], float(r["loss_rate"]),
                         int(r["K"]), MODE_ORDER[r["mode"]]))


# ---------------------------------------------------------------- exp2
def do_exp2():
    p = RESULTS / "exp2_loss_model.csv"
    rows = list(csv.DictReader(open(p))) if p.exists() else []
    fieldnames = ["topology", "loss_model", "K", "mode",
                  "flows_mean", "flows_std", "avg_fct_us", "agg_mbps"]

    for topo in ["25Gb", "100Gb"]:
        cf = str(conf_loss(topo, "0.01"))
        for mode in ["RaLL", "IRN", "SRNIC"]:
            Ks = sorted(set(K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])))
            for K in Ks:
                for lm_name, lm in [("no-loss", 0), ("independent", 0), ("burst", 1)]:
                    # Use no-loss config for no-loss baseline
                    if lm_name == "no-loss":
                        cf_use = str(conf_loss(topo, "0.0"))
                    else:
                        cf_use = cf

                    if already(rows, ["topology", "loss_model", "K", "mode"],
                               [topo, lm_name, K, mode]):
                        continue

                    # Run N_SEEDS_EXP2 times
                    flows_list, fct_list, mbps_list = [], [], []
                    for seed in SEEDS[:N_SEEDS_EXP2]:
                        print(f"[exp2] {topo} {mode} {lm_name} K={K} seed={seed}",
                              flush=True)
                        run(cli(cf_use, mode, K, loss_model=lm, seed=seed))
                        f, fct, mb, _ = metrics()
                        flows_list.append(f)
                        fct_list.append(fct)
                        mbps_list.append(mb)

                    # Aggregate
                    flows_mean, flows_std = aggregate_stats(flows_list)
                    fct_mean, _ = aggregate_stats(fct_list)
                    mbps_mean, _ = aggregate_stats(mbps_list)

                    rows.append(dict(
                        topology=topo, loss_model=lm_name, K=K, mode=mode,
                        flows_mean=round(flows_mean, 1),
                        flows_std=round(flows_std, 2),
                        avg_fct_us=round(fct_mean, 1),
                        agg_mbps=int(mbps_mean)
                    ))

    write_csv(p, rows, fieldnames,
              lambda r: (r["topology"],
                         0 if r["loss_model"]=="no-loss" else (1 if r["loss_model"]=="independent" else 2),
                         int(r["K"]), MODE_ORDER[r["mode"]]))


# ---------------------------------------------------------------- exp3
def do_exp3():
    p = RESULTS / "exp3_incast.csv"
    rows = list(csv.DictReader(open(p))) if p.exists() else []
    fieldnames = ["topology", "incast_n", "K", "mode",
                  "flows_mean", "flows_std", "avg_fct_us", "agg_mbps", "max_fct_us"]
    Ns = [8, 16, 32, 64]

    for topo in ["25Gb", "100Gb"]:
        for N in Ns:
            cf = str(conf_incast(topo, N))
            for mode in ["RaLL", "IRN", "SRNIC"]:
                Ks = K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])
                Ks = [k for k in Ks if k <= 128 or mode == "IRN"]
                for K in Ks:
                    if already(rows, ["topology", "incast_n", "K", "mode"],
                               [topo, N, K, mode]):
                        continue

                    # Run N_SEEDS_EXP3 times
                    flows_list, fct_list, mbps_list, max_fct_list = [], [], [], []
                    for seed in SEEDS[:N_SEEDS_EXP3]:
                        print(f"[exp3] {topo} {mode} N={N} K={K} seed={seed}",
                              flush=True)
                        run(cli(cf, mode, K, loss_model=0, seed=seed))
                        f, fct, mb, mx = metrics()
                        flows_list.append(f)
                        fct_list.append(fct)
                        mbps_list.append(mb)
                        max_fct_list.append(mx)

                    # Aggregate
                    flows_mean, flows_std = aggregate_stats(flows_list)
                    fct_mean, _ = aggregate_stats(fct_list)
                    mbps_mean, _ = aggregate_stats(mbps_list)
                    max_fct_mean, _ = aggregate_stats(max_fct_list)

                    rows.append(dict(
                        topology=topo, incast_n=N, K=K, mode=mode,
                        flows_mean=round(flows_mean, 1),
                        flows_std=round(flows_std, 2),
                        avg_fct_us=round(fct_mean, 1),
                        agg_mbps=int(mbps_mean),
                        max_fct_us=round(max_fct_mean, 1)
                    ))

    write_csv(p, rows, fieldnames,
              lambda r: (r["topology"], int(r["incast_n"]),
                         int(r["K"]), MODE_ORDER[r["mode"]]))


if __name__ == "__main__":
    which = sys.argv[1:] or ["1", "2", "3"]
    if "1" in which: do_exp1()
    if "2" in which: do_exp2()
    if "3" in which: do_exp3()
    print("done.")
