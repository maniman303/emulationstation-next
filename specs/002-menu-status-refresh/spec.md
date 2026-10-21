# Feature Specification: Menu Status Refresh

**Feature Branch**: `dual_screen_support` (existing branch; no branch hook configured)

**Created**: 2026-10-21

**Status**: Draft

**Input**: User description: "While menus are open, EmulationStation should refresh only explicitly designated main-screen components, such as the clock, Wi-Fi indicator, and battery indicators, at approximately 10 updates per second. All other covered components, including lists, carousels, and decorative animations, should retain their paused state and resume normally when the menu closes, without processing background input or accumulated elapsed time. Existing status components should support this behavior automatically, without theme changes. Background rendering may redraw the scene from its otherwise frozen state to preserve existing layering, clipping, dimming, and blur; selective GPU rendering is not required. Menu interactions, foreground animations, notifications, and existing sleep and screensaver behavior must remain unaffected."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read Current Status While Using Menus (Priority: P1)

As a user browsing menus, I can still read the current clock, Wi-Fi status, and battery status on the covered main screen without changing my theme.

**Why this priority**: Menus should not leave useful status information stale, and existing installations should benefit automatically.

**Independent Test**: Open a menu on an unchanged theme containing status indicators, advance the time and change available Wi-Fi and battery readings, and verify that the displayed status follows those readings while the menu remains open.

**Acceptance Scenarios**:

1. **Given** an existing theme displaying a clock, Wi-Fi indicator, and battery indicators, **When** a menu covers the main screen and their underlying readings change, **Then** all of those existing status components continue refreshing automatically and show the latest available values in their existing formats.
2. **Given** a menu remains open during normal awake operation, **When** refresh opportunities for designated main-screen components are observed for 10 seconds, **Then** those components receive approximately 10 updates per second independently of whether their displayed values change.
3. **Given** a status indicator is fully obscured by the menu, **When** its reading changes and the menu is subsequently closed, **Then** the indicator shows the current available reading rather than its pre-menu reading.

---

### User Story 2 - Preserve and Resume the Covered Screen (Priority: P1)

As a user opening and closing menus, I return to the same list selection, carousel position, and decorative animation progress I left, with normal movement resuming smoothly.

**Why this priority**: Updating status must preserve the established pause behavior of the rest of the main screen.

**Independent Test**: Open a menu while a carousel or decorative animation is moving, navigate the menu, leave it open for 60 seconds, and close it. Compare the covered screen's state and its resumed motion with its state when the menu opened.

**Acceptance Scenarios**:

1. **Given** lists, a carousel, and decorative animations on the main screen, **When** a menu opens, **Then** all components without explicit background-refresh designation retain their selection, position, and animation progress for the entire covered interval.
2. **Given** a menu is open, **When** navigation or held input is used in the menu, **Then** the covered screen processes none of that input and performs no resulting selection changes or actions, either immediately or after the menu closes.
3. **Given** an animation was paused by a menu, **When** the last covering menu closes after either a short or long interval, **Then** it resumes from its paused progress using only subsequent active time, without a jump, accelerated catch-up, or replay of paused work.
4. **Given** a submenu covers another menu, **When** the submenu closes but the parent menu remains open, **Then** designated status refresh continues and all other covered main-screen components remain paused until the final covering menu closes.

---

### User Story 3 - Keep Menu Presentation and Existing Lifecycle Behavior (Priority: P2)

As a user, I can interact with menus, see foreground animations and notifications, and rely on the same sleep and screensaver behavior while background status stays current.

**Why this priority**: The feature must fit existing presentation and lifecycle behavior without disrupting other interface activity.

**Independent Test**: Compare the same menu navigation, foreground animations, notification appearance and dismissal, and configured idle/sleep/screensaver transitions with the established behavior. Include themes with clipping, dimming, and blur.

**Acceptance Scenarios**:

1. **Given** a menu with foreground animation and a notification, **When** designated status components refresh, **Then** menu controls, foreground animation, and notification timing continue at their normal cadence and are not limited to the background status cadence.
2. **Given** a theme with overlapping elements, clipping, dimming, or blur behind a menu, **When** status information refreshes, **Then** the existing visual relationships and effects remain correct and paused content does not advance as a consequence of being redrawn.
3. **Given** configured idle, sleep, and screensaver behavior, **When** a menu remains open until the existing conditions for those behaviors are met, **Then** the same transitions, input handling, and wake/resume behavior occur as before; status refresh does not count as user activity or keep the interface awake.

### Edge Cases

- Opening and closing a menu before a single status-refresh interval elapses must not advance paused content or leave it permanently paused.
- Repeated menu open/close cycles and nested submenus must not accumulate paused time, input, or extra refresh activity.
- A designated indicator inside an otherwise paused layout must refresh without advancing the layout, its animations, or unrelated siblings and descendants.
- A theme with no designated status components must keep all covered main-screen components paused.
- Multiple existing battery or other status indicator instances must each follow the same refresh behavior without theme edits.
- Missing or unavailable Wi-Fi or battery readings must retain the existing unavailable-state presentation and must not interrupt menus or enable unrelated background updates.
- Long menus and a delayed refresh opportunity must not cause a burst of catch-up updates. Status should use the latest available reading at the next opportunity.
- Sleep and screensaver transitions take precedence over awake menu refresh; returning to an awake menu restores status refresh without advancing paused content through the inactive interval.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: While any menu covers the main screen, EmulationStation MUST update only main-screen components explicitly designated for background refresh. A component's designation MUST NOT implicitly enable updates of its layout, siblings, ancestors, or unrelated descendants.
- **FR-002**: Existing clock, Wi-Fi, and battery status components, including multiple instances, MUST receive this behavior automatically without theme edits, replacement components, or user configuration.
- **FR-003**: During normal awake menu operation, designated components MUST receive refresh opportunities at a nominal interval of 100 ms, with an average of 9–11 updates per second over a continuous 10-second observation window. This cadence concerns refresh opportunities, not a requirement for readings or displayed text to change that often.
- **FR-004**: Each status refresh MUST use the latest reading available through the existing status behavior and preserve existing display formats and unavailable-state handling. The feature MUST NOT require faster external status-data acquisition.
- **FR-005**: All covered main-screen components without explicit designation, including lists, carousels, and decorative animations, MUST retain their paused state throughout menu coverage, regardless of redraws or status refreshes.
- **FR-006**: Covered main-screen components, including designated status components, MUST NOT process menu input or queue that input for later background actions. Existing global input handling MUST remain unchanged.
- **FR-007**: Closing the final covering menu MUST restore normal main-screen updates from preserved state without applying elapsed menu time, replaying background input, or processing accumulated background update work. Closing only a submenu MUST NOT resume the main screen.
- **FR-008**: Redrawing the covered scene MUST preserve existing layering, clipping, dimming, and blur and MUST NOT advance otherwise paused state. Whole-scene redraws are permitted; reducing rendering to designated components is outside the required scope.
- **FR-009**: Menu interaction, foreground animations, and notification appearance, duration, and dismissal MUST retain their existing behavior and cadence independently of the background status-refresh limit.
- **FR-010**: Existing idle, sleep, and screensaver rules and transitions MUST remain unchanged. Background refresh MUST NOT simulate user activity, inhibit those transitions, or bypass their existing restrictions on updates.
- **FR-011**: Rapid menu transitions, long menu sessions, delayed refreshes, and wake/resume cycles MUST NOT produce catch-up bursts or advance paused content through inactive time. Once normal awake menu operation resumes, designated status refresh MUST return to its normal cadence.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: During a 10-second awake menu session, each designated status component receives 90–110 refresh opportunities, and a changed reading already available to the indicator appears by its next refresh opportunity.
- **SC-002**: Across menu sessions of 1 second and 60 seconds, covered lists, carousels, and decorative animations show zero unintended changes in selection, position, or animation progress while paused, and zero jumps or catch-up advancement when resumed.
- **SC-003**: Across 20 menu open/close cycles including nested menus and held navigation input, zero menu inputs trigger covered-screen actions and normal main-screen updates resume after every final menu close.
- **SC-004**: Unmodified themes containing the existing clock, Wi-Fi, and battery components pass all applicable status-refresh scenarios, including a theme with multiple indicator instances and one with indicators inside paused layouts.
- **SC-005**: All presentation and lifecycle acceptance scenarios in User Story 3 pass against the established behavior, with no observable change to menu responsiveness, foreground animation cadence, notification lifetime, layering, clipping, dimming, blur, or configured sleep/screensaver transitions.

## Assumptions

- “Approximately 10 updates per second” means the nominal 100 ms cadence and 9–11 updates-per-second average defined above under normal awake operation; this is not a hard real-time guarantee during system stalls, sleep, or screensaver activity.
- “Menus” includes nested menus and submenus that currently pause the covered main screen. Foreground notifications alone do not redefine when that screen is covered by a menu.
- Clock, Wi-Fi, and battery indicators are the required initial automatic designations. Other main-screen component types remain paused unless explicitly designated; a theme-facing designation option or settings interface is not required.
- Existing status-data availability, display formatting, global input behavior, and idle/sleep/screensaver behavior are dependencies and remain the baseline for comparison.
- Paused state preservation applies to ordinary covered-screen progression and input. A deliberate menu action that changes a setting or displayed content still takes effect through its existing behavior.
- This feature requires no new persistent user data, theme migration, rendering optimization target, or change to external status-data providers.
