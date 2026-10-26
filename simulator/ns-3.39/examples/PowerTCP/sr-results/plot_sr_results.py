#!/usr/bin/env python3
"""SR comparison plots — RaLL vs IRN vs SRNIC on 25 Gb and 100 Gb fabrics.

Each experiment produces one PNG per topology, plus annotates the
IRN-paper-recommended K = BDP and K = 5xBDP points (Mittal et al.,
SIGCOMM'18) as vertical reference lines on K-sweep panels.
"""

import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl

mpl.rcParams.update({
    "font.size": 11,
    "axes.labelsize": 12,
    "axes.titlesize": 12,
    "legend.fontsize": 10,
    "lines.linewidth": 2,
    "lines.markersize": 7,
})

DIR = os.path.dirname(os.path.abspath(__file__))

MODES   = ["RaLL", "IRN", "SRNIC", "CX6"]
COLORS  = {"RaLL": "#d62728", "IRN": "#1f77b4", "SRNIC": "#2ca02c", "CX6": "#9467bd"}
MARKERS = {"RaLL": "o",       "IRN": "s",        "SRNIC": "^",       "CX6": "D"}
# BDP for cross-pod traffic (28us RTT in 3-tier fat-tree topology with 5us
# Agg<->Spine links): 25Gb*28us/8/1KB=88 packets, 100Gb*28us/8/1KB=350.
BDP     = {"25Gb": 88, "100Gb": 350}


def _line(ax, sub, x, y, label_with_mode=True, show_errorbar=True):
    """Plot line with optional error bars (if y_std column exists)"""
    for mode in MODES:
        d = sub[sub["mode"] == mode].sort_values(x)
        if d.empty:
            continue

        # Check if we have std column for error bars
        y_std_col = y.replace("_mean", "_std") if "_mean" in y else None
        if show_errorbar and y_std_col and y_std_col in d.columns:
            # Plot with error bars
            ax.errorbar(d[x], d[y], yerr=d[y_std_col],
                       marker=MARKERS[mode], color=COLORS[mode],
                       label=mode if label_with_mode else None,
                       capsize=3, capthick=1.5, linewidth=2, markersize=7)
        else:
            # Plot without error bars (backward compatible)
            ax.plot(d[x], d[y], marker=MARKERS[mode], color=COLORS[mode],
                   label=mode if label_with_mode else None)


def _annotate_bdp(ax, topo):
    ax.axvline(BDP[topo],     color="grey", lw=0.8, ls=":", alpha=0.7)
    ax.axvline(5*BDP[topo],   color="grey", lw=0.8, ls=":", alpha=0.7)


def plot_exp1():
    df = pd.read_csv(os.path.join(DIR, "exp1_loss_vs_K.csv"))
    # Separate no-loss baseline from loss scenarios
    losses = sorted([l for l in df["loss_rate"].unique() if l > 0])
    for topo in ["25Gb", "100Gb"]:
        dft = df[df["topology"] == topo]
        if dft.empty:
            continue
        baseline = dft[dft["loss_rate"] == 0.0]
        flows_col = "flows_mean" if "flows_mean" in baseline.columns else "flows"
        baseline_flows = baseline[flows_col].mean() if not baseline.empty else None
        baseline_mbps  = baseline["agg_mbps"].mean() if not baseline.empty else None
        baseline_fct   = baseline["avg_fct_us"].mean() if not baseline.empty else None

        # 3 rows now: flows / agg_mbps / avg_fct_us
        fig, axes = plt.subplots(3, len(losses),
                                 figsize=(4 * len(losses), 10), sharex=True)
        for j, loss in enumerate(losses):
            sub = dft[dft["loss_rate"] == loss]
            _line(axes[0, j], sub, "K", flows_col, show_errorbar=True)
            _line(axes[1, j], sub, "K", "agg_mbps", show_errorbar=False)
            _line(axes[2, j], sub, "K", "avg_fct_us", show_errorbar=False)
            for r in range(3):
                _annotate_bdp(axes[r, j], topo)

            if baseline_flows is not None:
                axes[0, j].axhline(baseline_flows, color="black", lw=1.2,
                                   ls="--", alpha=0.5, label="no-loss baseline")
            if baseline_mbps is not None:
                axes[1, j].axhline(baseline_mbps, color="black", lw=1.2,
                                   ls="--", alpha=0.5)
            if baseline_fct is not None:
                axes[2, j].axhline(baseline_fct, color="black", lw=1.2,
                                   ls="--", alpha=0.5)

            axes[0, j].set_title(f"loss = {float(loss)*100:.2f}%")
            for r in range(3):
                axes[r, j].set_xscale("log", base=2)
                axes[r, j].grid(alpha=0.3)
            # FCT often spans orders of magnitude (esp. K=4 GBN-fallback)
            axes[2, j].set_yscale("log")
            axes[0, j].set_ylabel("flows completed" if j == 0 else "")
            axes[1, j].set_ylabel("agg throughput (Mbps)" if j == 0 else "")
            axes[2, j].set_ylabel("avg FCT (us, log)" if j == 0 else "")
            axes[2, j].set_xlabel("K (hardware bitmap, packets)")
            axes[0, j].legend(loc="lower right", fontsize=9)
        fig.suptitle(f"Exp1: Loss rate vs K — {topo} fabric "
                     f"(BDP={BDP[topo]}, 5×BDP={5*BDP[topo]} dotted, "
                     f"no-loss baseline dashed)")
        fig.tight_layout()
        out = os.path.join(DIR, f"exp1_loss_vs_K_{topo.lower()}.png")
        fig.savefig(out, dpi=130)
        plt.close(fig)
        print("saved", os.path.basename(out))


def plot_exp2():
    df = pd.read_csv(os.path.join(DIR, "exp2_loss_model.csv"))
    flows_col = "flows_mean" if "flows_mean" in df.columns else "flows"
    metric_specs = [(flows_col, "flows completed", False),
                    ("agg_mbps",  "agg throughput (Mbps)", False),
                    ("avg_fct_us", "avg FCT (us, log)", True)]
    loss_models = [("Independent", "Independent loss"),
                   ("Burst",       "Burst loss (size 2-8)")]
    for topo in ["25Gb", "100Gb"]:
        dft = df[df["topology"] == topo]
        if dft.empty:
            continue
        # 3 rows × 2 cols
        fig, axes = plt.subplots(3, 2, figsize=(11, 10), sharex=True)
        for r, (col, ylabel, log_y) in enumerate(metric_specs):
            for c, (lm, ltitle) in enumerate(loss_models):
                ax = axes[r, c]
                show_err = (col == flows_col)
                _line(ax, dft[dft["loss_model"] == lm], "K", col, show_errorbar=show_err)
                _annotate_bdp(ax, topo)
                ax.set_xscale("log", base=2)
                if log_y:
                    ax.set_yscale("log")
                ax.grid(alpha=0.3)
                if r == 0:
                    ax.set_title(ltitle)
                    ax.legend(loc="lower right", fontsize=9)
                if r == 2:
                    ax.set_xlabel("K (packets)")
                if c == 0:
                    ax.set_ylabel(ylabel)
        fig.suptitle(f"Exp2: Independent vs Burst loss — {topo} fabric, "
                     f"1% event rate (BDP={BDP[topo]}, 5×BDP={5*BDP[topo]} dotted)")
        fig.tight_layout()
        out = os.path.join(DIR, f"exp2_loss_model_{topo.lower()}.png")
        fig.savefig(out, dpi=130)
        plt.close(fig)
        print("saved", os.path.basename(out))


def plot_exp3():
    df = pd.read_csv(os.path.join(DIR, "exp3_incast.csv"))
    flows_col = "flows_mean" if "flows_mean" in df.columns else "flows"
    Ns = sorted(df["incast_n"].unique())
    for topo in ["25Gb", "100Gb"]:
        dft = df[df["topology"] == topo].copy()
        if dft.empty:
            continue
        dft["max_fct_ms"] = dft["max_fct_us"].astype(float) / 1000.0
        dft["avg_fct_ms"] = dft["avg_fct_us"].astype(float) / 1000.0
        # 3 rows: flows / avg FCT / tail (max) FCT
        fig, axes = plt.subplots(3, len(Ns),
                                 figsize=(4 * len(Ns), 10), sharex=True)
        for j, N in enumerate(Ns):
            sub = dft[dft["incast_n"] == N]
            _line(axes[0, j], sub, "K", flows_col, show_errorbar=True)
            _line(axes[1, j], sub, "K", "avg_fct_ms", show_errorbar=False)
            _line(axes[2, j], sub, "K", "max_fct_ms", show_errorbar=False)
            for r in range(3):
                _annotate_bdp(axes[r, j], topo)
                axes[r, j].set_xscale("log", base=2)
                axes[r, j].grid(alpha=0.3)
            axes[0, j].set_title(f"{N}:1 incast")
            axes[0, j].set_ylabel("flows completed" if j == 0 else "")
            axes[1, j].set_ylabel("avg FCT (ms)" if j == 0 else "")
            axes[2, j].set_ylabel("tail FCT (ms)" if j == 0 else "")
            axes[2, j].set_xlabel("K (packets)")
            axes[0, j].legend(loc="lower right", fontsize=9)
        fig.suptitle(f"Exp3: Incast scaling — {topo} fabric, "
                     f"0.1% loss, 500 KB/flow (BDP={BDP[topo]} dotted)")
        fig.tight_layout()
        out = os.path.join(DIR, f"exp3_incast_{topo.lower()}.png")
        fig.savefig(out, dpi=130)
        plt.close(fig)
        print("saved", os.path.basename(out))


if __name__ == "__main__":
    plot_exp1()
    plot_exp2()
    plot_exp3()
    print("\nAll plots written to", DIR)
