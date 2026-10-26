#!/bin/bash
# Comprehensive progress monitor for all experiments

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║        Multi-Seed SR Experiments Progress Monitor             ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Check pipeline monitor
PIPELINE_PID=$(pgrep -f "monitor_and_continue.sh")
if [ -n "$PIPELINE_PID" ]; then
    echo "✓ Pipeline monitor running (PID: $PIPELINE_PID)"
else
    echo "✗ Pipeline monitor not running"
fi
echo ""

# Exp1 status
echo "━━━ Experiment 1: Loss Rate × K (5 seeds) ━━━"
EXP1_PID=$(pgrep -f "run_all_multiseed.py 1")
if [ -n "$EXP1_PID" ]; then
    ELAPSED=$(ps -p $EXP1_PID -o etime= | tr -d ' ')
    echo "Status: RUNNING (PID: $EXP1_PID, elapsed: $ELAPSED)"

    # Estimate progress
    COMPLETED=$(grep -c "seed=5" /tmp/exp1_multiseed.log 2>/dev/null || echo 0)
    TOTAL=168
    if [ "$COMPLETED" -gt 0 ]; then
        PERCENT=$((COMPLETED * 100 / TOTAL))
        REMAINING=$((TOTAL - COMPLETED))
        ETA_MIN=$((REMAINING * 5 * 15 / 60))
        echo "Progress: $COMPLETED / $TOTAL configs ($PERCENT%)"
        echo "ETA: ~$ETA_MIN minutes"
    fi

    echo "Last line:"
    tail -1 /tmp/exp1_multiseed.log 2>/dev/null || echo "(no log)"
elif grep -q "done." /tmp/exp1_multiseed.log 2>/dev/null; then
    echo "Status: ✓ COMPLETED"
    ROWS=$(wc -l < examples/PowerTCP/sr-results/exp1_loss_vs_K.csv 2>/dev/null || echo 0)
    echo "CSV rows: $ROWS"
else
    echo "Status: NOT STARTED or FAILED"
fi
echo ""

# Exp2 status
echo "━━━ Experiment 2: Loss Model (5 seeds) ━━━"
EXP2_PID=$(pgrep -f "run_all_multiseed.py 2")
if [ -n "$EXP2_PID" ]; then
    ELAPSED=$(ps -p $EXP2_PID -o etime= | tr -d ' ')
    echo "Status: RUNNING (PID: $EXP2_PID, elapsed: $ELAPSED)"

    COMPLETED=$(grep -c "seed=5" /tmp/exp2_multiseed.log 2>/dev/null || echo 0)
    TOTAL=126
    if [ "$COMPLETED" -gt 0 ]; then
        PERCENT=$((COMPLETED * 100 / TOTAL))
        echo "Progress: $COMPLETED / $TOTAL configs ($PERCENT%)"
    fi

    echo "Last line:"
    tail -1 /tmp/exp2_multiseed.log 2>/dev/null || echo "(no log)"
elif grep -q "done." /tmp/exp2_multiseed.log 2>/dev/null; then
    echo "Status: ✓ COMPLETED"
    ROWS=$(wc -l < examples/PowerTCP/sr-results/exp2_loss_model.csv 2>/dev/null || echo 0)
    echo "CSV rows: $ROWS"
else
    echo "Status: WAITING (will start after exp1)"
fi
echo ""

# Exp3 status
echo "━━━ Experiment 3: Incast Scaling (3 seeds) ━━━"
EXP3_PID=$(pgrep -f "run_all_multiseed.py 3")
if [ -n "$EXP3_PID" ]; then
    ELAPSED=$(ps -p $EXP3_PID -o etime= | tr -d ' ')
    echo "Status: RUNNING (PID: $EXP3_PID, elapsed: $ELAPSED)"

    COMPLETED=$(grep -c "seed=3" /tmp/exp3_multiseed.log 2>/dev/null || echo 0)
    TOTAL=168
    if [ "$COMPLETED" -gt 0 ]; then
        PERCENT=$((COMPLETED * 100 / TOTAL))
        echo "Progress: $COMPLETED / $TOTAL configs ($PERCENT%)"
    fi

    echo "Last line:"
    tail -1 /tmp/exp3_multiseed.log 2>/dev/null || echo "(no log)"
elif grep -q "done." /tmp/exp3_multiseed.log 2>/dev/null; then
    echo "Status: ✓ COMPLETED"
    ROWS=$(wc -l < examples/PowerTCP/sr-results/exp3_incast.csv 2>/dev/null || echo 0)
    echo "CSV rows: $ROWS"
else
    echo "Status: WAITING (will start after exp2)"
fi
echo ""

# Overall summary
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if grep -q "All experiments completed" /tmp/pipeline.log 2>/dev/null; then
    echo "✓ ALL EXPERIMENTS COMPLETED!"
    echo ""
    echo "Results:"
    ls -lh examples/PowerTCP/sr-results/*.csv 2>/dev/null | tail -3
    echo ""
    echo "Plots:"
    ls -lh examples/PowerTCP/sr-results/*.png 2>/dev/null | tail -6
else
    echo "Experiments in progress..."
    echo "Estimated total time: ~8 hours"
fi
echo ""
echo "Logs:"
echo "  - Pipeline: /tmp/pipeline.log"
echo "  - Exp1: /tmp/exp1_multiseed.log"
echo "  - Exp2: /tmp/exp2_multiseed.log"
echo "  - Exp3: /tmp/exp3_multiseed.log"
echo ""
echo "To watch live: tail -f /tmp/pipeline.log"
