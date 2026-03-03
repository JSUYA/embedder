# WL FPS Triage (Immediate Plan)

## Goal
Find why cursor movement drops app FPS from ~23-24 to ~11.

## Current branch policy
- Stabilized to known non-crashing baseline (`c4dd530` lineage).
- Experimental pointer/dispatch patches rolled back.

## Collect evidence on target device
1. Run app and reproduce (idle 10s, cursor move 10s).
2. Capture profile:
   ```bash
   tools/profile_wl_fps.sh <app_pid_or_name> 20
   ```
3. Share:
   - `perf-report.txt`
   - `trace-report.txt`

## Decision rules
- If hotspots are in `wl_display_dispatch*`/input path: batch & isolate dispatch path.
- If hotspots are in `eglSwapBuffers`/driver wait: present path and compositor/driver interaction.
- If kernel IRQ/sched dominates during cursor movement: compositor + driver route, not app logic.
