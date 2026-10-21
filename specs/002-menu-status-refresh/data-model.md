# Data Model: Menu Status Refresh

This feature adds transient coordination state only. No persisted entities, theme fields, migrations, or user settings are introduced.

## Background refresh policy

**Owner**: One Window. Implemented by the focused `BackgroundStatusRefresh` helper.

**Fields**:

- Coverage state: inactive or eligible covered main screen.
- Root identity: identity of the currently covered stack bottom; never an owning reference or a dereferenceable pointer retained across tree replacement.
- Last observation/deadline: monotonic unsigned milliseconds, interpreted with wrap-safe subtraction.
- Interval: constant 100 ms.

**Relationships**: Window supplies current eligibility and time. The policy supplies whether one pass is due and a finite remaining deadline for an eligible standby wait. It does not own GUI components, poll devices, dispatch input, or change PowerSaver state.

**Validation rules**:

- Distinct stack bottom/top is required; an empty stack or uncovered root is inactive.
- Sleep and active screensaver override coverage eligibility.
- Entry/reentry permits one immediate refresh; ordinary opportunities follow the 100 ms schedule.
- At most one traversal is dispatched per observation. Missed work is discarded, never queued.
- Under ordinary frame jitter preserve cadence phase; on a stall schedule the next future opportunity without rapid compensating passes. Normal 60 Hz frame rounding must satisfy 90–110 opportunities per 10 seconds.
- Inactive state has no refresh deadline. An existing infinite event timeout becomes finite only while eligible.
- Root replacement, deinit, and inactive transitions clear timing so no old elapsed time reaches the replacement or resumed screen.

| Prior state | Event | Result |
| --- | --- | --- |
| Inactive | First covering GUI appears while awake | Eligible, immediate status pass, new future deadline |
| Eligible | Deadline reached | One pass, next future deadline |
| Eligible | Nested menu opens/closes, same root still covered | Remain eligible, preserve cadence |
| Eligible | Last covering menu closes | Inactive; normal top-GUI updates resume with current frame delta |
| Eligible | Root replaced/reloaded | Discard old identity/timing, initialize new eligible root |
| Eligible | Sleep/screensaver activates | Inactive, no refresh wake deadline |
| Inactive | Wake into still-open menu | Eligible with fresh timing, no inactive-time replay |

## Component status capability

**Owner**: Existing GuiComponent instance; no separate registered object.

**Fields**: Existing status presentation state and a synchronized latest watcher reading, kept independent of visibility preferences. An image status component may need a small pending-readiness flag if its existing image state does not already expose this condition.

**Relationships**:

- `refreshStatus()` defaults to no operation and false; clock, network icon, battery icon, and battery text override it.
- `refreshBackgroundStatus()` performs status-only traversal; return values aggregate all changed descendants without skipping any visit.
- ViewController chooses the active screen rather than cached children. SystemView includes manually owned extras/static backgrounds; other layouts follow ordinary child ownership.

**Validation rules**:

- Neither hook calls ordinary component update, input, animation completion, or visibility lifecycle events.
- A designated node does not designate any related node implicitly.
- Invisible status nodes remain visitable; status can change visibility.
- Changed means actual presentation change, including an icon becoming ready; an unchanged reading must not needlessly invalidate the shader cache.
- Existing watcher intervals and presentation formats remain intact. Network and battery presentation reads cached snapshots, without hardware polling or notification-gated settings checks. Hiding a component never overwrites its source reading; initial seeding cannot overwrite a newer notification. Clock background sampling uses current wall time and does not advance the normal text animation or its update countdown.
- A changed icon may update its own texture readiness and presentation, but cannot advance unrelated fades, playlists, child animations, or parent layout progress. A status row may repack current child positions when status visibility or size changes, preserving child sizes, cross-axis positions, and animation progress.

## Covered render scope

**Owner**: A stack-scoped RAII object around Window's covered-bottom render only. Separate support unit, render-thread use only.

**Fields**: Previous covered-render state, restored on destruction.

**Relationships**: Image, web-image, video, and ViewController render mutation sites consult this context. It does not change draw order, transforms, clipping, or renderer resources by itself.

**Validation rules**:

- Scope ends before middle/top GUI, overlay, global status, notifications, and screensaver draw calls.
- Nested scopes restore the previous state on every exit path.
- Decorative state is drawn from its current presented values; pending unrelated resource presentation is deferred.
- Explicit status texture readiness is resolved by status refresh, not by disabling the freeze for a whole subtree.

## Menu background cache

**Owner**: Existing Window shader texture cache; ownership and invalid texture sentinel remain unchanged.

**Transitions**: Valid → invalid when aggregated status presentation changes; invalid → regenerated through existing renderMenuBackgroundShader after the fresh frozen bottom is drawn. Existing GUI push/remove/reset behavior remains in place. Stable status leaves a valid cache intact.

## Covered input bookkeeping

**Owners**: Existing IList repeat fields and MultiStateInput hold fields.

**Transitions**: On coverage loss, clear input velocity/tier/accumulators and press/hold tracking; retain cursor, geometry, animation timeline, and callbacks. Chained topWindow handlers ensure manually owned carousel and gamelist holds participate. On uncover, new input is processed normally; cleared input is never replayed.

**Validation rules**: Silent cancellation must not call cursor-change callbacks, finish animations, manufacture releases, or modify persistent user state. Balance the existing repeat-related power-save lock without altering pause counters.
