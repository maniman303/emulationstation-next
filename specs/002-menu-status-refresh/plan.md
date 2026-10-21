# Implementation Plan: Menu Status Refresh

**Branch**: `dual_screen_support` | **Date**: 2026-10-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/002-menu-status-refresh/spec.md`, with the additional requirement for surgical changes and focused separate files.

## Summary

Keep the existing top-GUI update and input routing. Add a separate, status-only traversal of the covered main screen at a nominal 100 ms cadence. Existing clock, network, and battery widgets opt in through a dedicated virtual hook that never calls their ordinary update methods. Preserve theme formats, watcher polling, normal animation timing, and menu rendering order.

Put scheduling/traversal support and covered-render scope in separate files. Add narrow ownership adapters for ViewController and SystemView, invalidate the existing menu background shader cache when presentation changes, and guard the few render methods that currently advance state. Cancel stale held-input bookkeeping on coverage without forwarding menu input or completing animations. No renderer replacement, theme schema change, global component registry, new thread, or GUI-stack rewrite is needed.

## Technical Context

**Language/Version**: C++17, existing CMake project.

**Primary Dependencies**: Existing SDL2 event loop, OpenGL/GLES renderer, GuiComponent hierarchy, theme loader, watcher notifications, VLC media components. No new runtime dependencies.

**Storage**: Transient per-Window timing and component presentation state only; no persistent data or configuration changes.

**Testing**: Focused C++ behavior tests with fake time and instrumented components, plus existing frontend integration and visual checks. The repository has standalone C++ tests but no general GUI test framework; add a small dedicated runner rather than introducing one.

**Target Platform**: Existing supported frontend platforms; local ROCKNIX AArch64 cross-build and eventual RG DS Plus validation are the acceptance target.

**Project Type**: Native graphical application.

**Performance Goals**: 90–110 status-refresh passes in 10 seconds of normal awake menu use; foreground updates remain on their existing cadence. One pass per due iteration, never catch-up loops. Existing watcher intervals stay unchanged; status presentation adds no hardware/network polling.

**Constraints**: Default background behavior is no state advancement. Hidden status nodes must be reachable, cached offscreen views must not be refreshed, and no raw-pointer registry may survive tree reloads. Preserve clipping, composition, shader cache ownership, sleep/screensaver behavior, and global input. No unnamed namespaces. Prefer bounded state and dedicated helper files.

**Scale/Scope**: Four themed status widget types, two special view owners, Window integration, a small main-loop timeout adjustment, and targeted render/input guards. All covered scene drawing may run through the existing renderer.

## Constitution Check

*Pre-research gate*: PASS. `.specify/memory/constitution.md` contains placeholders only and supplies no concrete principles. Apply the actual repository instructions: no anonymous namespaces; no dependency installation or sudo; build locally before device access; preserve user data and launcher configuration.

*Post-design gate*: PASS. The design uses focused files and explicit hooks, preserves normal update/input paths, introduces no dependency or persistent migration, and isolates necessary render guards. Hardware validation remains a later implementation activity, with the repository deployment procedure mandatory. No device activity or runtime implementation is performed by this planning command.

## Project Structure

### Documentation (this feature)

```text
specs/002-menu-status-refresh/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── checklists/requirements.md
└── contracts/menu-status-refresh.md
```

`tasks.md` is intentionally left for `$speckit-tasks`.

### Source Code (repository root)

```text
es-core/src/
├── BackgroundStatusRefresh.h/.cpp       # new: cadence policy and base traversal definitions
├── CoveredSceneRenderScope.h/.cpp       # new: scoped freeze context for covered rendering
├── GuiComponent.h                       # small status hook/traversal declarations
├── Window.h/.cpp                        # per-window policy, call site, cache invalidation, render scope
├── MultiStateInput.h/.cpp               # small silent cancel operation
└── components/
    ├── ClockComponent.h/.cpp            # status-only hook, share existing formatting
    ├── NetworkIconComponent.h/.cpp      # status-only hook, latest cached reading plus current preferences
    ├── BatteryIconComponent.h/.cpp      # status-only hook
    ├── BatteryTextComponent.h/.cpp      # status-only hook
    ├── ImageComponent.cpp               # suppress covered texture promotion/fade advancement
    ├── WebImageComponent.cpp            # defer covered request/callback work
    ├── VideoComponent.cpp              # suppress render-time start/loop progression
    ├── VideoVlcComponent.cpp            # retain last uploaded covered video frame
    ├── IList.h                         # clear stale repeat state on coverage
    └── CarouselComponent.cpp           # chain coverage cancellation
es-app/src/
├── main.cpp                            # cap only eligible power-save wait to refresh deadline
└── views/
    ├── ViewController.h/.cpp            # active-view traversal; guard render-time return accounting
    ├── SystemView.h/.cpp                # extras traversal and silent input cancellation
    └── gamelist/ISimpleGameListView.h/.cpp # cancel long-press state on coverage
es-core/CMakeLists.txt                   # register new support files
scripts/test-menu-status-refresh.sh      # new: local focused test runner
tests/menu-status-refresh.cpp            # new: behavioral tests
```

**Structure Decision**: Preserve current ownership and class layout. Place the new behavior in two small support units, with member definitions for default status traversal in `BackgroundStatusRefresh.cpp`; existing files receive declarations, short adapters, extracted status bodies, or guarded mutation sites only. The paths above are a change budget, not permission to refactor their surrounding logic. Keep helper interfaces small and explicit.

## Implementation Boundaries and Sequence

1. Add and behavior-test the cadence policy and default status traversal. Use monotonic unsigned time with wrap-safe subtraction. Ordinary `update`, `input`, animation completion, and visibility lifecycle calls are forbidden in the status traversal.
2. Extract shared status presentation bodies. Normal widget updates keep their existing base updates and clock throttle; the background hook reads current available status without calling base update. Clock background refresh samples wall time each opportunity, rather than applying a paused countdown. Return whether presentation actually changed.
3. Add owner adapters: ViewController visits the active view once; SystemView visits ordinary children and its manually owned extras/static backgrounds once. Gamelist extras and nested layouts already participate through children. Include extras for the frozen rendered composition without activating or creating cached views.
4. Integrate the per-Window scheduler after posted functions/top-GUI updates, re-reading the stack because callbacks may replace it. Reset eligibility on coverage exit, root replacement, sleep, screensaver activation, and deinit. Use an immediate initial refresh, followed by 100 ms opportunities. Never apply inactive time to component updates.
5. Expose the next eligible deadline for the existing power-save wait in `main.cpp`. Combine timeouts with explicit handling of `-1` (infinite). Do not alter screensaver-specific waits, PowerSaver pause counters, input timestamps, or the normal frame limiter.
6. Scope frozen rendering around the covered bottom only. Suppress known progression sites, defer unrelated async presentation changes, and leave top/middle GUI, overlays, global status, notifications, and screensaver rendering outside that scope. Status hooks explicitly resolve their own image readiness without exempting descendants. Preserve the normal composition and cache generation order.
7. Cancel covered input latches through existing `topWindow(false)` lifecycle: silent repeat/hold reset only. Ensure derived owners chain it. Do not use `stopScrolling()` if its callbacks change visual state, do not call `onHide/onShow`, and do not forward synthetic releases to the covered tree.
8. Run focused tests, cross-build, and bounded visual/lifecycle validation. No broad refactoring or optimization follows unless a demonstrated defect requires it.

The detailed decisions, rejected alternatives, and edge-case mitigations are in [research.md](research.md). Observable and internal contracts are in [contracts/menu-status-refresh.md](contracts/menu-status-refresh.md).

## Complexity Tracking

No constitution violations require exceptions. The extra render/input touch points address concrete existing mutation paths; omitting them would violate preserved state and input requirements. A generic reactive renderer, callback registry, new watcher infrastructure, and new GUI scheduler are rejected as unnecessary scope expansion.


## Shared status presentation follow-up

The network regression audit found the same preference-gating defect in both battery widgets, plus unsynchronized notification handoff in the global overlay. `es-core/src/StatusComponentState.h` supplies a small reusable latest-reading snapshot and presentation-change detector for existing status components. Every presentation pass applies current preferences without waiting for a provider event; hiding retains the original source reading. Constructor seeding yields to a notification already received. Battery/network watchers keep their existing five-second schedule and publish copied snapshots; widgets perform no additional hardware reads. Text/icon formatting and ordinary base updates remain in their existing components. The clock uses the shared presentation detector and retains its normal one-second throttle. The global overlay uses the same snapshot handoff while keeping its normal update/render and layout paths. Existing stack-panel repacking applies to all status types. This replaces the earlier assumption that existing dirty flags and redundant battery queries could be preserved safely.
