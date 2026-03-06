# Wayland Rework Cherry-pick Plan

Base analysis range: `4c99713..2720603`

## Recommended sets

### Set A (safe baseline, strongly recommended)

```bash
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
```

### Set B (do NOT pick by default: experiment-heavy perf loop)

```bash
5ed7c60 909cd23 23a9b86 2650d4c 6a3a682 e67c45e
16198f0 eb3d0d8 8dfd10b 679fb41 db4f892 caf7a0c
c67c407 35e98d2 71d4fcc e1d1f21 ab7ce6d c4d9ce3 de01feb fd17321
f5d8c52 e0f4204 dd83444
```

## Cherry-pick order (Set A)

This order minimizes dependency surprises in Tizen platform files:

```bash
3c332d8 737224c d24f739 2e67699 893225b e682cbb 03bf2f8 109d5d5 ab34b5b \
ffa659b 202b625 8d48e40 2dc0638 461c4f4 40599c0 8a9c3d3 c4dd530 2720603
```

## Execution template

```bash
# 0) checkout target branch
# git checkout <target-branch>

# 1) ensure clean tree
# git status --porcelain

# 2) cherry-pick in sequence
for c in \
  3c332d8 737224c d24f739 2e67699 893225b e682cbb 03bf2f8 109d5d5 ab34b5b \
  ffa659b 202b625 8d48e40 2dc0638 461c4f4 40599c0 8a9c3d3 c4dd530 2720603
do
  git cherry-pick -x "$c" || break
done
```

## Conflict-prone files (handle carefully)

- `flutter/shell/platform/tizen/tizen_window_ecore_wl2.cc`
- `flutter/shell/platform/tizen/tizen_window_ecore_wl2.h`
- `flutter/shell/platform/tizen/tizen_renderer_egl.cc`
- `flutter/shell/platform/tizen/tizen_event_loop.cc`
- `flutter/shell/platform/tizen/tizen_vsync_waiter.cc`
- `flutter/shell/platform/tizen/tizen_input_method_context.cc`
- `flutter/shell/platform/tizen/BUILD.gn`

## Conflict resolution policy

1. Prefer **non-xdg tolerance + null/early guard** semantics from Set A.
2. Keep **wayland-cursor link/runtime path** from `c4dd530` and `2720603`.
3. Reject accidental reintroduction of aggressive hover-motion throttling from Set B.
4. Validate IME paths:
   - noop callback signatures matched (`8d48e40` intent)
   - commit_content + recapture_string flow preserved (`40599c0`, `8a9c3d3` intent)

## Post-pick checks

```bash
# build / smoke (adjust to your local build entrypoints)
# <your tizen build command>

# sanity grep for known bad patterns from experiment-heavy loop
rg -n "hard-disable|aggressive input-loop cut|skip wayland dispatch|hover.*disable" flutter/shell/platform/tizen || true
```
