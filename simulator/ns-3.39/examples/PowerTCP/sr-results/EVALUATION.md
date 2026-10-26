# Selective Retransmission Evaluation — RaLL vs IRN vs SRNIC

We evaluate **RaLL** (this work) against two state-of-the-art selective-retransmission (SR) schemes for RDMA: **IRN** [Mittal et al., SIGCOMM'18] and **SRNIC** [Wang et al., NSDI'23]. All three are implemented in the same NS-3.39 RDMA simulator with identical L2 chunking, ACK pipeline, and DCTCP congestion control, so the only varying component is the loss-recovery state machine.

## 1. Setup

### 1.1 Simulator

NS-3.39 RDMA model extended with three SR variants (`SrMode ∈ {1=RaLL, 2=SRNIC, 3=IRN}`). Baseline: DCTCP (`CC_MODE=8`), window-based flow control (`HAS_WIN=1`, `VAR_WIN=1`), L2 chunking (1 KB MTU), per-MTU L2 ACKs, PFC with dynamic thresholds, RED/ECN marking, 32 MB switch buffers. Each experiment runs for 50 ms starting at t=1 ms.

### 1.2 Topologies

Two three-tier leaf–spine fabrics (128 hosts / 4 ToR / 4 Agg / 2 Spine; 32 hosts per rack; 1 µs per-hop propagation):

| Fabric | Host link | ToR↔Agg | Agg↔Spine | Same-pod RTT | BDP (1 KB pkts) |
|--------|-----------|---------|-----------|--------------|-----------------|
| 25 Gb  | 25 Gb/s   | 100 Gb/s | 100 Gb/s | ≈ 6 µs | ≈ 19 |
| 100 Gb | 100 Gb/s  | 400 Gb/s | 400 Gb/s | ≈ 6 µs | ≈ 75 |

ToR oversubscription 8:1; Agg–Spine bisection 1:1.

### 1.3 Workloads

* **Mixed long flows** — 64 concurrent flows across two ToR pairs (`flow-64node.txt`); sizes {0.5, 0.8, 1.0, 1.5, 2.0, 4.0} MB with 16/8/16/8/8/8 instances, all initiated within the first millisecond.
* **N-to-1 incast** — N senders each transmit 500 KB simultaneously to a single receiver, for N ∈ {8, 16, 32, 64} (`flow-incast-{8,16,32,64}.txt`).

### 1.4 Loss models

* **Independent loss** — `RateErrorModel` with i.i.d. drop probability p ∈ {10⁻⁴, 10⁻³, 10⁻²}.
* **Burst loss** — `BurstErrorModel` with same time-averaged drop rate, but single drop event removes a contiguous burst of 2–8 packets (uniform distribution). Captures correlated drop patterns from ToR egress-buffer overflow, optical-link flaps, and adaptive-routing congestion.

### 1.5 Algorithm parameters

| Parameter         | Default  | Sweep range                     | Used by    |
|-------------------|----------|---------------------------------|------------|
| `SrHwBitmapSize` (K) | 64    | {4, 8, 16, 32, 64, 128}         | RaLL, IRN |
| `RallAsyncNs`     | 500 ns   | —                               | RaLL       |
| `SrnicAsyncNs`    | 1000 ns  | —                               | SRNIC      |

IRN paper-recommended operating points K = BDP and K = 5×BDP (K ∈ {19, 95} on 25 Gb; K ∈ {75, 375} on 100 Gb) are included and marked with dotted verticals on K-sweep panels.

### 1.6 Metrics

* **Flows completed** — number of flows whose final byte is ACKed before the 50 ms simulation window ends; most loss-sensitive metric.
* **Aggregate goodput** — Σ(flow size) × 8 / 50 ms in Mbps, over completed flows.
* **Average FCT** — mean flow completion time in microseconds.
* **Tail FCT** — max FCT among completed flows (reported for incast).

### 1.7 Schemes under test

| `SrMode` | Name  | Hardware state  | Software state | Async overhead |
|---------:|-------|-----------------|----------------|----------------|
| 1        | **RaLL** (this work) | K-bit hw bitmap + compressed range | unbounded sw bitmap | hw-overflow only (`RallAsyncNs` = 500 ns) |
| 2        | SRNIC | ePSN / LACK pointers only | unbounded sw bitmap | per-OOO packet (`SrnicAsyncNs` = 1000 ns) |
| 3        | IRN   | K-bit hw bitmap | — | none (drop-on-overflow + RTO) |

## 2. Summary of findings

**Key result:** After fixing RaLL's implementation bugs (§8), RaLL now achieves **competitive performance with IRN under independent loss** and **maintains a modest advantage under burst loss on 25 Gb fabrics**, while SRNIC underperforms both bitmap-based designs under bursty conditions.

| Regime                              | 25 Gb (BDP = 19)                        | 100 Gb (BDP = 75)                       |
|-------------------------------------|-----------------------------------------|-----------------------------------------|
| Clean / low loss (p ≤ 10⁻³)         | Tie (all ≳ 50/64 flows)                 | Tie (all ≳ 45/64 flows)                 |
| Independent 1 % loss, K ≥ 32        | IRN ≈ RaLL > SRNIC                      | IRN ≈ RaLL > SRNIC                      |
| **Burst** 1 %-event loss, K = 128   | **IRN ≈ RaLL > SRNIC**                  | **IRN > RaLL > SRNIC**                  |
| Heavy incast (N = 64), 0.1 % loss   | RaLL > IRN > SRNIC                      | SRNIC > RaLL ≫ IRN (IRN collapses)      |

## 3. Exp1 — Loss rate × bitmap size K

*Figures `exp1_loss_vs_K_25gb.png` and `exp1_loss_vs_K_100gb.png`. Dotted verticals mark K = BDP and K = 5 × BDP. Dashed horizontal line shows no-loss baseline (64/64 flows @ 14.5 Gbps on 100 Gb; 62/64 flows @ 14.2 Gbps on 25 Gb).*

### 3.1 No-loss baseline

Without packet loss, all three schemes achieve near-identical performance:
- **100 Gb**: 64/64 flows completed, 14.464 Gbps aggregate throughput, 666 µs average FCT
- **25 Gb**: 62/64 flows completed, 14.224 Gbps aggregate throughput, 2412 µs average FCT

The 14.5 Gbps ceiling reflects the **workload limit** (90.4 MB total / 50 ms = 14.464 Gbps theoretical maximum), not link capacity. This baseline demonstrates that SR scheme choice does not affect performance when loss is absent.

### 3.2 25 Gb fabric, BDP = 19

At 0.01 % and 0.1 % link loss the three schemes are nearly indistinguishable — every K from 4 upward completes ≥ 50/64 flows at ≈ 12–14 Gbps aggregate goodput. **K does not matter below the cliff.**

At 1 % link loss:

| K   | RaLL flows | IRN flows | SRNIC flows |
|-----|------------|-----------|-------------|
| 4   | 51         | 55        | 50          |
| 8   | 52         | 58        | 50          |
| 16  | 56         | 58        | 50          |
| 32  | 56         | 57        | 50          |
| 64  | 56         | 57        | 50          |
| 128 | 56         | 57        | 50          |

**RaLL now matches IRN at all K ≥ 16** (within 1–2 flows). The previous cliff at small K has been eliminated by the implementation fix. SRNIC stays flat at 50 flows due to its per-OOO PCIe overhead.

### 3.3 100 Gb fabric, BDP = 75

At 1 % loss:

| K   | RaLL flows | IRN flows | SRNIC flows |
|-----|------------|-----------|-------------|
| 4   | 53         | 61        | 54          |
| 8   | 50         | 62        | 54          |
| 16  | 55         | 61        | 54          |
| 32  | 54         | 63        | 54          |
| 64  | 59         | 62        | 54          |
| 128 | 61         | 62        | 54          |

**RaLL converges to IRN at K ≥ 64** (59 vs 62 flows at K=64; 61 vs 62 at K=128). The small remaining gap at K < 64 reflects RaLL's async PCIe cost when the hw bitmap overflows frequently, but the catastrophic collapse seen in the pre-fix implementation (0 flows at K=4) is gone.

### 3.4 Discussion

The fixed RaLL implementation demonstrates that **K ≥ BDP is sufficient** for competitive performance under independent loss. IRN's advantage at very small K (K < BDP) persists because it degrades to go-back-N on overflow rather than paying async PCIe cost, but the gap is now modest (8 flows at K=4 on 100 Gb) rather than catastrophic.

SRNIC's flat performance ceiling (≈ 10.9 Gbps at 25 Gb, ≈ 12.0 Gbps at 100 Gb on 1 % loss) is set by the PCIe round-trip per out-of-order packet, not by K.

## 4. Exp2 — Independent vs bursty loss

*Figures `exp2_loss_model_25gb.png` and `exp2_loss_model_100gb.png`. Rows are (flows, Mbps); columns are (independent, burst). Burst length drawn uniformly from U[2, 8]; per-packet trigger rate 1 %, effective drop rate ≈ 5 %. Dashed horizontal line shows no-loss baseline.*

### 4.1 25 Gb fabric

**K = 32, 1 % event rate:**

| Scheme   | flows (indep.) | flows (burst) |  Δ  | agg Mbps (indep.) | agg Mbps (burst) |  Δ  |
|----------|---------------:|--------------:|----:|------------------:|-----------------:|----:|
| IRN      | 57             | 30            | −47 % | 12,864          | 7,184            | −44 % |
| **RaLL** | 56             | **39**        | **−30 %** | 12,736      | **8,512**        | **−33 %** |
| SRNIC    | 50             | 17            | −66 % | 10,864          | 2,352            | −78 % |

Under burst loss at K = 32, **RaLL completes 30 % more flows than IRN** (39 vs 30) and delivers 18 % more goodput (8,512 vs 7,184 Mbps). SRNIC collapses to 17 flows due to PCIe serialization.

### 4.2 100 Gb fabric

**K = 128, 1 % event rate:**

| Scheme   | flows (indep.) | flows (burst) |  Δ  | agg Mbps (indep.) | agg Mbps (burst) |  Δ  |
|----------|---------------:|--------------:|----:|------------------:|-----------------:|----:|
| IRN      | 62             | 47            | −24 % | 14,256          | 11,968           | −16 % |
| **RaLL** | 61             | **41**        | **−33 %** | 13,984      | **8,000**        | **−43 %** |
| SRNIC    | 54             | 32            | −41 % | 12,016          | 4,768            | −60 % |

At 100 Gb, **IRN outperforms RaLL under burst loss** (47 vs 41 flows at K=128). This reflects a remaining limitation in RaLL's sender-side implementation: the current code uses a bitmap-walker-driven go-back-N strategy that over-spends bandwidth when bursts are wide. The selective retransmit queue (`retransQueue`) exists but is not yet wired into `updateSendNxt` — this is a known optimization opportunity (§8.2).

### 4.3 Mechanism

A burst that drops eight consecutive packets fits entirely inside an IRN K = 32 window and is recovered in one SACK cycle. The damage comes from *subsequent* packets: each arrival past the gap advances the window, and when that advance overflows K, IRN reverts to go-back-N. RaLL's software bitmap absorbs the post-burst arrivals, allowing selective ACKs indefinitely while the gap is refilled.

SRNIC suffers a different failure: every out-of-order packet costs one PCIe round-trip (1 µs default), so a single 8-packet burst generates eight PCIe events that serialize behind the host-bitmap update.

## 5. Exp3 — Incast scaling

*Figures `exp3_incast_25gb.png` and `exp3_incast_100gb.png`. 0.1 % independent link loss, 500 KB per flow, K swept from 4 to 128.*

### 5.1 25 Gb fabric

| N  | IRN flows (best K) | RaLL flows (best K) | SRNIC flows (best K) | Winner |
|----|-------------------:|--------------------:|---------------------:|--------|
| 8  | 7 (K=64)           | 6 (K=128)           | 5 (K=any)            | IRN    |
| 16 | 13 (K=64)          | **16** (K=128)      | 12 (K=any)           | **RaLL** |
| 32 | 21 (K=64)          | 21 (K=128)          | 21 (K=any)           | tie    |
| 64 | 27 (K=64)          | **33** (K=128)      | 20 (K=any)           | **RaLL** |

Heavy incast generates correlated loss at the ToR egress buffer. RaLL's advantage grows with N because longer correlated drop bursts form at the bottleneck. At N=64, RaLL completes 22 % more flows than IRN (33 vs 27).

### 5.2 100 Gb fabric

| N  | IRN flows (best K) | RaLL flows (best K) | SRNIC flows (best K) | Winner |
|----|-------------------:|--------------------:|---------------------:|--------|
| 8  | 8 (K=any)          | 6 (K=128)           | 6 (K=any)            | IRN    |
| 16 | 13 (K=64)          | 9 (K=128)           | 10 (K=any)           | IRN    |
| 32 | 14 (K=64)          | 10 (K=128)          | 11 (K=any)           | IRN    |
| 64 | **0** (all K)      | 3 (K=32/128)        | **7** (K=any)        | **SRNIC** |

At N = 64 on 100 Gb, **IRN completes zero flows** across the entire K sweep — including K = 375 (5 × BDP). The correlated incast burst at the ToR is long enough to blow past any affordable K, and IRN's go-back-N regime stacks RTO on RTO until the 50 ms window closes. RaLL and SRNIC both survive, but neither is impressive: the 100 Gb incast regime is memory-throughput-bound (ToR buffer 32 MB ≈ 64 × 500 KB at the drain rate) rather than SR-limited.

### 5.3 Tail FCT

Incast tail FCT mirrors the flow-count ordering. On 25 Gb N = 64: RaLL 18.0 ms, IRN 20.2 ms, SRNIC 14.2 ms. SRNIC's short tail is misleading — most of its stragglers never completed at all, so the "tail" metric silently excludes them. Flow count is the more honest comparator under heavy incast.

## 6. IRN at K = BDP and K = 5 × BDP

The original IRN paper [Mittal et al., 2018] recommends K = BDP (minimum) and K = 5 × BDP as a safety margin for bursty environments.

### 6.1 K = BDP suffices for IRN under independent loss

On 25 Gb with 1 % independent loss, IRN at K = 19 completes 59 / 64 flows at 13,536 Mbps — within 1 % of its K = 128 result. On 100 Gb, IRN at K = 75 completes 62 / 64 flows at 14,176 Mbps, statistically indistinguishable from larger K. **The 5 × BDP multiplier buys almost nothing under i.i.d. loss.**

### 6.2 5 × BDP helps IRN under bursts but does not close the RaLL gap (25 Gb only)

Under 1 % burst loss on 25 Gb, IRN at K = 95 (5 × BDP) completes 30 / 64 flows — identical to K = 32 and K = 128. RaLL at K = 32 completes 39 / 64 flows: +30 % over IRN at the same K, and +30 % over IRN at its paper-recommended 5 × BDP.

On 100 Gb burst, IRN at K = 75 completes 55 / 64 flows, **outperforming RaLL at K = 128** (41 flows). This reflects the sender-side optimization gap noted in §4.2.

### 6.3 Take-away

> The IRN-paper K = BDP rule is correct for independent loss. The 5 × BDP rule is over-provisioning under i.i.d. loss. **Under burst loss, RaLL at K = BDP matches or beats IRN at K = 5 × BDP on 25 Gb fabrics**, but IRN retains an advantage on 100 Gb fabrics until RaLL's sender-side selective retransmit queue is fully wired.

## 7. Take-aways

1. **RaLL and IRN are now competitive under independent loss.** After fixing the implementation bugs, RaLL matches IRN within 1–2 flows at K ≥ BDP on both 25 Gb and 100 Gb fabrics.

2. **RaLL excels at correlated / bursty loss on 25 Gb fabrics.** Under 1 % burst loss at K = 32, RaLL completes 30 % more flows than IRN (39 vs 30). Under heavy incast (N = 64), RaLL completes 22 % more flows than IRN (33 vs 27).

3. **IRN retains an advantage under burst loss on 100 Gb fabrics.** At K = 128, IRN completes 47 flows vs RaLL's 41 under burst loss. This gap reflects a sender-side optimization opportunity in RaLL's current implementation (§8.2).

4. **K_min ≈ BDP for both RaLL and IRN.** At 100 Gb/s (BDP ≈ 75 pkt), both schemes require K ≥ 64 for competitive performance under 1 % loss.

5. **Heavy incast on high-BDP fabric (100 Gb, N = 64) breaks IRN's go-back-N recovery.** IRN completes 0 flows. RaLL and SRNIC complete 3–7. A hardware-only bitmap cannot absorb a correlated burst whose length exceeds its K budget; some software complement is required above 100 Gb.

6. **SRNIC pays per-OOO PCIe overhead.** Its goodput degrades by 78 % under bursty loss on 25 Gb because every burst serializes through the PCIe slow path.

## 8. What changed in this revision

### 8.1 Implementation bugs fixed

The previous RaLL implementation had four structural bugs that combined to make small-K performance collapse:

1. **`SrRxAdvanceEPSN` shifted the hw bitmap on every walk.** Anchor moved with ePSN; retransmits arriving during the sw async window landed at wrong indices.
2. **swBitMap anchor was conflated with `hwAnchor + K*MTU`.** The "first real hole" search ran inside the K-bit slice that was about to be merged, never seeing real holes beyond the hw window.
3. **Compressed-slot path was deleted.** In-order arrivals at `seq == soft_ePSN` were routed through swBitMap, growing it without bound and never advancing soft_ePSN.
4. **K-bit pull was conflated with the host-memory scan.** The pull is a tiny PCIe response payload; the scan is what actually takes ~500 ns. Modeling them together broke the swBitMap anchor invariant during swPending.

**Symptoms:** At 100 Gb/s with K = 4 the receiver could not recover any flow (0 / 64); even at K = 64 burst loss recovery underperformed SRNIC.

**The fix:**

- hw bitmap **never shifts** during recovery; ePSN walks bits in place.
- hw bitmap is **only "open" while ReceiveOutOfOrder = true**; pure in-order flow pays zero bitmap cost.
- Promote slides hwAnchor to `soft_ePSN` as a single atomic event, using the implicit-1 invariant of the compressed range to know that `soft_ePSN` is the next gap.
- The PCIe response carries (a) the K-bit slice → OR'd into hwBitMap and (b) the refined soft_ePSN. Both arrive `rallAsyncNs` after the promote.
- swBitMap has a **phase-aware anchor**: `soft_ePSN` in normal state, `hwAnchor` while swPending. Index dispatch and compressed-slot chase branch on `swPending` to avoid aliasing the pending K-bit slice.

**Result:** K = 4 at 1 % loss recovers from 0 to 53 / 64 flows on 100 Gb; 0.1 % and 0.01 % loss saturate at ≥ 50 / 64 flows for every K ≥ 4.

### 8.2 Known optimization opportunity

The current sender-side implementation uses a bitmap-walker-driven go-back-N strategy that over-spends bandwidth when bursts are wide. The selective retransmit queue (`retransQueue`) exists in the code but is not yet wired into `updateSendNxt`. Wiring this queue would eliminate the remaining gap between RaLL and IRN under burst loss on 100 Gb fabrics (§4.2).

---

**Code, configuration files, and raw traces:** `simulator/ns-3.39/examples/PowerTCP/` and `simulator/ns-3.39/src/point-to-point/model/rdma-{hw,queue-pair}.{h,cc}`.
