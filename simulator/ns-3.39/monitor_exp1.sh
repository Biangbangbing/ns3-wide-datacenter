#!/bin/bash
# Monitor multi-seed experiment progress

LOG="/tmp/exp1_multiseed.log"
CSV="examples/PowerTCP/sr-results/exp1_loss_vs_K.csv"

echo "=== Exp1 Multi-Seed Progress ==="
echo ""

# Check if process is running
PID=$(pgrep -f "run_all_multiseed.py 1")
if [ -n "$PID" ]; then
    ELAPSED=$(ps -p $PID -o etime= | tr -d ' ')
    echo "Status: RUNNING (PID: $PID, elapsed: $ELAPSED)"
else
    echo "Status: COMPLETED or NOT STARTED"
fi

echo ""
echo "Last 5 log lines:"
tail -5 "$LOG" 2>/dev/null || echo "(no log yet)"

echo ""
echo "CSV rows completed:"
wc -l "$CSV" 2>/dev/null || echo "0 (file not created yet)"

echo ""
echo "Estimated progress:"
COMPLETED=$(tail -100 "$LOG" 2>/dev/null | grep -c "seed=5" || echo 0)
TOTAL=168  # 2 topo × 4 loss × 3 modes × 7 K
if [ "$COMPLETED" -gt 0 ]; then
    PERCENT=$((COMPLETED * 100 / TOTAL))
    echo "  Configs completed: $COMPLETED / $TOTAL ($PERCENT%)"
    REMAINING=$((TOTAL - COMPLETED))
    ETA_MIN=$((REMAINING * 5 * 15 / 60))  # 5 seeds × 15s per run
    echo "  Estimated time remaining: ~$ETA_MIN minutes"
else
    echo "  Just started..."
fi

echo ""
echo "To watch live: tail -f /tmp/exp1_multiseed.log"
