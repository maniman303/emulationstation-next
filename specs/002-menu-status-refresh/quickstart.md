# Quickstart Validation: Menu Status Refresh

Baseline before implementation: this repository has an SDL main loop, stack-based foreground GUI updates, and whole-scene rendering. Existing checklist review passed. No local graphical session or device observation has been recorded yet; visual/lifecycle baseline remains pending. This guide is updated as each acceptance check is completed. A successful build or helper test is not device validation.

## Prerequisites

- Repository root working directory and the established multiarch toolchain/dependencies.
- Existing unmodified theme with a clock, network indicator, and battery status where available; include a system screen and a gamelist screen. Use a disposable local test fixture for unusual nested/multiple indicators, never edit the user's device themes.
- A known pre-change baseline for menu response, foreground animation, notifications, blur/clipping, idle/sleep/screensaver, media pause, and power-saving modes.
- Follow [the behavior contract](contracts/menu-status-refresh.md) and [state transitions](data-model.md). Keep test counters transient and avoid logging device credentials, private network details, or persistent user content.

## Local focused tests

After implementation supplies the dedicated runner, run from repository root:

```sh
scripts/test-menu-status-refresh.sh
```

The runner should compile the production cadence/traversal logic with a small native test harness. Use stub resources/platform readings where needed, not a new test framework or live-device polling. Required tests:

1. Feed steady 16/17 ms frame times for 10 seconds and assert 90–110 opportunities. Repeat with jitter and many input-event wakes. Status timing must use monotonic observations rather than normalized animation delta.
2. Feed a long stall, unsigned tick wrap, immediate close, nested menus, root replacement, sleep/screensaver entry and exit. Assert at most one pass per observation, no immediate catch-up burst, correct new deadline, and no inactive elapsed time passed to components.
3. Use a paused parent containing a hidden designated status node with decorative children/siblings. Assert exact-once status visits and zero ordinary update/input/storyboard/marquee/playlist calls. Include separate SystemView extras and active-vs-cached ViewController routing.
4. Exercise real status presentation paths with controlled available readings. Assert original text/icon/visibility handling, no extra watcher polling, no decorative progress, and readiness retry after an asynchronously loaded status icon becomes available.
5. Test cache behavior with stable and changed status, including a status image that becomes ready after its dirty reading has already been consumed. Stable passes retain the cache; changed presentation invalidates before a fresh capture.
6. Test the covered render scope and its nested restoration. Image fade/promotion, web completion callback, video upload/start/loop, and ViewController return accounting stay unchanged inside it and run normally outside it. Status readiness changes only the designated icon.
7. Hold list navigation/long-press input before covering, release while covered, and uncover. Assert preserved selection/animation progress, zero cancellation cursor callbacks, no resumed repeat/short-press/long-press action, and normal response to a fresh press.
8. Test timeout combination for finite, zero, and `-1` timeouts. Ineligible, sleeping, and screensaver paths retain the original timeout; eligible standby wakes by the refresh deadline without resetting inactivity.

## Local ROCKNIX build

```sh
scripts/build-rgdsplus.sh
file emulationstation
```

Expected: build succeeds with ROCKNIX enabled; root `emulationstation` is an AArch64 ELF executable. Use ARM64 multiarch libraries. If a dependency or sudo step is required, stop and provide exact installation commands to the user; do not install packages.

## Frontend acceptance session

Use a disposable local profile/theme fixture when changing test conditions. On the device preserve the user's themes/configuration; observe existing conditions or wait for naturally changing status. A missing device condition is an untested case, not permission to change user settings silently.

- Open a menu on the system view, then a gamelist. Observe clock/network/battery changes and compare with available readings, allowing the existing provider delay. A clock rendered only to minutes needs a minute boundary; refresh counters are needed to prove 10 Hz even when its text stays constant.
- Record refresh counts for a continuous 10-second normal awake menu session (90–110 per designated instance). Repeat under the existing power-saving mode; test instant/enhanced modes in the disposable local profile. Verify foreground animation is not reduced to 10 Hz.
- Cover a moving carousel, scrolling text/list, decorative fade/playlist, and any existing video/web image. Leave the menu open for 1 second and 60 seconds. Force an available status change through the fixture and verify redraw preserves all other paused progress, including with blur and without a shader cache. Close and verify smooth resume without catch-up.
- Repeat 20 open/close cycles, including nested menus, below-100 ms intervals, held navigation and long-press buttons released while covered. No menu action may change the covered selection, and no stale action may fire after closing. A deliberate settings action remains effective.
- Exercise the existing loading/message/popup composition branches and notifications. Compare layering, transforms, clipping, dimming, blur, notification duration, and menu interaction against the baseline.
- On the disposable local profile, cover during image loading, status image loading, game-return transition, root/theme reload, and media delayed start. Confirm no stale access, no decorative completion during coverage, status readiness becomes visible, and established media lifecycle resumes.
- Let existing idle/sleep/screensaver conditions occur while a menu is open. Confirm unchanged transition/wake behavior, no status wakeups while inactive, and fresh status after waking without paused-time advancement.

## Bounded RG DS Plus deployment and verification

Only after the local tests/build pass and when device deployment is authorized, use the repository AGENTS.md procedure. First use network-enabled read-only SSH inspection to verify current OS/architecture, unit owner/state, frontend launch command, launcher, built-in executable, Sway, and deployment/target mounts. Use configured private credentials without printing them. Do not infer current device state from previous observations.

Then, from repository root:

```sh
scripts/deploy-rgdsplus.sh
```

Use the root binary and existing direct-upload/hash/mount-helper flow. Keep the normal launcher, service, HOME, arguments, themes, configuration, and user data. Do not use staged binary filenames, broad process termination, direct executable launches, reboot, or build on the device.

Report separately: local build/architecture; transfer and matching source hash; runtime bind mount and target hash; service/Sway/running process hash; UI scenarios actually observed. Leave a successfully verified deployment mounted and running for the user's UI check. Mark unavailable conditions as untested and do not claim runtime success from compilation or transfer alone.


## Implementation evidence (2026-10-21)

The focused production-helper runner completed successfully: `scripts/test-menu-status-refresh.sh` reports 23 checks passed. It exercises cadence entry/exit, unsigned tick wrap, normal frame rounding, stall recovery, exact-once hidden/nested traversal without ordinary updates, and nested covered-render-scope restoration. These are local helper checks; they do not substitute for runtime widget or GUI acceptance.

The repository build completed with `scripts/build-rgdsplus.sh`. CMake configured `ROCKNIX=ON`; the root `emulationstation` artifact is ELF64 AArch64, and `aarch64-linux-gnu-readelf` reports `Machine: AArch64`. The local artifact has not been deployed. `git diff --check` passed. The reviewed source changes are limited to the covered status traversal/cadence/render/input integration and the focused helper/test/docs; no theme, launcher, deployment, or user-data files were changed, no persistent node registry was introduced, and the new helper/test files contain no unnamed namespaces.

## Evidence map and pending acceptance

| Requirement | Local evidence | Remaining evidence |
| --- | --- | --- |
| FR-001, FR-005: designated status traversal while the covered scene stays paused | Exact-once hidden/nested helper traversal checks; status-only hooks and render freeze guards compile in the AArch64 build | Real widget counts, decorative/resource pause and resume, cursor/animation preservation: pending GUI runtime checks (T011, T020, T026) |
| FR-002, FR-004, SC-004: existing status components work without theme changes | Clock, network icon, battery icon/text implement status hooks; AArch64 build succeeds | Real readings, formatting, multiple instances, async icon readiness and unchanged-theme fixture: pending GUI checks (T011, T019) |
| FR-003, FR-011, SC-001: roughly 10 Hz and current presentation | Cadence helper checks include 60 Hz frame rounding, jitter, stalls, wrap and no catch-up; scheduler integrated with Window and event timeout | 10-second end-to-end opportunities and cache behavior: pending GUI runtime check (T019, T027) |
| FR-006, FR-007, SC-002, SC-003: input is not replayed and paused state resumes | Silent hold/repeat cancellation and owner forwarding implemented; AArch64 build succeeds | Held-key behavior, 20 menu cycles, no phantom actions and normal fresh input: pending GUI check (T020, T026) |
| FR-008, SC-005: existing composition and effects remain intact | Covered-scene render scope bounds redraw at Window; image/web/video/game-return guards compile | Clipping, dimming, blur, cache on/off and resource completion observation: pending GUI check (T027, T030) |
| FR-009, SC-005: foreground and notifications unaffected | Scope ends before foreground render; main-loop timeout integration preserves foreground path; AArch64 build succeeds | Timings and notification/global-shortcut behavior: pending GUI check (T029) |
| FR-010, SC-005: sleep/screensaver unaffected | Eligibility and transition resets integrated; AArch64 build succeeds | Finite/infinite wait and sleep/screensaver wake behavior: pending GUI check (T031) |

No local graphical session was available. Device inspection, transfer, mount, service, Sway, and running-process verification were completed as recorded below. Tasks T011, T019, T020, T026, T027, and T029–T031 remain unchecked pending the documented GUI observations. T036 remains unchecked because its on-device UI observation has not been recorded.

## RG DS Plus deployment result (2026-10-21)

After the local build passed, the device was inspected read-only and confirmed as ROCKNIX `20261020`, Linux `7.2.11` AArch64 on RG DS Plus hardware. `essway.service` and `sway.service` were active before deployment; the standard `/usr/bin/start_es.sh` launcher ran the frontend with its existing arguments. The mount table showed `/usr/bin/emulationstation` already bind-mounted from the runtime copy. The deploy script transferred the root AArch64 artifact and completed its source, mounted target, and running-process hash checks successfully.

The deployed build SHA-256 was `3a4391e0d0bd644a2eea8b077c38cdaf38a2be1791f33aa8ec59c7602dd86a5c`. The script reported `essway.service`, `sway.service`, the bind mount, and one running frontend process verified. The deployment remains mounted and running. The graphical UI scenarios have not been observed; leave the device available for the user's UI check. Therefore T036 remains unchecked until those UI results are recorded, and graphical acceptance tasks remain pending.

## Network-indicator regression follow-up

The reported settings/Wi-Fi toggle regression exposed two gaps: the network icon reconciled its visibility preference only after a watcher notification, and the generic traversal recursively bypassed nested virtual traversal overrides. Network presentation now reconciles the current setting with cached atomic watcher readings on every ordinary/status pass, without increasing network polling or relying on a dirty bit that could miss a notification. Child traversal now dispatches each child's virtual status traversal.

Stack panels implement status-only repacking when a child's visibility or displayed size changes. This reuses their existing stacking order and spacing while retaining presented child sizes and cross-axis positions; it does not run normal updates, reapply theme bounds, or advance animations. The focused runner now reports 26 checks passed, including nested virtual ownership and hidden-instance reachability regressions. Runtime checks still needed: save indicator off/on with connectivity unchanged; disconnect/reconnect Wi-Fi with indicator enabled (allow the existing watcher delay); verify the hidden icon leaves no gap and reappears in its proper position while a menu remains open. Device settings are not changed remotely to perform these checks.

The regression build passed `scripts/build-rgdsplus.sh` and was confirmed ELF64 AArch64; the final focused runner passed all 26 checks and `git diff --check` passed. Read-only device inspection reconfirmed ROCKNIX `20261020`, AArch64, the standard launcher/arguments, root-owned frontend, active `essway.service`/Sway, and the existing bind mount. `scripts/deploy-rgdsplus.sh` then exited successfully and verified SHA-256 `d1f0a4d2e072a524f0b450cb43535b449f4c880eaa585fed775da871ac6e8c64` at the upload, mounted target, and running frontend. The bind mount and both services were verified; the frontend remains running for the user's toggle/spacing check. Those interactive UI outcomes remain unobserved by the agent.


## General status-component audit and shared fix

Both battery widgets shared the notification-gated visibility defect and modified their cached battery-presence reading when hidden. The clock already applied visibility preferences on each covered pass, and the global overlay already applied preferences in render, but the overlay's notification handoff used unsynchronized fields/dirty flags.

`StatusComponentState.h` now provides synchronized latest-reading snapshots and shared visibility/value/size/readiness change detection. Clock, network icon, and battery icon/text use the shared presentation helper. Battery/network watchers and their consumers, including the global overlay, use copied synchronized readings; initial seeding cannot supersede a newer notification. Settings are applied every presentation pass, hidden components retain source readings, and hardware polling remains solely in the existing watchers. This supersedes earlier notes about preserving widget dirty flags and extra battery queries. Stack-panel status repacking covers all of these widgets.

The focused native runner passed 46 checks, including settings-only hide/re-enable, missing/returning readings, newest readings received while hidden, size-only/readiness-only change detection, source immutability, concurrent coherent snapshots, and safe initialization. These test production shared helpers; real widget formatting and physical toggle/layout observations remain pending rather than being inferred from helper success.

The shared-status build passed `scripts/build-rgdsplus.sh` and was verified ELF64 AArch64. Before deployment, read-only inspection reconfirmed the current OS/architecture, root-owned frontend, standard service launcher/arguments, active frontend/Sway services, deployment directory/helper, storage mount and executable bind mount. `scripts/deploy-rgdsplus.sh` exited successfully and verified SHA-256 `dfbdfd2ced8b734c05c94ac3743815dd7cc1b8b239c248c336d6a74461231ddc` at the uploaded source, mounted target, and running frontend. Services and bind mount were verified; the deployment remains running. Physical clock/network/battery setting round trips and row spacing still require user observation. No device settings, themes, launcher or other user data were changed.
