#!/usr/bin/env python3
"""Run the full SR comparison matrix across:
  - topologies: 25Gb / 100Gb
  - schemes: RaLL / IRN / SRNIC
  - K (incl. IRN-paper-recommended K = BDP and K = 5xBDP, per Mittal'18)

Existing rows are kept; only missing (topology, ..., mode, K) cells are run.

Usage (from ns-3.39):
    python3 examples/PowerTCP/sr-results/run_all.py [1|2|3 ...]
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
SIM_SEC  = 0.05
NS3      = str(NS3_ROOT / "ns3")
ALG_DCTCP = 8

# BDP (1 KB packets, 6 us same-pod RTT)
BDP = {"25Gb": 19, "100Gb": 75}
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
        rall_async=500, srnic_async=1000):
    sr_mode = MODE_TO_SR[mode]
    return (
        f"powertcp-evaluation-burst "
        f"--conf={conf} --algorithm={ALG_DCTCP} --windowCheck=1 "
        f"--srMode={sr_mode} --srK={K} "
        f"--rallAsyncNs={rall_async} --srnicAsyncNs={srnic_async} "
        f"--lossModel={loss_model} "
        f"--burstSizeMin={burst_min} --burstSizeMax={burst_max}"
    )


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
    rows = list(csv.DictReader(open(p)))
    fieldnames = ["topology", "loss_rate", "K", "mode",
                  "flows", "avg_fct_us", "agg_mbps"]
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
                    print(f"[exp1] {topo} {mode} loss={rate} K={K}", flush=True)
                    run(cli(cf, mode, K, loss_model=0))
                    f, fct, mb, _ = metrics()
                    rows.append(dict(topology=topo, loss_rate=rate, K=K,
                                     mode=mode, flows=f,
                                     avg_fct_us=fct, agg_mbps=mb))
    write_csv(p, rows, fieldnames,
              lambda r: (r["topology"], float(r["loss_rate"]),
                         int(r["K"]), MODE_ORDER[r["mode"]]))


# ---------------------------------------------------------------- exp2
def do_exp2():
    p = RESULTS / "exp2_loss_model.csv"
    rows = list(csv.DictReader(open(p)))
    fieldnames = ["topology", "loss_model", "K", "mode",
                  "flows", "avg_fct_us", "agg_mbps"]
    for topo in ["25Gb", "100Gb"]:
        cf = str(conf_loss(topo, "0.01"))
        for mode in ["RaLL", "IRN", "SRNIC"]:
            Ks = sorted(set(K_BASE + (K_IRN_EXTRA[topo] if mode == "IRN" else [])))
            for K in Ks:
                for lm_name, lm in [("independent", 0), ("burst", 1)]:
                    if already(rows, ["topology", "loss_model", "K", "mode"],
                               [topo, lm_name, K, mode]):
                        continue
                    print(f"[exp2] {topo} {mode} {lm_name} K={K}",
                          flush=True)
                    run(cli(cf, mode, K, loss_model=lm))
                    f, fct, mb, _ = metrics()
                    rows.append(dict(topology=topo, loss_model=lm_name,
                                     K=K, mode=mode, flows=f,
                                     avg_fct_us=fct, agg_mbps=mb))
    write_csv(p, rows, fieldnames,
              lambda r: (r["topology"], r["loss_model"], int(r["K"]),
                         MODE_ORDER[r["mode"]]))


# ---------------------------------------------------------------- exp3
def do_exp3():
    p = RESULTS / "exp3_incast.csv"
    rows = list(csv.DictReader(open(p)))
    fieldnames = ["topology", "incast_n", "K", "mode",
                  "flows", "avg_fct_us", "agg_mbps", "max_fct_us"]
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
                    print(f"[exp3] {topo} {mode} N={N} K={K}", flush=True)
                    run(cli(cf, mode, K, loss_model=0))
                    f, fct, mb, mx = metrics()
                    rows.append(dict(topology=topo, incast_n=N, K=K,
                                     mode=mode, flows=f, avg_fct_us=fct,
                                     agg_mbps=mb, max_fct_us=mx))
    write_csv(p, rows, fieldnames,
              lambda r: (r["topology"], int(r["incast_n"]),
                         int(r["K"]), MODE_ORDER[r["mode"]]))


if __name__ == "__main__":
    which = sys.argv[1:] or ["1", "2", "3"]
    if "1" in which: do_exp1()
    if "2" in which: do_exp2()
    if "3" in which: do_exp3()
    print("done.")
