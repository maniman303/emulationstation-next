# Research: Menu Status Refresh

Research used repository source as the primary evidence. Two focused read-only research agents examined status ownership and lifecycle/rendering. No new library or external service is needed. All design unknowns are resolved; the tests below verify implementation, rather than asking the user to choose an architecture.

## 1. Separate status refresh from ordinary updates

**Decision**: Add `GuiComponent::refreshBackgroundStatus()` (virtual traversal) and a protected `refreshStatus()` hook (virtual, default returns false). Default traversal invokes the node's hook and each child's traversal exactly once, aggregating presentation-change results without short-circuiting. Implement default definitions in `BackgroundStatusRefresh.cpp`. Four existing status classes override only their own status hook; default components remain inert except for traversal.

**Evidence**: `GuiComponent.cpp:65–99` advances animation controllers/storyboards and children. `ClockComponent::update` and `BatteryTextComponent::update` call TextComponent update; NetworkIcon and BatteryIcon call ImageComponent update. ImageComponent update includes image playlists; text includes scrolling. Normal widget updates must retain their base call and existing behavior.

**Rationale**: A status-only contract prevents opting in an entire subtree or advancing animations even on a designated widget. Constructor/type behavior gives existing themed instances automatic support.

**Alternatives considered**: Calling ordinary update on opted-in widgets leaks animation and child updates. Calling update with zero time still allows dirty processing, callbacks, and other side effects. A persistent registered-pointer list creates unnecessary lifetime/reload risks. Theme opt-in breaks automatic compatibility.

**Details**: Extract existing status formatting with minimal movement; keep readings separate from presentation preferences. Normal Clock update retains its existing 1000 ms content throttle; background Clock hook samples current wall time each due pass so available clock changes are visible on the next opportunity. Preserve invalid-year handling, 12/24-hour formatting, and existing width stabilization. Network/battery hooks reconcile current preferences against synchronized latest watcher snapshots on every pass. They never query hardware directly or gate preference changes on a dirty notification. Source snapshots remain intact while a component is hidden, and constructor seeding cannot overwrite a newer notification. `BatteryLevelWatcher.h` and `NetworkStateWatcher.h` specify 5000 ms watcher intervals. Return true only for changed text, icon, visibility, or newly installed status image readiness. Retry pending image readiness on later opportunities, even without another watcher notification. Do not advance status image playlists or decorative fades to make a changed icon visible: install a newly selected status icon without waiting for its asynchronous loading fade, retaining theme opacity/transform and decorative animation timelines and preserving unrelated children. This exception applies to status presentation replacement only, not general image fading. Use the small shared StatusComponentState.h snapshot/presentation helper in existing watchers and status consumers; keep watcher ownership, notification delivery, polling intervals, and thread structure unchanged. No new thread or watcher registry is introduced.

## 2. Follow actual ownership, not visibility alone

**Decision**: Default traversal follows ordinary children without visibility/showing filters. ViewController overrides it to visit the active view exactly once and avoid all cached child views. SystemView adds its non-child status-bearing extras and static backgrounds to its normal child traversal; use existing ownership collections and avoid duplicate visits. Do not instantiate additional views or change active selections.

**Evidence**: `GuiComponent.cpp:725–767` loads nested theme children. `views/gamelist/ISimpleGameListView.cpp:89–92` adds extras as children. `ViewController.cpp:937–964` updates only `mCurrentView`, although cached views are also children at 831/845. `SystemView.cpp:605–617` manually updates `mStaticBackgrounds` and per-entry `backgroundExtras`; `updateExtras` at 1489 visits manually owned extras. NetworkIcon starts invisible. SystemView marks extras not showing while covered, so `isShowing()` is unsuitable as a status eligibility filter.

**Rationale**: Hidden indicators must become visible when status changes, and indicators inside paused containers still need refresh. SystemView's frozen transition composition may include adjacent entries; visiting its existing extras is a bounded, simpler correctness choice than reconstructing render culling.

**Alternatives considered**: Ordinary child recursion alone misses system-screen extras; updating all ViewController children refreshes cached screens and duplicates work. Visibility filtering can permanently hide an initially disconnected network indicator. RTTI searches and global registries add avoidable coupling.

## 3. Bound cadence without changing foreground timing

**Decision**: A per-Window cadence helper tracks coverage eligibility and next due opportunity using monotonic time. Eligibility requires distinct stack bottom/top, awake Window, and no active screensaver. Refresh on entry, then nominally every 100 ms; execute at most one pass per loop. Advance a missed deadline directly to a future opportunity, discarding missed work. Keep cadence phase under normal frame jitter so repeatedly rounded 16 ms frames do not degrade to one update every 112 ms. A delayed pass must never be followed by a catch-up burst. Use unsigned wrap-safe elapsed-time arithmetic. For normal lateness below one interval, keep the deadline phase; after missing a whole additional interval, discard it and schedule the next opportunity 100 ms from now.

**Evidence**: `Window.cpp:433–543` updates only `peekGui()` while independently updating global overlays, notifications, audio, and screensaver. `main.cpp:716–733` can wait indefinitely through `PowerSaver::getTimeout()`; `main.cpp:810` resets normal frame timing on event wake. Therefore frame delta accumulation alone is not a reliable status clock.

**Rationale**: The helper stays independent of animation elapsed time and allows deterministic testing. Status passes do not reset inactivity or increment/decrement PowerSaver counters. A normal no-stall schedule must meet the specification's 90–110 opportunities over 10 seconds.

**Alternatives considered**: A new periodic thread risks component lifetime/render-thread access. Globally disabling power saving changes battery and sleep behavior. Resetting elapsed time to zero on every due frame can miss the cadence tolerance at 60 Hz. Replaying all elapsed intervals violates the no-catch-up requirement.

**Integration**: Run the pass after existing posted functions and foreground update, rechecking the current GUI stack before obtaining its root. Do not hold a root pointer across callbacks or reloads. The main loop caps only its existing awake power-save standby wait to the remaining refresh deadline: `-1` means infinite, not a smaller numerical timeout. The screensaver-specific wait and sleeping branches remain unchanged. Reset state on leaving coverage, replacing the bottom root, deinit, or becoming inactive; wake/reentry starts fresh. Submenu changes with the same bottom remain one covered interval. A menu action that deliberately changes settings/content retains its established effects.

## 4. Refresh the cache through existing composition

**Decision**: When a status pass changes presentation, call existing `resetMenuBackgroundShader()` before rendering. Keep the current bottom → existing optional middle/overlay/shader → top composition branches exactly as they are. Render the covered bottom under a scoped freeze context, then release that context before any menu, global overlay, notification, or screensaver drawing. Existing whole-scene redraw is acceptable.

**Evidence**: `Window.cpp:625–670` owns the shader texture cache; `Window.cpp:672–735` skips drawing the bottom while the cache exists. Without invalidation even correctly updated status would remain invisible behind cached blur.

**Rationale**: Reusing existing scene composition preserves clipping, transforms, dimming, shader effects, and dialog special cases. Report changes only when necessary; never destroy and regenerate the cache every frame.

**Alternatives considered**: Drawing status separately above the snapshot breaks occlusion/blur/z-order. Replacing cache ownership or adding selective rendering is unnecessary. Invalidating only on watcher callbacks misses clocks and asynchronous status texture readiness. Rebuilding the cache without freezing render mutations advances unrelated content.

## 5. Guard concrete render-time mutations

**Decision**: Add a small RAII `CoveredSceneRenderScope` in separate files. Save/restore the prior scope state so nested rendering cannot leave the foreground frozen. Restrict use to the render thread. A guard is active only while drawing the covered bottom; status presentation hooks run separately and do not exempt a widget's descendants.

| Existing mutation | Surgical treatment |
| --- | --- |
| `ImageComponent.cpp:626` promotes asynchronously loaded textures; `fadeIn` at 800 advances opacity per draw | Defer ordinary covered texture promotion and fade progression; explicitly resolve only designated status image readiness during refresh. Continue drawing current geometry/texture. |
| `WebImageComponent.cpp:196–229` starts requests and invokes completion callbacks from render | Defer those actions while covered, continue drawing its previously presented image, and process completion normally after uncovering. |
| `VideoComponent.cpp:205–209` starts delayed video and handles loops from render | Skip those progressing calls while drawing covered content; retain snapshot and child drawing. |
| `VideoVlcComponent.cpp:475` uploads newly decoded frames | Retain the last uploaded covered texture; allow normal frame upload once uncovered. Preserve existing topWindow pause/resume behavior. |
| `ViewController.cpp:1014–1033` advances game-return readiness counters from render | Skip that accounting during the covered render scope so a redraw cannot finish a paused transition. |

**Rationale**: Skipping `update()` does not freeze these methods. These are targeted guards at demonstrated mutation sites, not a general renderer refactor. `PostProcessShaderComponent.cpp` and renderer shader code show no time-driven progression; unchanged shader parameters can be redrawn.

**Alternatives considered**: Moving every render side effect into update is a large behavioral rewrite. Freezing all render calls hides refreshed status. A global freeze flag lasting through menu rendering freezes foreground effects. Allowing all status subtree rendering mutations advances unrelated decorative descendants.

**Compatibility note**: Video coverage already invokes `topWindow`/`manageState`, pausing playback and handling delayed starts. Preserve those existing lifecycle semantics, including the established handling of an unstarted delayed video; do not substitute onHide/onShow or introduce a new media timeline.

## 6. Cancel stale held-input state without delivering background input

**Decision**: Keep Window's top-only input routing. On `topWindow(false)`, silently cancel navigation repeat velocity/tier/accumulators and MultiStateInput press/hold bookkeeping. Add a small explicit silent cancellation helper to IList and `cancel()` to MultiStateInput. Wire only the owners that require it, including the manually owned SystemView carousel and ISimpleGameListView hold buttons. Ensure Carousel's override chains its IList base and forwards existing child lifecycle appropriately.

**Evidence**: `Window.cpp:75–96` calls topWindow(false) when covering a GUI; `Window.cpp:287–288` forwards input only to the current top GUI, so releases can be swallowed. IList stores repeat state separately. Crucially, `IList.h:267–293` shows that **both `stopScrolling()` and `listInput(0)` can invoke `onCursorChanged(CURSOR_STOPPED)`**; neither is a safe silent freeze primitive. `SystemView.cpp:1480–1486` currently forwards topWindow to extras/static backgrounds but not its carousel. MultiStateInput stores held state and elapsed hold time and lacks a reset operation.

**Rationale**: Clearing only input bookkeeping prevents a released-in-menu key from scrolling or triggering a long press after closing. Selection, visual offsets, animation progress, and pending animation callbacks must remain untouched. Preserve existing power-save unlock semantics associated with cancelling active repeat, without fabricating input or changing pause counters.

**Alternatives considered**: Sending releases to the bottom violates background-input isolation and can trigger short-press actions. Calling stopScrolling/listInput(0) can finalize transitions and change content. Keeping held state can resume phantom repeat. Broad input-manager changes affect global hotkeys and menu behavior.

## Edge-case coverage and evidence needed

| Challenge | Mitigation | Validation |
| --- | --- | --- |
| Nested/rapid menus, below-100 ms sessions | Derive coverage from distinct bottom/top; preserve interval across submenus; reset on uncover | 20 cycles, nested opens/closes, immediate entry refresh |
| Root destruction, theme reload, posted callbacks | No persistent node registry; resolve live ownership each pass; reset root identity | Replace/reload root during menu action, no duplicate callbacks or stale access |
| Hidden network/battery; nested layouts | No visibility eligibility filter; no parent update | Disconnected → connected and unavailable → available with original theme |
| Multiple status indicators | Per-instance hook, exact-once owner traversal | Distinct counters and displayed values for each instance |
| Unchanged values or pending icon texture | Cache unchanged on stable passes; readiness retry independently invalidates when ready | Stable cache plus delayed status texture becomes visible |
| Decorative textures/web requests finish while covered | Scoped deferral; no layout-changing callback during redraw | Cover during image fade/download; compare state through forced cache refresh |
| In-flight video decode | Pause lifecycle plus frozen texture upload | Last frame stable while menus open; normal playback resumes |
| Menu opened during animation/game return | No ordinary background update; guard render accounting | Progress/camera/fade counters unchanged until final close |
| Held navigation/long press released in menu | Silent input cancellation at coverage boundary | No resumed repeat, unexpected short press, launch, or long-press action |
| Power-save indefinite wait | Eligible deadline caps standby wait | 90–110 passes in 10 seconds with instant/enhanced modes |
| Sleep, screensaver, stalled loop, tick wrap | Reset inactive cadence, one pass only, unsigned time | No wake inhibition, no bursts, unchanged idle transitions |

## Phase 0 outcome

Research is complete. No unresolved architectural questions or additional dependencies remain. The main risks are render-time mutation coverage and held-input lifecycle; both have source-backed, bounded changes and explicit validation gates. Runtime evidence is deferred to implementation.
