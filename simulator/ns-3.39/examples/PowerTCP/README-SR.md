# Selective Retransmission (SR) Schemes — Comparison Suite

This directory contains the simulation infrastructure to compare four
selective-retransmission schemes for RDMA, plus the experimental data and
plotting scripts used to characterize them.

## 1. Schemes

| `srMode` | Name      | Hardware bitmap | Software bitmap | Async cost  | Reference |
|---------:|-----------|-----------------|-----------------|-------------|-----------|
| 0        | Legacy    | dynamic vector  | -               | n/a         | original prototype |
| 1        | **RaLL**  | K-bit, fixed    | unbounded, dynamic | only on hw-overflow (`RallAsyncNs`) | this work |
| 2        | SRNIC     | only ePSN/LACK pointers | unbounded, dynamic | per OOO packet (`SrnicAsyncNs`) | [Wang et al., NSDI'23](https://www.usenix.org/system/files/nsdi23-wang-zilong.pdf) |
| 3        | IRN       | K-bit, fixed    | -               | none        | [Mittal et al., SIGCOMM'18](https://radhikam.web.illinois.edu/irn.pdf) |

### High-level design summary

```
─── IRN ─────────────────────────────────────────────────────
   ePSN ──────────────►
   ┌───────────────────┐
   │ K-bit hw bitmap   │  out-of-window packets are dropped,
   └───────────────────┘  rely on RTO retransmission

─── SRNIC ───────────────────────────────────────────────────
   ePSN ──────────────►        (no hw bitmap)
                       ┌──────────────────────────────────┐
                       │ unbounded software bitmap (host) │
                       └──────────────────────────────────┘
   each OOO arrival pays one PCIe round-trip (~1µs)

─── RaLL (this work) ────────────────────────────────────────
   ePSN ─►  hwAnchor──────►       soft_ePSN──────►
            ┌──────────────┐╳╳compressed╳╳┌──────────────────────────┐
            │ K-bit hw bm  │  region (1)  │ unbounded sw bitmap      │
            └──────────────┘              └──────────────────────────┘
   only when hw bitmap fills do we ask sw, every K packets
```

## 2. RaLL — design (corrected, this revision)

### 2.1 State

```
hwAnchor    : PSN of hwBitMap[0]
hwBitMap    : K-bit hardware bitmap, covers [hwAnchor, hwAnchor+K*MTU)
ePSN        : next-expected PSN; tracks first 0 inside the hw window
soft_ePSN   : first hole as seen by software = swBitMap anchor
swBitMap    : variable-size software bitmap (host memory)
swPending   : true while a sw-update PCIe round-trip is in flight
```

### 2.2 Memory layout (key invariants)

```
  [hwAnchor ......... hwAnchor+K*MTU)   [hwAnchor+K*MTU .... soft_ePSN)   [soft_ePSN .....)
       hwBitMap (K bits)                  compressed region (implicit 1)     swBitMap
```

- Compressed range is **never explicitly stored** — it is the gap between
  `hwAnchor+K*MTU` and `soft_ePSN`, known to be all-1.
- swBitMap anchor depends on phase:
  - **normal**: anchored at `soft_ePSN`
  - **swPending**: anchored at `hwAnchor` (frozen at the promote that started
    the PCIe round-trip; the K-bit slice that lives at `swBitMap[0..K-1]` is
    waiting to be merged into hwBitMap when sw fires)

### 2.3 Hardware bitmap lifecycle

The hw bitmap is **not always in use**. It has two life states keyed by
`ReceiveOutOfOrder`:

| State | When | hwAnchor / hwBitMap |
|------|------|----------------------|
| **A. inactive** | `!ReceiveOutOfOrder` (pure in-order flow) | not maintained; hw sees no work |
| **B. active**   | hole detected, recovery in progress | hwAnchor locked, hwBitMap fills |

The transitions:

```
   pure in-order flow                             A   ←──────────┐
        │                                                        │
        │   first OOO arrival                                    │
        ▼                                                        │
   open hw bitmap:                                               │
     hwAnchor   := ePSN                                          │
     soft_ePSN  := ePSN + K*MTU                                  │
     hwBitMap   := all 0                          B              │
                                                                 │
   ── retransmits/late OOO fill bits ──                          │
                                                                 │
   walk past 1s; ePSN reaches hwAnchor+K → promote               │
                                                                 │
   ePSN >= receiveHigh + MTU  ──────────────────────────────────►┘
       (no more known holes; close the bitmap)
```

So in steady-state in-order flow, RaLL's hw bitmap pays **zero** maintenance
cost — `ePSN` advances per packet, nothing else moves.

### 2.4 Promote (hw becomes all 1)

Triggered **only** when every bit in hwBitMap is 1. Uses the fact that the
compressed range is implicitly all 1, so the entire `[hwAnchor, soft_ePSN)`
range is known-received and the next real gap is at `soft_ePSN`.

```
oldSoft := soft_ePSN
hwAnchor := oldSoft                        # whole window slides to soft_ePSN
ePSN     := oldSoft
hwBitMap := all 0                          # fresh window; PCIe response will refill
soft_ePSN := oldSoft + K*MTU               # placeholder, sw fire refines

if swBitMap is empty:
    silent slide — no PCIe round-trip
else:
    swPending := true
    schedule SrRxApplySwUpdate after rallAsyncNs
```

The K-bit slice `swBitMap[0..K-1]` is **not** copied to hwBitMap synchronously
— matching real hardware, this slice rides along with the PCIe response and
arrives `rallAsyncNs` later.

### 2.5 PCIe round-trip = SrRxApplySwUpdate

What software does and what it returns to the NIC:

```
software side (host memory):
  1. scan swBitMap from index K (= old soft_ePSN + K*MTU) onward for the
     first 0 → new soft_ePSN
  2. discard swBitMap[0..K+leading_1s-1]   (pre-K-bit slice + compressed-grow)

PCIe response writes back to the NIC:
  3. K-bit slice  = swBitMap[0..K-1]   →  OR-merge into hwBitMap[0..K-1]
  4. new soft_ePSN                     →  store
```

In the simulator, all four steps happen in `SrRxApplySwUpdate`:
- step 3: OR-merge K-bit slice into hwBitMap
- step 2: erase first K from swBitMap (now anchored at hwAnchor+K)
- compressed-slot races during the async window are reconciled by erasing an
  additional `(soft_ePSN − hwAnchor − K*MTU)/MTU` entries
- step 1: trim leading 1s; advance soft_ePSN by `trim*MTU`
- swPending := false; SrRxAdvanceEPSN walks any newly-merged 1s through ePSN

### 2.6 Out-of-order arrivals during swPending

While a PCIe round-trip is in flight new packets can keep arriving. The
dispatch is the same as in the normal phase, except for two anchor caveats:

| seq region | action (normal) | action (swPending) |
|-----------|------------------|--------------------|
| `< hwAnchor` | duplicate, drop | duplicate, drop |
| `[hwAnchor, hwAnchor+K*MTU)` | hwBitMap[idx]=1; walk | hwBitMap[idx]=1 (will be OR'd with K-bit slice at fire) |
| `[hwAnchor+K*MTU, soft_ePSN)` | compressed (dup) | compressed (dup) |
| `== soft_ePSN` | grow compressed; chase swBitMap leading 1s | grow compressed; **no chase** (swBitMap[0] aliases the pending K-bit slice) |
| `> soft_ePSN` | swBitMap[(seq−soft_ePSN)/MTU] = 1 | swBitMap[(seq−hwAnchor)/MTU] = 1 |

### 2.7 Sender mirror

The sender keeps the same shape with renamed fields: `snd_una ↔ ePSN`,
`sackHigh ↔ receiveHigh`, `soft_eACK ↔ soft_ePSN`. ACK fast path bumps
snd_una only; SACK opens the hw bitmap on first OOO and runs the same
walk / promote / async-merge state machine.

## 3. Configurable parameters (RdmaHw attributes)

| Attribute       | Default | Meaning |
|----------------|---------|---------|
| `SrMode`       | 1       | Algorithm selector (0=Legacy/1=RaLL/2=SRNIC/3=IRN) |
| `SrEnabled`    | true    | Master switch for the new SR machinery |
| `SrHwBitmapSize` | 64    | K (packets); used by RaLL and IRN |
| `SrnicAsyncNs` | 1000    | hw↔sw round-trip cost in SRNIC slow path |
| `RallAsyncNs`  | 500     | hw↔sw round-trip cost in RaLL sw-update |

CLI passthrough on the example binaries:
`--srMode --srK --srnicAsyncNs --rallAsyncNs --lossModel --burstSizeMin --burstSizeMax`

## 4. Topology and workload

`topology-25g.txt` (default): 138 nodes — 128 hosts under 4 ToRs (32 each),
ToR↔host = 25 Gb/s, ToR↔spine = 100 Gb/s, 4 spines + 2 aggs, 1 µs hop delay.
BDP @ 25 Gb × 6 µs ≈ 19 packets (MTU = 1 KB).

`topology-100g.txt`: same shape but 100 Gb/s hosts and 400 Gb/s spines.
BDP ≈ 75 packets.

Workloads:
- `flow-64node.txt` — 64 mixed-size flows (500KB–4MB) across two ToR pairs.
- `flow-incast-{8,16,32,64}.txt` — N-to-1 incast to host 64.

## 5. Experiments

All commands assume `cwd = simulator/ns-3.39`.

### Exp 0 — Bandwidth × K cliff finding

```bash
./ns3 run "powertcp-evaluation-burst \
   --conf=$PWD/examples/PowerTCP/config-100g-loss-0.01.txt \
   --algorithm=8 --windowCheck=1 --srMode=1 --srK=$K --rallAsyncNs=500"
```

### Exp 1 — Loss rate × K (corrected RaLL)

```bash
for rate in 0.0001 0.001 0.01; do
  ./ns3 run "powertcp-evaluation-burst \
    --conf=$PWD/examples/PowerTCP/config-100g-loss-${rate}.txt \
    --algorithm=8 --windowCheck=1 --srMode=$mode --srK=$K --rallAsyncNs=500"
done
```

100 Gb/s flows-completed (out of 64), DCTCP, async = 500 ns:

| loss rate \ K | 4 | 8 | 16 | 32 | 64 | 128 |
|---------------|---|---|----|----|----|-----|
| 0.01 % | 64 | 64 | 64 | 64 | 64 | 64 |
| 0.1 %  | 64 | 64 | 64 | 64 | 64 | 64 |
| 1 %    | 53 | 50 | 55 | 54 | 59 | 61 |

Reference baselines at 1 % loss (any K saturates):

| Mode | typical flows |
|------|---------------|
| IRN  | 61–63 |
| SRNIC | 54 |

K_min for RaLL is now **4 packets even at 1 % loss** (53 / 64) — small K is
viable on real hardware as long as the compressed range and async PCIe
semantics are correct. This was the central bug fixed in this revision.

### Exp 2 — Independent vs burst loss  ⭐ critical experiment

```bash
./ns3 run "powertcp-evaluation-burst \
   --conf=...  --srMode=1 --srK=$K --rallAsyncNs=500 \
   --lossModel=1 --burstSizeMin=2 --burstSizeMax=8"
```

100 Gb/s, 1 % event rate × burst 2–8 packets (~5 % effective loss):

| K   | RaLL flows | IRN flows |
|-----|------------|-----------|
| 16  | 19 | 47 |
| 32  | 31 | 50 |
| 64  | 35 | 51 |
| 128 | 41 | 47 |

Burst remains harder for RaLL than for IRN under DCTCP (because the sender
side does go-back-N driven by the bitmap walker, which over-spends bandwidth
when the burst is wide). This is a future-work knob — RaLL's selective
retransmit queue exists (`retransQueue`) but is not yet wired into
`updateSendNxt`.

### Exp 3 — Incast scaling

```bash
for N in 8 16 32 64; do
  ./ns3 run "powertcp-evaluation-burst \
     --conf=$PWD/examples/PowerTCP/config-100g-incast-${N}.txt \
     --algorithm=8 --windowCheck=1 --srMode=1 --srK=$K --rallAsyncNs=500"
done
```

## 6. What changed in this revision

The previous RaLL implementation had **four structural bugs** that combined
to make small-K performance collapse:

1. **`SrRxAdvanceEPSN` shifted the hw bitmap on every walk.** Anchor moved
   with ePSN; retransmits arriving during the sw async window landed at
   wrong indices.
2. **swBitMap anchor was conflated with `hwAnchor + K*MTU`.** The "first
   real hole" search ran inside the K-bit slice that was about to be merged,
   never seeing real holes beyond the hw window.
3. **Compressed-slot path was deleted.** In-order arrivals at
   `seq == soft_ePSN` were routed through swBitMap, growing it without
   bound and never advancing soft_ePSN.
4. **K-bit pull was conflated with the host-memory scan.** The pull is a
   tiny PCIe response payload; the scan is what actually takes ~500 ns.
   Modeling them together broke the swBitMap anchor invariant during
   swPending.

Symptoms: at 100 Gb/s with K = 4 the receiver could not recover any flow
(0 / 64); even at K = 64 burst loss recovery underperformed SRNIC.

The fix:

- hw bitmap **never shifts** during recovery; ePSN walks bits in place.
- hw bitmap is **only "open" while ReceiveOutOfOrder = true**; pure in-order
  flow pays zero bitmap cost.
- Promote slides hwAnchor to `soft_ePSN` as a single atomic event, using
  the implicit-1 invariant of the compressed range to know that
  `soft_ePSN` is the next gap.
- The PCIe response carries (a) the K-bit slice → OR'd into hwBitMap and
  (b) the refined soft_ePSN. Both arrive `rallAsyncNs` after the promote.
- swBitMap has a **phase-aware anchor**: `soft_ePSN` in normal state,
  `hwAnchor` while swPending. Index dispatch and compressed-slot chase
  branch on `swPending` to avoid aliasing the pending K-bit slice.

Result (100 Gb / DCTCP / 64 flows / async = 500 ns): K = 4 at 1 % loss
recovers from 0 to 53 / 64 flows; 0.1 % and 0.01 % loss saturate at 64 / 64
for every K we tested.

## 7. Reproducing all plots

```bash
cd examples/PowerTCP/sr-results
python3 plot_sr_results.py     # writes exp{0,1,2,3}*.png
```

Dependencies: `pandas`, `matplotlib` (install with `pip3 install --user pandas matplotlib`).

## 8. Files added by this work

```
src/point-to-point/model/rdma-hw.{h,cc}        # SR algorithms + helpers
src/point-to-point/model/rdma-queue-pair.{h,cc}# per-QP SR state
examples/PowerTCP/powertcp-evaluation-burst.cc # CLI passthrough
examples/PowerTCP/powertcp-evaluation-workload.cc # CLI passthrough
examples/PowerTCP/topology-100g.txt             # 100 Gb topology variant
examples/PowerTCP/flow-64node.txt               # mixed workload
examples/PowerTCP/flow-incast-{8,16,32,64}.txt  # incast workloads
examples/PowerTCP/config-64node*.txt            # 0.1% / 1% loss configs
examples/PowerTCP/config-incast-*.txt           # incast configs
examples/PowerTCP/config-loss-*.txt             # loss-rate sweep configs
examples/PowerTCP/sr-results/*.csv              # measured data
examples/PowerTCP/sr-results/plot_sr_results.py # plotting script
examples/PowerTCP/sr-results/exp*.png           # generated figures
```
