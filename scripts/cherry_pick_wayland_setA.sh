#!/usr/bin/env bash
set -euo pipefail

# Cherry-pick recommended Wayland rework stabilization set
# Source branch context: feat/remove-ecore-wayland-rework (analysis range 4c99713..2720603)

COMMITS=(
  3c332d8
  737224c
  d24f739
  2e67699
  893225b
  e682cbb
  03bf2f8
  109d5d5
  ab34b5b
  ffa659b
  202b625
  8d48e40
  2dc0638
  461c4f4
  40599c0
  8a9c3d3
  c4dd530
  2720603
)

echo "[info] current branch: $(git rev-parse --abbrev-ref HEAD)"
if [[ -n "$(git status --porcelain)" ]]; then
  echo "[error] working tree is not clean. Commit/stash first."
  exit 1
fi

for c in "${COMMITS[@]}"; do
  echo "[pick] $c"
  if ! git cherry-pick -x "$c"; then
    echo
    echo "[stop] conflict encountered at $c"
    echo "Resolve conflicts, then run:"
    echo "  git cherry-pick --continue"
    echo "or abort via:"
    echo "  git cherry-pick --abort"
    exit 2
  fi
done

echo "[done] Set A cherry-pick completed successfully."
