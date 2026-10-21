# Contract: Menu Status Refresh

## User-visible behavior

This is an application UI contract. No network API, CLI, or new theme/configuration interface is exposed.

1. Existing themed clock, Wi-Fi/network, battery icon, and battery text instances work automatically, including nested and multiple instances. No theme syntax changes are required.
2. While any menu covers the main screen, those components receive nominal 100 ms refresh opportunities. Values follow existing data availability; 10 Hz presentation does not require 10 Hz hardware polling.
3. All other main-screen content retains its paused visual/selection state; menu navigation never reaches it. After the last menu closes it resumes normally without inactive-time catch-up or stale held-input actions.
4. Existing composition, clipping, dimming, blur, notifications, foreground animation, global shortcuts, and idle/sleep/screensaver behavior remain the baseline.
5. Deliberate changes through a menu, such as selecting a setting that changes main-screen content, retain their normal effects. Pause does not undo intentional actions.

## Internal component boundary

Proposed signatures to be implemented with the existing C++ class conventions:

- `GuiComponent::refreshBackgroundStatus() -> bool`: virtual ownership traversal; reports whether any visited status presentation changed. Default implementation lives in the separate background-refresh support source.
- `GuiComponent::refreshStatus() -> bool`: protected virtual per-node capability, default false. Only explicitly designated types override it.

No elapsed time or input is accepted by the status hook. Implementations consume current status; they never advance normal component animations, child updates, playlists, marquee, or parent state. Normal updates call shared presentation logic while retaining their existing base updates and throttles. Aggregate results must not short-circuit traversal.

`ViewController::refreshBackgroundStatus` visits the active view exactly once, with no cache creation or inactive-screen traversal. `SystemView::refreshBackgroundStatus` adds its separately owned extras/static backgrounds exactly once to the ordinary child walk. A node that is invisible or not showing because of coverage may still have status to refresh.

## Scheduler and event-loop boundary

The dedicated helper accepts monotonic time and eligibility/root identity and returns at most one due pass plus the next finite deadline when eligible. Deterministic tests supply time directly. Window owns the helper and recomputes live stack eligibility after foreground callbacks. No helper stores component subscriptions or modifies global time.

The existing standby wait uses the earlier of its finite timeout and refresh deadline; if the existing timeout is `-1`, use the eligible deadline. Ineligible state leaves the existing timeout untouched. The screensaver wait and sleeping branches retain precedence. A status wake is not user input and must not reset inactivity or PowerSaver counters.

## Rendering boundary

CoveredSceneRenderScope is active only for the covered bottom render. Its destructor restores the prior context. Concrete render-time mutation sites defer work while scoped; ordinary draw calls still preserve the existing composition. Foreground rendering is always outside the new scope.

A refresh pass returns true when status presentation or its image readiness changes, causing Window's existing menu shader cache to be invalidated before scene drawing. False leaves a valid cache alone. No separately composited status overlay is introduced. Pending status readiness is retried until it can appear; pending unrelated texture/callback changes wait for uncovering.

## Coverage/input boundary

Existing topWindow(false) notification silently cancels held/repeat bookkeeping. IList requires a distinct cancellation operation because listInput(0) and stopScrolling can invoke cursor callbacks. MultiStateInput receives a small cancel operation. Existing view/derived overrides must chain the appropriate base/owned objects. Coverage must not be implemented by onHide/onShow, synthetic releases, animation completion, or broad input rerouting.

## Acceptance mapping

| Specification | Required evidence |
| --- | --- |
| FR-001, FR-005 | Nested-tree test: only status hooks run; zero ordinary updates/input/animation changes |
| FR-002, FR-004 | All four real status types, unchanged themes, watcher notification, preference-only changes, and unavailable-state cases |
| FR-003, FR-011 | Fake-time cadence, jitter, stall, wrap, rapid transitions and inactive reset; real idle 10-second observation |
| FR-006, FR-007 | Menu navigation and pre-held input release tests; zero callbacks/actions during cover and no phantom action afterward |
| FR-008 | Cache dirty/stable tests and visual clipping/dimming/blur check during forced status change |
| FR-009 | Foreground counters/timing and notification appearance/dismissal at original cadence |
| FR-010 | Existing idle/sleep/screensaver transition comparison with power saving and menu open |
