# Tasks: Menu Status Refresh

**Input**: Design documents from `specs/002-menu-status-refresh/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [UI and internal contracts](contracts/menu-status-refresh.md), [quickstart.md](quickstart.md).

**Validation**: Focused behavior checks implement the specification's explicit cadence, paused-state, input, and lifecycle acceptance scenarios using the small harness described in the plan. No general test framework, broad test expansion, or TDD requirement is introduced. Write the applicable regression cases before the behavior they exercise, where the harness permits, and distinguish failing behavior from missing build dependencies.

**Organization**: Shared safety primitives precede enabling background status refresh; story phases then implement their distinct acceptance journeys. Changes stay in focused helper files and narrow adapters. All task paths are repository-relative. No unnamed namespaces, new runtime dependencies, theme schema, persistent pointer registry, new thread, or GUI-stack rewrite.

## Format: `[ID] [P?] [Story] Description`

- `[P]` marks tasks that may run together within the explicitly listed dependency wave; they edit separate files.
- `[US1]`, `[US2]`, and `[US3]` map to the specification's user stories.
- Each task is complete only after its stated behavior is implemented or its validation evidence is recorded. A source review or successful transfer does not substitute for runtime evidence.

## Path Conventions

Existing application code is in `es-app/src/`, shared GUI code is in `es-core/src/`, standalone behavioral tests are in `tests/`, and build/validation runners are in `scripts/`. Feature evidence belongs in `specs/002-menu-status-refresh/quickstart.md`.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish a reproducible local baseline and minimal focused validation scaffold.

- [x] T001 Record baseline commands and observable menu, image/video pause, notification, power-save, and idle behavior in `specs/002-menu-status-refresh/quickstart.md`; inspect the existing standalone test pattern and `scripts/build-rgdsplus.sh`, use only local/disposable fixtures, and clearly mark unavailable runtime observations as pending.
- [x] T002 Create `tests/menu-status-refresh.cpp` and `scripts/test-menu-status-refresh.sh` with a small native C++17 assertion runner, fake monotonic clock, instrumented component counters, and controlled resource/status fixtures; compile production helper/component behavior where exercised rather than duplicating algorithms in tests, and use existing dependencies without installing packages or adding a framework.

**Checkpoint**: Baseline and focused runner structure are available; no runtime refresh behavior is enabled.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Introduce the shared traversal, cadence, and render guards before status refresh can cause background redraws.

- [x] T003 Add `refreshBackgroundStatus()` and protected `refreshStatus()` declarations to `es-core/src/GuiComponent.h`, with default definitions in new `es-core/src/BackgroundStatusRefresh.cpp` and policy declarations in `es-core/src/BackgroundStatusRefresh.h`; implement default false status capability and exact-once child traversal with non-short-circuit aggregation. Enforce: “Neither hook calls ordinary component update, input, animation completion, or visibility lifecycle events.” “A designated node does not designate any related node implicitly.” “Invisible status nodes remain visitable; status can change visibility.”
- [x] T004 Implement the per-Window cadence policy in `es-core/src/BackgroundStatusRefresh.h` and `es-core/src/BackgroundStatusRefresh.cpp`, covering identity-only root tracking, unsigned wrap-safe time, immediate entry, normal jitter phase preservation, and rescheduling 100 ms from now after missing a whole additional interval. Enforce: “Distinct stack bottom/top is required; an empty stack or uncovered root is inactive.” “Sleep and active screensaver override coverage eligibility.” “Entry/reentry permits one immediate refresh; ordinary opportunities follow the 100 ms schedule.” “At most one traversal is dispatched per observation. Missed work is discarded, never queued.” “Under ordinary frame jitter preserve cadence phase; on a stall schedule the next future opportunity without rapid compensating passes. Normal 60 Hz frame rounding must satisfy 90–110 opportunities per 10 seconds.” “Inactive state has no refresh deadline. An existing infinite event timeout becomes finite only while eligible.” “Root replacement, deinit, and inactive transitions clear timing so no old elapsed time reaches the replacement or resumed screen.” Do not retain dereferenceable component pointers or modify PowerSaver state.
- [x] T005 Create `es-core/src/CoveredSceneRenderScope.h` and `es-core/src/CoveredSceneRenderScope.cpp` as a small render-thread RAII context; enforce “Nested scopes restore the previous state on every exit path.” and “Decorative state is drawn from its current presented values; pending unrelated resource presentation is deferred.” Leave the scope inactive until Window integration.
- [x] T006 [P] Guard ordinary covered image texture promotion and frame-based fade advancement in `es-core/src/components/ImageComponent.cpp`; add only the minimal protected readiness helper in `es-core/src/components/ImageComponent.h` if required for the explicit status hook, with no general subtree exemption. Enforce: “Explicit status texture readiness is resolved by status refresh, not by disabling the freeze for a whole subtree.” Newly selected status icons may bypass their loading fade while retaining theme opacity/transform; unrelated decorative timelines remain frozen.
- [x] T007 [P] Guard covered request initiation and completion callbacks in `es-core/src/components/WebImageComponent.cpp`; retain drawing of the previously presented image and defer layout-changing callback work until normal uncovered rendering resumes.
- [x] T008 [P] Guard render-time delayed starts and looping in `es-core/src/components/VideoComponent.cpp` and decoded-frame uploads in `es-core/src/components/VideoVlcComponent.cpp`; keep the last uploaded texture and existing topWindow pause/resume and delayed-start lifecycle semantics, without media lifecycle rewrites.
- [x] T009 [P] Guard render-time game-return readiness and elapsed accounting in `es-app/src/views/ViewController.cpp` so repeated covered redraws cannot complete a paused return transition; leave normal uncovered accounting unchanged.
- [x] T010 Register the two new support units in `es-core/CMakeLists.txt`, finish native linkage in `scripts/test-menu-status-refresh.sh`, and add/run foundational assertions in `tests/menu-status-refresh.cpp` for hidden/nested exact-once traversal, false default capability, no ordinary updates/input, cadence jitter/stall/wrap, and nested render-scope restoration.

**Checkpoint**: Shared primitives compile and foundational checks pass. Ordinary runtime behavior is unchanged until US1 integration enables the new path. T006–T009 form one parallel wave after T003–T005; T010 joins that wave.

## Phase 3: User Story 1 - Read Current Status While Using Menus (Priority: P1)

**Goal**: Existing clock, Wi-Fi, and battery indicators refresh automatically while menus cover the main screen.

**Independent Test**: On unchanged system and gamelist themes, change controlled available status readings while a menu stays open; verify all existing instances show current readings, including initially hidden/nested indicators, and receive 90–110 opportunities over 10 awake seconds. No theme migration or new setting is allowed.

### Validation cases for User Story 1

- [ ] T011 [US1] Add focused cases to `tests/menu-status-refresh.cpp` for the four real status types, hidden-to-visible transitions, missing readings, original formatting, multiple/nested instances, active-versus-cached view routing, unchanged/changed presentation, delayed status texture readiness, and finite/zero/infinite timeout combination; assert that normal base updates and external polling frequency do not increase during covered refresh.

### Implementation for User Story 1

- [x] T012 [P] [US1] Extract shared clock presentation logic and override the status-only hook in `es-core/src/components/ClockComponent.h` and `es-core/src/components/ClockComponent.cpp`; keep normal TextComponent update and the normal one-second throttle, sample wall time per covered opportunity, preserve invalid-year handling, 12/24-hour formatting, visibility and stabilized dimensions. Enforce: “Existing dirty-triggered reads and presentation formats remain intact. Clock background sampling uses current wall time and does not advance the normal text animation or its update countdown.”
- [x] T013 [P] [US1] Reconcile network presentation from the latest watcher snapshot and current preferences and implement the status-only hook in `es-core/src/components/NetworkIconComponent.h` and `es-core/src/components/NetworkIconComponent.cpp`; retain watcher cadence and connected/airplane visibility/icon behavior, report actual changes, and retry only this icon's pending readiness independently of a new reading. Enforce: “Changed means actual presentation change, including an icon becoming ready; an unchanged reading must not needlessly invalidate the shader cache.”
- [x] T014 [P] [US1] Reconcile battery-icon presentation from the latest watcher snapshot and current preferences and implement the status-only hook in `es-core/src/components/BatteryIconComponent.h` and `es-core/src/components/BatteryIconComponent.cpp`; preserve charging/level thresholds, missing-battery visibility, existing watcher polling and synchronized cached readings and normal base updates, with explicit status-icon readiness. Enforce: “A changed icon may update its own texture readiness and presentation, but cannot advance unrelated fades, playlists, child animations, or parent layout progress.”
- [x] T015 [P] [US1] Reconcile battery-text presentation from the latest watcher snapshot and current preferences and implement the status-only hook in `es-core/src/components/BatteryTextComponent.h` and `es-core/src/components/BatteryTextComponent.cpp`; preserve percentage formatting, unavailable/hidden state, cached readings, stabilized dimensions, and normal TextComponent update while returning true only for presentation changes.
- [x] T016 [US1] Add ownership traversal overrides in `es-app/src/views/ViewController.h`, `es-app/src/views/ViewController.cpp`, `es-app/src/views/SystemView.h`, and `es-app/src/views/SystemView.cpp`; visit only the active ViewController view, and include SystemView ordinary children plus manually owned static backgrounds and per-entry extras exactly once without activating views or filtering out hidden/not-showing status nodes. Leave existing gamelist child ownership intact.
- [x] T017 [US1] Integrate the per-Window helper in `es-core/src/Window.h` and `es-core/src/Window.cpp`: retain top-only normal updates, re-read the live stack after posted functions/foreground callbacks, run a due status-only traversal of the covered bottom, and invalidate the existing menu shader cache only for changed presentation/readiness. Scope only the covered bottom render, ensuring “Scope ends before middle/top GUI, overlay, global status, notifications, and screensaver draw calls.” Keep existing composition/cache generation order and global status overlays intact, and expose a read-only eligible refresh deadline.
- [x] T018 [US1] Cap only eligible awake power-save standby waits in `es-app/src/main.cpp` using Window's remaining deadline, explicitly treating existing `-1` as infinite; preserve screensaver-specific wait precedence, sleeping behavior, frame limiting, global hotkeys, inactivity accounting, PowerSaver counters, and event routing. Use monotonic observations rather than normalized animation delta.
- [ ] T019 [US1] Run the US1 cases through `scripts/test-menu-status-refresh.sh` and record results in `specs/002-menu-status-refresh/quickstart.md`; verify unmodified theme compatibility on system/gamelist fixtures, fully obscured status current on uncover, 90–110 opportunities per 10 seconds under idle/input wakeups, stable cache reuse, and late icon-readiness invalidation. Mark visual cases pending if no local graphical session is available.

**Checkpoint**: US1 is independently observable and validated locally. This is the first functional slice, not a deployment gate: the specification's pause/input and presentation/lifecycle guarantees still require US2 and US3.

## Phase 4: User Story 2 - Preserve and Resume the Covered Screen (Priority: P1)

**Goal**: Lists, carousels, decorative animations, and input state stay paused and resume without catch-up or stale held actions.

**Independent Test**: Cover a moving screen for 1 and 60 seconds, use nested menus, release a previously held navigation/long-press key while covered, and close the final menu. Assert unchanged selection/visual progress, no input replay or jump, and normal response to a fresh press across 20 cycles.

### Validation cases for User Story 2

- [ ] T020 [US2] Add covered-state regression cases in `tests/menu-status-refresh.cpp` for render-time image fade/texture promotion, web completion callbacks, video starts/loops/uploads, return accounting, 1/60-second pauses, below-100 ms menus, nested coverage, and pre-held navigation/long-press releases; assert zero cursor callbacks during silent cancellation, no unintended actions or catch-up, and normal fresh input/uncovered behavior.

### Implementation for User Story 2

- [x] T021 [P] [US2] Add a silent `cancel()` operation to `es-core/src/MultiStateInput.h` and `es-core/src/MultiStateInput.cpp` that clears press and accumulated hold bookkeeping without producing short/long-press events or changing any GUI animation state.
- [x] T022 [P] [US2] Add silent repeat cancellation and coverage handling to `es-core/src/components/IList.h`, directly clearing velocity/tier/repeat accumulators without using callback-generating `stopScrolling()` or `listInput(0)`. Enforce: “Silent cancellation must not call cursor-change callbacks, finish animations, manufacture releases, or modify persistent user state. Balance the existing repeat-related power-save lock without altering pause counters.” Preserve cursor, title-overlay progress, geometry and pending animations, and chain existing child lifecycle.
- [x] T023 [US2] Chain the appropriate IList coverage handling in `es-core/src/components/CarouselComponent.cpp` while retaining its existing disable flag and lifecycle behavior; verify no animation completion, visual repositioning, or cursor-change callback is introduced when covered.
- [x] T024 [P] [US2] Extend `topWindow(false)` handling in `es-app/src/views/SystemView.h` and `es-app/src/views/SystemView.cpp` to cancel its long-press bookkeeping and notify the manually owned carousel, while preserving ordinary child/extras/static-background lifecycle forwarding without duplicate notifications.
- [x] T025 [P] [US2] Add/extend coverage handling in `es-app/src/views/gamelist/ISimpleGameListView.h` and `es-app/src/views/gamelist/ISimpleGameListView.cpp` to cancel OK/select/X/Y hold state through the new silent operation and chain existing children; keep menu releases away from the background and preserve intentional menu-driven content changes.
- [ ] T026 [US2] Run the US2 cases through `scripts/test-menu-status-refresh.sh` and record pause/resume evidence in `specs/002-menu-status-refresh/quickstart.md`; verify ViewController coverage forwarding reaches the active view, 20 nested/rapid menu cycles produce zero phantom actions, unrelated paused render state remains unchanged despite status-driven cache regeneration, and only post-menu active time advances animations. Resolve any failed case at the narrow identified boundary rather than broadening input routing.

**Checkpoint**: US2 can be tested with the shared primitives even without live device readings. After integration, US1 and US2 both pass; no menu input or elapsed covered time is replayed.

## Phase 5: User Story 3 - Keep Menu Presentation and Existing Lifecycle Behavior (Priority: P2)

**Goal**: Preserve menu responsiveness, foreground animation, notifications, composition and configured sleep/screensaver behavior.

**Independent Test**: Compare the established foreground/notification timings, loading/message/popup composition and clipped/dimmed/blurred scene against baseline while status changes; allow existing idle/sleep/screensaver transitions and verify unchanged wake/resume behavior.

### Validation cases for User Story 3

- [ ] T027 [US3] Add integration cases to `tests/menu-status-refresh.cpp` that compare foreground versus background update counts, scope restoration before menu/global-overlay/notification rendering, cache redraw ordering, and sleeping/screensaver timeout/reset behavior; include root replacement and callbacks that mutate the stack so no stale root is visited.

### Implementation and validation for User Story 3

- [x] T028 [US3] Wire/reset policy state at actual coverage exit, root replacement, deinit, sleep and screensaver transitions in `es-core/src/Window.h` and `es-core/src/Window.cpp`, including transitions that happen during render or while normal updates are skipped; restore fresh timing on wake into an open menu without bypassing inactive lifecycle restrictions or touching inactivity/global-overlay/notification behavior.
- [ ] T029 [US3] Validate menu controls, foreground animations, notification appearance/duration/dismissal, global shortcuts, and intentional settings changes against the baseline using `scripts/test-menu-status-refresh.sh` and disposable local fixtures; record observed timing/cadence and any unavailable graphical cases in `specs/002-menu-status-refresh/quickstart.md` rather than assuming success from background refresh counts.
- [ ] T030 [US3] Exercise the existing loading/message/popup composition branches with overlapping/clipped, dimmed and blurred themes, cache enabled and disabled, and status changing mid-menu; record layering and frozen-decoration results in `specs/002-menu-status-refresh/quickstart.md`, including deferred web-image completion, delayed status-image readiness, media pause/resume and game-return transitions.
- [ ] T031 [US3] Validate power-saving idle behavior with finite/infinite waits and existing sleep/screensaver transitions/wake while a menu stays open, using controlled local-profile settings only; record cadence before inactivity, absence of refresh wake deadlines while inactive, unchanged notification/sleep behavior, and no bursts after wake or root/theme reload in `specs/002-menu-status-refresh/quickstart.md`.

**Checkpoint**: All story acceptance scenarios have evidence or an explicit pending runtime condition; pending graphical/device checks remain visible and cannot be represented as passing.

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Verify the complete change, its narrow scope and target build; keep hardware acceptance separate.

- [x] T032 Run the complete focused runner via `scripts/test-menu-status-refresh.sh` after integration, resolve any failures at their source, and record aggregate results in `specs/002-menu-status-refresh/quickstart.md`; do not expand into unrelated test suites or framework changes.
- [x] T033 Run `scripts/build-rgdsplus.sh` and `file emulationstation` locally, verify ROCKNIX configuration and the root artifact's AArch64 architecture, and record build evidence in `specs/002-menu-status-refresh/quickstart.md`; preserve ARM64 dependency paths and stop with exact user-run commands if installation or sudo is needed, before any device access.
- [x] T034 Review the final source diff against `specs/002-menu-status-refresh/plan.md` and `AGENTS.md`, remove incidental refactors, and run whitespace checks; verify focused new helper files, no unnamed namespaces, no theme/user-data or launcher changes, no persistent node registry, and no unintended normal-update/render/input behavior outside the covered scope. Record the audit in `specs/002-menu-status-refresh/quickstart.md`.
- [x] T035 Update `specs/002-menu-status-refresh/quickstart.md` with a final FR-001–FR-011 and SC-001–SC-005 evidence mapping, separate local native checks, graphical observations, cross-build, and hardware outcomes, and identify every pending/unavailable acceptance condition without claiming it passed.
- [ ] T036 After T032–T035 pass and device work is authorized, execute the bounded RG DS Plus inspection/deployment/UI verification from `specs/002-menu-status-refresh/quickstart.md` using `scripts/deploy-rgdsplus.sh` and the exact `AGENTS.md` restrictions; verify current device state read-only first, then direct transfer/hash, mount/helper, service/Sway/runtime hash and observable UI results separately. Leave a verified deployment mounted/running. If authorization or device access is unavailable, leave this task unchecked and report hardware validation pending; if deployment verification succeeds but visual UI observation is unavailable, record the verified runtime and leave the UI result explicitly pending. Do not mark local validation as device success.

**Checkpoint**: Local completion requires T001–T035 and their available acceptance evidence. Full hardware acceptance additionally requires T036 and any graphical cases deferred earlier. Generating this task list does not itself authorize device work.

## Dependencies & Execution Order

### Phase dependencies

```text
Setup: T001 → T002
Foundation: T003 → T004 → T005 → {T006, T007, T008, T009} → T010
US1: T011 → {T012, T013, T014, T015} → T016 → T017 → T018 → T019
US2: T020 → {T021, T022} → T023 → {T024, T025} → T026
US3: T027 → T028 → T029 → T030 → T031
Polish: T032 → T033 → T034 → T035 → T036 (authorized hardware gate)
```

Foundation depends on Setup and blocks every story. US1, US2, and US3 describe distinct observable journeys, but shared owner/Window/test files mean integration should follow the displayed order. Specifically T016 follows T009, T024 follows T016/T023, T025 follows T021/T023, and T028 follows T017/T027. No parallel task may edit the same shared file as an unfinished integration task.

### Story completion order

```text
Setup → Foundation → US1 (P1) → US2 (P1) → US3 (P2) → Local final checks → Authorized hardware acceptance
```

US2's isolated hold/repeat behavior can be developed and tested after Foundation without live US1 readings; US3's foreground/lifecycle invariants can be tested with synthetic status hooks. Final acceptance is cumulative. Do not deploy an intermediate US1 slice that has not passed the US2/US3 regression gates.

### Parallel opportunities

- Foundation: after T005, T006/T007/T008/T009 modify independent image, web-image, video, and view-controller files; T010 integrates them afterward.
- US1: after T011 and the completed Foundation, T012/T013/T014/T015 implement four status types in disjoint files; T016–T019 are serial integration/verification.
- US2: after T020, T021/T022 modify independent hold/list primitives; after T023, T024/T025 update independent system/gamelist owners. Both owner tasks depend on the completed primitives.
- US3: runtime evidence collection for T029/T030 can be split across separate disposable sessions and merged serially into the shared quickstart, after T028. They are not marked `[P]` because they share the evidence file and may share a graphical runtime.

## Parallel Example: User Story 1

After T011 completes, run T012 (clock), T013 (network icon), T014 (battery icon), and T015 (battery text) together. Each consumes the already-established hooks and writes different component files. Join all four before T016 ownership integration.

## Parallel Example: User Story 2

After T020 completes, run T021 (MultiStateInput cancel) and T022 (IList silent cancellation) together. Complete T023 carousel chaining, then run T024 (system view) and T025 (gamelist view) together. Join before T026 integrated held-input validation.

## Parallel Example: User Story 3

After T028 completes, one disposable graphical session may measure menu/notification behavior for T029 while another checks blur/clipping/loading dialogs for T030. Keep observations separate during execution and merge them into `quickstart.md` serially; never write that shared file concurrently. Run T031 sleep/screensaver checks in a dedicated session afterward.

## Requirement Coverage

| Requirements/outcomes | Implementation and evidence tasks |
| --- | --- |
| FR-001, FR-005; paused designated-only traversal | T003, T006–T010, T016–T017, T020, T026 |
| FR-002, FR-004; SC-004 automatic compatibility | T011–T016, T019 |
| FR-003, FR-011; SC-001 cadence and latest status | T004, T010–T019, T027–T028, T031 |
| FR-006, FR-007; SC-002, SC-003 input/pause/resume | T020–T026, T028 |
| FR-008; SC-005 layering, clipping, dimming, blur | T005–T009, T017, T027, T030 |
| FR-009; SC-005 foreground/notifications | T017–T018, T027, T029 |
| FR-010; SC-005 sleep/screensaver | T004, T018, T027–T028, T031 |
| Integrated local and hardware evidence | T032–T036 |

## Implementation Strategy

### MVP first

Complete Setup, Foundation, and US1 to demonstrate current status behind an open menu using an existing theme. This is the smallest functional demonstration. Because preserving pause/input and foreground/lifecycle behavior is part of the requested feature, the smallest releasable result also includes US2 and US3. Do not reduce those guarantees to optional polish.

### Incremental delivery

Build and validate the default-inert primitives first; enable only status-specific bodies and live ownership traversal next; integrate deadline/cache/scope behavior; finish stale-input cancellation; then validate foreground/lifecycle compatibility. Use the existing class ownership and service/provider behavior at every step. Fix failures at the narrowest demonstrated mutation site.

### Execution discipline

Run tasks sequentially except the explicitly described disjoint-file waves. Story checks belong to their respective phase; final checks consolidate evidence after integration. Keep all unchecked work visible, including unavailable graphical/device validation. No task asks for a general renderer, input, watcher, theme, or GUI-stack rewrite, and no task authorizes credential exposure, package installation, or device configuration changes.
