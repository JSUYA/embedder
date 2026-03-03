# WL Cursor-Move FPS Regression (Tizen Embedder)

## Summary
On `feat/remove-ecore-wayland-rework`, app FPS drops from ~23-24 to ~11-12 when moving mouse cursor.

- Idle (lottie animation): ~23-24 FPS
- Cursor moving: ~11-12 FPS
- Historical baseline before rework: idle ~29, cursor move ~28-29

## Reproduction
1. Run app on target device with lottie animation active.
2. Observe FPS while idle.
3. Move mouse cursor continuously over app window.
4. Observe FPS collapse.

## In-app PERF_DIAG evidence
(Collected with `FLUTTER_TIZEN_PERF_DIAG=1`)

- `wl-io` path eventually optimized to low average dispatch cost (us-level), but
- `egl present_count` still collapses to ~11-12 during cursor movement windows.

Representative logs:
- `[PERF_DIAG][wl-io] in=27 dispatch=27 avg_us=50`
- `[PERF_DIAG][egl] present_count=11 avg_swap_us=374`
- `[PERF_DIAG][wl-io] in=31 dispatch=31 avg_us=73`
- `[PERF_DIAG][egl] present_count=11 avg_swap_us=283`

Interpretation:
- Even after reducing wl dispatch overhead, render present cadence still halves during cursor movement.
- Suggests compositor/driver-level interaction (cursor path / frame scheduling), not only app-side dispatch cost.

## Branch status policy (stabilized)
This branch is now reset to stable file set based on commit `c4dd530` lineage
(buildable, non-crashing baseline), to stop further churn while external root-cause is tracked.

## Next recommended actions
1. Open upstream issue with this report and logs.
2. Correlate compositor + driver versions on target.
3. Validate with alternate compositor/session settings if available.
4. Keep app-side changes minimal until upstream direction is confirmed.
