#!/bin/bash
# Periodic progress reporter - runs every 30 minutes

LOG="/tmp/progress_report.log"
INTERVAL=1800  # 30 minutes

echo "=== Progress Reporter Started ===" | tee -a "$LOG"
echo "Time: $(date)" | tee -a "$LOG"
echo "" | tee -a "$LOG"

while true; do
    sleep $INTERVAL

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$LOG"
    echo "Progress Report: $(date)" | tee -a "$LOG"
    echo "" | tee -a "$LOG"

    # Check exp1
    if pgrep -f "run_all_multiseed.py 1" > /dev/null; then
        COMPLETED=$(grep -c "seed=5" /tmp/exp1_multiseed.log 2>/dev/null || echo 0)
        echo "Exp1: RUNNING ($COMPLETED/168 configs)" | tee -a "$LOG"
    elif grep -q "done." /tmp/exp1_multiseed.log 2>/dev/null; then
        echo "Exp1: ✓ COMPLETED" | tee -a "$LOG"
    fi

    # Check exp2
    if pgrep -f "run_all_multiseed.py 2" > /dev/null; then
        COMPLETED=$(grep -c "seed=5" /tmp/exp2_multiseed.log 2>/dev/null || echo 0)
        echo "Exp2: RUNNING ($COMPLETED/126 configs)" | tee -a "$LOG"
    elif grep -q "done." /tmp/exp2_multiseed.log 2>/dev/null; then
        echo "Exp2: ✓ COMPLETED" | tee -a "$LOG"
    fi

    # Check exp3
    if pgrep -f "run_all_multiseed.py 3" > /dev/null; then
        COMPLETED=$(grep -c "seed=3" /tmp/exp3_multiseed.log 2>/dev/null || echo 0)
        echo "Exp3: RUNNING ($COMPLETED/168 configs)" | tee -a "$LOG"
    elif grep -q "done." /tmp/exp3_multiseed.log 2>/dev/null; then
        echo "Exp3: ✓ COMPLETED" | tee -a "$LOG"
    fi

    # Check if all done
    if grep -q "All experiments completed" /tmp/pipeline.log 2>/dev/null; then
        echo "" | tee -a "$LOG"
        echo "🎉 ALL EXPERIMENTS COMPLETED! 🎉" | tee -a "$LOG"
        echo "Time: $(date)" | tee -a "$LOG"
        echo "" | tee -a "$LOG"
        echo "Results in: examples/PowerTCP/sr-results/" | tee -a "$LOG"
        break
    fi

    echo "" | tee -a "$LOG"
done

echo "Progress reporter exiting." | tee -a "$LOG"
