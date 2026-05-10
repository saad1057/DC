#!/usr/bin/env bash
# Pretty-print both benchmark CSVs (tabular columns).
set -euo pipefail
cd "$(dirname "$0")"

for f in results/results.csv results/results_full.csv; do
  if [[ ! -f "$f" ]]; then
    echo "Missing: $f (run: python3 benchmark.py)" >&2
    continue
  fi
  echo ""
  echo "================================================================================"
  echo " $f"
  echo "================================================================================"
  column -t -s, "$f"
done
echo ""
