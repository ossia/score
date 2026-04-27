# Sequence Process — Deep Architecture Review

> **For:** the agent continuing implementation of `Sequence::SequenceModel` in this worktree.
> **Status:** v2 (2026-04-27).
> **Companion docs:**
> - `IMPLEMENTATION_REPORT.md` — what the previous agent shipped.
> - `missing_design_choices.md` — ambiguous design decisions with
>   best-guess answers; surface to the user before proceeding.
> This document is the *critical reading* of the implementation.

---

## TL;DR

The current `SequenceModel` correctly chose the heavyweight option: it embeds a
sub-scenario (its own `EntityMap`s of `IntervalModel` / `EventModel` /
`TimeSyncModel` / `StateModel`) and implements `Scenario::ScenarioInterface`.
That's the right call for a multi-section process — it's exactly what
`Scenario::ProcessModel` itself does, and it gives the new feature free access
to all existing scenario algorithms (`startEvent()`, `endEvent()`,
`AddProcess`, ID-scoping for path resolution, etc.).

But several details of the implementation will break in subtle ways under
real use (undo/redo with stable IDs, copy/paste, executor cleanup, drop
handlers from device explorer, snapshot policy). The high-leverage fixes
documented below should be applied **before** more user-visible work is
piled on top.

There is also a large amount of duplicated execution code that should
either be refactored to share with `Scenario::ScenarioComponent` or kept
duplicated with an explicit "diverge from scenario when…" comment.

This document is opinionated about which path to take where it matters.

---

## 1. Architectural sanity check: is the chosen pattern correct?

**Question.** Should `SequenceModel` be a self-contained sub-scenario, or
should it use a flatter structure (e.g., a list of named "section" objects
with embedded curves, à la `Curve::Model` segments)?

**Answer (recommendation):** keep the current sub-scenario pattern. Reasons:

1. **Process composition.** Each section needs to host arbitrary processes
   in addition to the linked-namespace automations (the user will want to
   drop sound files, ISF effects, sub-scenarios into a section). Modeling
   sections as a flat segment list would re-implement most of `IntervalModel`.
2. **Reuse of executor.** A sub-scenario maps cleanly onto
   `ossia::scenario` + `ossia::time_interval` + `ossia::time_event` +
   `ossia::time_sync`. A flat-segments approach would need a new ossia node
   class.
3. **Reuse of `ScenarioInterface`.** A large body of existing code
   (creation gestures, cohesion macros, presenter algorithms, magnet/snap
   helpers, selection plumbing) accepts any `ScenarioInterface`-conforming
   container. Keeping that contract gives sequence-aware features for free.

**Required clarifications (carry forward into design choices):**
- The "namespace" attached to the sequence is the differentiator: it's a
  list of `State::AddressAccessor` whose values must be defined at every
  IS (intermediate state). Sections are intervals, IS are timeSyncs.
- Boundary IS (start/end of the sequence as a whole) mirror the *parent*
  interval's start/end states.

**Closest precedent in the codebase: `Nodal::Model`.**
Files: `src/plugins/score-plugin-nodal/Nodal/{Process.hpp,Process.cpp,Presenter.hpp}`.
Nodal is the simplest existing "container ProcessModel" — it owns
`score::EntityMap<Process::ProcessModel> nodes`, overrides the three
`setDurationAndScale/Grow/Shrink` and the two
`ancestorStartDateChanged / ancestorTempoChanged` virtuals,
implements `loadPreset/savePreset`, and ships a `score::ObjectEditor`
subclass for copy/paste/remove on its children. Its presenter
embeds child views via `Process::NodeItem`. Several gaps in the
current `SequenceModel` (§3.8 below) come down to "we forgot to
mirror what Nodal does." Always cross-check against Nodal when in
doubt about the contract a container ProcessModel must fulfil.

---

## 2. Backwards-compatibility audit

The sequence work to date is **purely additive** at the file level:
- No existing class definitions are changed in a way that touches
  serialization payloads.
- The only new fields appear inside `SequenceModel`, which is itself a new
  process type — old files do not contain it, so nothing to migrate.
- `score_plugin_scenario.cpp` registers the new factories alongside the
  existing ones; the plugin UUID bump is unnecessary because no public ABI
  shrunk.
- The scenario palette state machine (`ScenarioCreationState.hpp`,
  `ScenarioCreation_FromState.hpp`) is patched in *additional* branches
  guarded by `tool() == Tool::CreateSequence`. The non-sequence path is
  unchanged.
- `ScenarioRollbackStrategy.cpp` adds two new keys to the rollback list;
  pre-existing keys keep working.

**Verdict: no break.** No score document written by an older build will fail
to load on the new build. New documents that contain a `SequenceModel`
will not load on older builds — but that is expected for any new process
and is not a regression.

There is **one latent risk** to flag (see §5): the on-disk serialization of
`SequenceModel` itself is brand new and has not yet been "frozen". As long
as the feature is unreleased, the serialization format may still change.
If a release ships before fixes land, future breakage will be a true
backwards-compat hazard. Recommendation in §5.

---

## 3. Critical bugs in the current implementation

The following are *correctness bugs* that should be fixed before any
follow-on UX work. They are listed in order of severity.

### 3.1 `AppendSequenceSection` regenerates IDs on every redo — breaks undo/redo

`AppendSequenceSection::redo` calls `seq.appendSection(m_duration)`, which
calls `getStrongId(seq.timeSyncs)` and friends. The new IDs are stored
into `m_info` each redo. After undo → redo, the freshly created entities
have *different* IDs from the originally-created ones, so any
`Path<>` (held by selections, inspector widgets, executor components,
slot/layer presenters) becomes dangling.

**Fix.** Generate the new IDs in the *constructor* of
`AppendSequenceSection` (using `getStrongIdRange` over the relevant entity
maps), store them in `m_info`, and pass them as parameters to a new
`SequenceModel::appendSectionWithIds(const AppendedSection& info, TimeVal duration)`.
Match the pattern of `CreateInterval_State_Event_TimeSync`
(`Scenario/Commands/Scenario/Creations/`).

### 3.2 `CreateSequence` calls `cmd->redo(ctx)` on each subcommand and uses `submitQuiet`

The `make` function calls `redo(ctx)` on each subcommand inline, then
ultimately `m_dispatcher.submitQuiet(cmd)`. The other creation commands
(`CreateInterval_State_Event_TimeSync`) follow this same pattern, so this
is in fact the established convention here — fine. Two issues remain:

- The aggregate `addCommand(create_command)` calls happen *after* the
  redo, but `addCommand` is declared on `score::AggregateCommand`. We
  need to check that the aggregate's own `undo()` walks subcommands in
  reverse and calls undo on each (yes, it does — see
  `score/command/AggregateCommand.cpp`). Good.
- The `AddSequenceParameter` subcommands appended at the end create
  **flat 0.5** curves (see §3.4) — the redo pre-creates them, but on
  *load from disk* the redo path is not re-run, so the flat curves are
  whatever they got serialized as. OK in practice.

Recommendation: leave the redo-inline pattern alone (it's idiomatic in
this codebase) but extract the "find the SequenceModel inside the
interval" loop into a helper since the same pattern is duplicated in
`ExtendSequence::make`, `ScenarioCreationState::createToNothing_base`,
and `ScenarioCreation_FromState::move_nothing.entered`. Add a free
function `Sequence::findSequenceProcess(const IntervalModel&)` returning
`SequenceModel*`. Single source of truth, easier to maintain.

### 3.3 Extend-drag state machine is fragile (sentinel IDs + per-frame rollback)

`ScenarioCreationState::createToNothing_base` populates the parent's
`createdIntervals/Events/States/TimeSyncs` with **the moving end
elements** so that `mapWithCollision` (used by other transitions) does
not re-pick them up. Combined with the rollback-and-recreate-every-frame
loop in `ScenarioCreation_FromState`, this produces visible flicker and
is the root of the "Sequence extend drag bug" recorded in memory.

The cleaner design is to give the extend gesture its own `QState` in
`Creation_FromState`, with explicit transitions and an
`OngoingCommand` (subclass of `score::Command` with `update()`)
analogous to `MoveNewEvent`/`MoveNewState`. This avoids both the
rollback-recreate flicker and the sentinel-ID hack.

**Concrete refactor sketch:**
1. Promote `ExtendSequence` from `AggregateCommand` to a concrete
   `score::Command` that owns a `MoveEventMeta` + `AppendSequenceSection`
   internally and exposes `update(newDate, y)` which forwards to the
   inner `MoveEventMeta::update(...)` and (if no IS yet at this section)
   creates an IS.
2. Add a new `ExtendSequence_OnGoing` state in `Creation_FromState`
   guarded by `isExtend`. This state submits the ongoing command on
   entry, calls `update()` on every move, and `commit()` on release. It
   does **not** rollback between frames.
3. Remove the sentinel-ID branch and the per-frame
   rollback-and-recreate.

This avoids touching `mapWithCollision` and the rollback strategy at all.
Keep the sentinel patch in place as an emergency stop only if the cleaner
state machine cannot be landed in the same change.

### 3.4 Flat-0.5 curves are unconditionally created

`SequenceModel::addParameter` initialises every section's automation
with a flat curve at value `0.5` (normalized). This is wrong for any
parameter whose domain is not `[0,1]` and ignores the actual current
device value.

**Fix.** Read the actual current value of the parameter from the
device explorer (`DeviceDocumentPlugin::rootNode()` →
`Device::AddressSettings::value`) at `addParameter()` time, normalize it
into the curve's `[0,1]` Y axis using the parameter domain, and write
that value at every IS for that parameter. Curve points (start and end of
each segment) should mirror the IS values of the surrounding boundary
states.

This is exactly what the existing `CreateSequenceProcesses` code path
does for the legacy auto-sequence — the same domain/value conversion
helpers (`State::convert::value<double>`,
`Curve::CurveDomain`) should be reused.

### 3.5 `setISValue` is a placeholder

The current implementation only calls `curve.changed()` and never updates
the segment Y values nor the message-tree state. As a result, dragging an
IS handle vertically (a UX feature called for in §6) does nothing.

**Fix.** Implement properly:
1. Update `MessageItemModel` of the IS state with the new value.
2. For the automation curves on the left and right adjacent intervals,
   move the corresponding endpoint of the relevant `Curve::SegmentModel`
   (`segs.back()->setEnd({...})` for the arriving curve,
   `segs.front()->setStart({...})` for the leaving curve), then call
   `curve.changed()`.
3. If the IS is "frozen" for that parameter, propagate forward through
   consecutive frozen IS (already partially implemented).

### 3.6 `SequenceExecution.cpp` is a copy-paste of `ScenarioExecution.cpp`

This is ~459 lines of duplicated logic. The risk is divergence: any bug
fix or feature added to `ScenarioExecution` will need to be mirrored here.

**Recommendation.** Extract the common machinery into a
`ScenarioComponentBaseT<Container>` template parameterised on the
container model type (`Scenario::ProcessModel` or `Sequence::SequenceModel`).
Both already implement `ScenarioInterface`; templating by the model type
should require no new virtuals. Targeted file:
`src/plugins/score-plugin-scenario/Scenario/Document/Components/ScenarioComponent.hpp`.

This is non-trivial — punt to a follow-up PR if needed, but **mark the
duplicated file with a `// FIXME: extract` comment** and include the
extracted template in the next milestone. Otherwise the technical-debt
will calcify.

### 3.7 Boundary IDs hardcoded to 0 / 1

`SequenceModel` constructor sets:
```cpp
m_startTimeSyncId = Id<Scenario::TimeSyncModel>(0);
m_endTimeSyncId   = Id<Scenario::TimeSyncModel>(1);
```
This relies on the convention that `Scenario::startId<TimeSyncModel>() == 0`
(used by `SequenceComponentBase::make<TimeSyncComponent,…>` to attach the
ossia start sync). Same for events and states.

Issue: `getStrongId` always picks an unused ID, so on a fresh
`SequenceModel` constructor this works. But:
- **Copy-paste / clone:** if a sequence is cloned through the
  copy-paste machinery, the IDs may collide with the surrounding
  scenario's IDs. (They don't actually, because each `EntityMap` is its
  own ID namespace — verify in §6 of this report.)
- **Robustness:** there is no assert that 0 / 1 were unused. A future
  refactor that pre-creates entities before the constructor runs would
  silently corrupt.

**Fix.** Replace literal `Id<...>(0)` / `(1)` with
`Scenario::startId<TimeSyncModel>()` (and add an `endId<>` if not
present). Document the invariant in a comment in `SequenceModel.hpp`
near the field declarations.

### 3.8 `setDurationAndGrow` / `setDurationAndShrink` do not propagate to children

`SequenceModel::setDurationAndScale` correctly rescales every timeSync,
event, interval, and child process. But `setDurationAndGrow` and
`setDurationAndShrink` only call `setDuration(newDuration)` on the
sequence itself — they do **not** update the section intervals or
child processes.

Concretely: drag the parent interval's right edge in `Grow` mode, the
sequence's reported duration changes, but its sections keep their
old durations. The sum of section durations no longer equals the
sequence duration — the executor will run off the end (or stop
short).

**Fix.** Mirror `Nodal::Model` (the closest container-process
precedent in the tree):
```cpp
void setDurationAndGrow(const TimeVal& newDuration) noexcept override {
  // strategy: grow the LAST section to absorb the slack
  auto& last_itv = *--intervals.end();
  auto cur = duration();
  auto extra = newDuration - cur;
  last_itv.duration.setDefaultDuration(
      last_itv.duration.defaultDuration() + extra);
  // …also move every TimeSync after the last sync forward by `extra`
  // …also re-anchor the end TimeSync at newDuration
  for(auto& proc : last_itv.processes)
    proc.setParentDuration(ExpandMode::GrowShrink,
        last_itv.duration.defaultDuration());
  setDuration(newDuration);
}
```
For `Shrink`, refuse if the new duration would be smaller than the
sum of the non-last sections (or compress the last section to a
minimum). Match the choice from `missing_design_choices.md` §I.

### 3.9 `m_parentStartStateId` / `m_parentEndStateId` reconstruction is unreliable

These are derived from `qobject_cast<IntervalModel*>(parent)` in both
constructors. But during `Process::ProcessModel` *deserialization*, the
parent QObject hierarchy may not yet be set up the way construction
expects (the parent argument is the deserialization-time parent, not
necessarily the eventual interval — actually it is, but only because
process deserialization is always parented under the host interval).

Verify by inspection in `Process::ProcessModel(DataStreamReader&, …)`: the
`parent` arg is propagated. Should be safe but **add an assertion** in
both constructors:
```cpp
SCORE_ASSERT(qobject_cast<Scenario::IntervalModel*>(parent) != nullptr);
```
to fail loudly if a sequence is ever embedded outside an interval (e.g.
top-level inside another sequence's loop body, etc.).

---

## 4. Code organisation issues

### 4.1 New files live under `score-plugin-scenario` rather than their own plugin

The current location keeps everything in one plugin (no new CMake target,
no inter-plugin export), which is the right call for now: the sequence
shares so much with the scenario (palette, state machine, executor)
that splitting them would create a circular dependency. Keep here.

But: the namespace is `Sequence::` (sibling to `Scenario::`). The folder
is `score-plugin-scenario/Scenario/Sequence/`. That mismatch is a
maintenance trap. Either:
- (preferred) move folder to
  `score-plugin-scenario/Sequence/` (sibling of `Scenario/`) and keep
  the `Sequence::` namespace, OR
- (compromise) keep the folder where it is, but rename namespace to
  `Scenario::Sequence`.

### 4.2 `CreateSequenceProcesses` is dead code in the new path

In the rewritten `CreateSequence::make`, the legacy
`CreateSequenceProcesses` command (which interpolates sibling-process
automations between start/end states) is no longer wired up. But the
class is still defined in `CreateSequence.{hpp,cpp}` and exported.

If the user wants the legacy "auto-sequence" gesture to keep working
*without* a `SequenceModel` (i.e., the pre-existing tool behaviour
unchanged), `CreateSequenceProcesses` must still be called. Per the
implementation report, "remove redundant old-style automations from
CreateSequence" was a deliberate commit. Confirm this is the intended
scope reduction with the user. If yes, delete `CreateSequenceProcesses`
entirely. If no, restore it under a settings flag (e.g. behave like
legacy when `getAutoSequence()` is true *and* sequence-process is
disabled in settings).

### 4.3 Inspector lists "sibling processes" addresses as read-only filler

The inspector widget displays sibling processes' outlet addresses below
the namespace list. This conflates two concerns. Either:
- the sequence should use those addresses *automatically* (then make it
  not read-only and offer a one-click "track" button), or
- they should not be shown (clutter).

Recommend hiding them by default; add an "Add from siblings…" menu
button if discoverability is a concern.

---

## 5. Serialization & forward-compat

The DataStream serializer in `SequenceModelSerialization.cpp`:
1. Does not version the payload. If a field is added in the future, no
   migration path exists.
2. Reads/writes `m_namespace` as a raw `int32_t count + N elements`,
   bypassing Qt container serialization. Same for `m_frozenParams`. This
   is OK but not idiomatic — the codebase usually serializes `QList`
   directly via `m_stream << container`.

**Fix (low-cost, high-leverage, do BEFORE any release ships):**
- Wrap the entire payload in a single `int32_t version = 1` prefix on
  DataStream. On read, branch on it. JSON does not need this (key
  presence is forward-compatible).
- Use the standard Qt-stream operators where possible.
- Add the score "delimiter" pattern (`insertDelimiter()` /
  `checkDelimiter()`) — already present, just verify it's at the right
  layer relative to the version field.

If a release ships the *current* unversioned format and we later need
to add a field, we will need a runtime-detected migration. Avoid by
versioning now.

---

## 6. Identifier scoping (verified safe — for the record)

`score::EntityMap<T>` is per-parent. `Id<T>` values are unique only
within a given `EntityMap`. `getStrongId(map)` returns
`max(existing) + 1`. So two `EntityMap<IntervalModel>` instances (one in
the parent scenario, one in the sequence) can reuse the same numeric
IDs without conflict, as long as path resolution walks through the right
parent.

`Path<T>::find(ctx)` walks the QObject tree from the document root and
matches each segment by `objectName()` and `Id<T>`. A `Path<IntervalModel>`
into a sequence's interval includes the
`Scenario::ProcessModel → IntervalModel → SequenceModel → IntervalModel`
chain. Resolution is unambiguous.

`SequenceComponentBase` keeps its own
`hash_map<Id<IntervalModel>, IntervalComponent>`, scoped to the
sequence. Same isolation as scenario components. Safe.

The hardcoded `0`/`1` IDs in §3.7 do *not* cause cross-sequence
collisions because each new `SequenceModel` has its own empty entity
maps at construction. The risk noted in §3.7 is robustness against
future refactors, not a present bug.

---

## 7. Process system contracts to preserve

`Process::ProcessModel` has several optional virtuals
(`startStateData()`, `endStateData()`, `selectableChildren()`,
`setSelection()`, `loadPreset()`, `savePreset()`). The current
`SequenceModel` implements `selectableChildren`, `selectedChildren`,
`setSelection`, but **not** `startStateData` / `endStateData`.

`startStateData` returns the data that feeds the parent interval's
*start state* on the upstream side. Since the sequence acts as a
state-mutating process, returning the namespace's resolved values at
`m_startTimeSyncId` would let the parent state pre-fill its own
message tree from the sequence. This is the right hook for the
behaviour described in `IMPLEMENTATION_REPORT.md` ("Parent boundary
state IDs"). Implement both.

`loadPreset` / `savePreset` should serialise just the namespace and
section structure — useful for the "save sequence as preset" UX. Not
urgent, but easy to plug in once §3.5 is resolved.

---

## 8. Drop handling from the device explorer

The current implementation accepts drops *only on the inspector widget*.
The architectural pattern in score is:
- `Process::ProcessDropHandler` → drops onto an interval; creates a
  process.
- `Process::IntervalDropHandler` → drops with custom semantics.
- `Process::AddressDropHandler` → drops directly on a process layer.

For sequences, the right shape is an `IntervalDropHandler` subclass
specialised to detect sequences that:
1. Accepts `score::mime::nodelist()` MIME (already what the inspector
   does, and same MIME the existing
   `Scenario::AutomationDropHandler::drop` consumes).
2. If the target interval contains a `SequenceModel`, submit
   `AddSequenceParameter` for each address (and skip the default
   "create curve in interval" path).
3. Otherwise return `false` and let `AutomationDropHandler` handle
   it.

**Registration.** Add to the `FW<Scenario::IntervalDropHandler, …>`
list in
`score-plugin-scenario/score_plugin_scenario.cpp` (line ~280),
**before** `Scenario::AutomationDropHandler` so it has first refusal:
```cpp
FW<Scenario::IntervalDropHandler,
   Scenario::DropProcessInInterval, …,
   Sequence::SequenceDropHandler,        // NEW — must precede AutomationDropHandler
   Scenario::AutomationDropHandler, …>,
```
The dispatch loop tries handlers in order until one returns `true`.

Move the drop logic out of the inspector and into the new dedicated
`SequenceDropHandler`. The inspector becomes display-only.

---

## 9. Slot management & layer presentation

`SequenceLayerFactory = LayerFactory_T<SequenceModel, SequencePresenter, SequenceView>`.
The presenter sets `recommendedHeight = false`, meaning the slot keeps
whatever height the user gave it. That is the right default for a
container.

But: the sequence layer itself only renders **handle lines**. The
sections' processes (curves) live inside the child intervals, which are
not displayed by `SequencePresenter`. So how does the user see the
curves?

Two design options:
- **(A) Inline curves in SequencePresenter.** Render mini-curves for
  each namespace parameter at child interval positions, similar to a
  pianoroll. Heavy: requires re-implementing curve rendering.
- **(B) Nested rack.** The sequence's sections are rendered as nested
  `IntervalView`s with their own slots inside the parent slot. The
  scenario's existing rack-rendering applies.

**Recommendation: (B)**, but recognise the parent presenter will need
to host child interval presenters. This is exactly what
`FullViewIntervalPresenter` does for the top-level scenario's intervals.
Reuse `Scenario::TemporalIntervalPresenter` to render each section.
This is a non-trivial follow-on but is *the* missing piece between the
current "lines only" view and a usable sequence editor.

If (A) is preferred for compactness, scope it to a "compact mode"
toggle on the layer.

---

## 10. Testing recommendations

There are no tests in the worktree for the sequence model. Before more
features stack on top, add at least:
- A round-trip serialization test (DataStream + JSON) for an empty
  sequence, a sequence with a few sections, and one with frozen
  parameters.
- An undo/redo test for `AppendSequenceSection` that asserts ID
  stability across redo cycles (this catches §3.1 directly).
- An integration test that creates a sequence, adds a parameter,
  appends a section, then undoes the parameter add — must not crash and
  the section must still be present.

Files to look at for the existing test harness:
`tests/Scenario/`. Add `tests/Scenario/SequenceTests.cpp` mirroring
existing patterns.

---

## 11. Open investigation items — answered

- [x] **AutomationDropHandler registration.** It's just a class with
      `SCORE_CONCRETE("uuid")` listed in
      `score_plugin_scenario.cpp` under
      `FW<Scenario::IntervalDropHandler, …>` (line ~280). Register
      `Sequence::SequenceDropHandler` in the same list, *before*
      `AutomationDropHandler` so it can intercept drops on intervals
      that host a `SequenceModel`. See §8 for full detail.

- [x] **No `LoopProcess` exists in the current tree** — only a
      commented-out reference in
      `score_plugin_scenario.cpp:256-257`. The closest "container
      ProcessModel" precedent is `Nodal::Model`
      (`src/plugins/score-plugin-nodal/Nodal/Process.{hpp,cpp}`):
      child `EntityMap<Process::ProcessModel>`, presenter that
      embeds child views (`Process::NodeItem`), proper override of
      `setDurationAndScale/Grow/Shrink` and `ancestorStartDateChanged
      / ancestorTempoChanged`. Matches what `SequenceModel` should
      do (see §3.8 — current Sequence misses the Grow/Shrink
      propagation).

- [x] **No external users of `Scenario::startId<...>()`** that
      assume the IDs are anywhere other than inside `Scenario::ProcessModel`.
      Replacing the literal `0/1` with `Scenario::startId<>` /
      `Scenario::endId<>` in `SequenceModel` is purely additive — no
      breakage risk. (See §3.7.)

- [x] **Layer rack does not "decide" container-shape.** The
      `LayerFactory_T<Model, Presenter, View>` simply hands the
      slot a presenter/view; the presenter is responsible for
      rendering its own children however it wants. For sequences
      that means: (a) `SequencePresenter` decides whether to render
      mini-curves or nested `IntervalView`s, and (b) `View::height()`
      is whatever the slot was sized to. There is no codified
      "container" interface — just convention.

- [x] **Path resolution into nested entity maps works.**
      `Path<IntervalModel>::find(ctx)` walks the QObject tree using
      `objectName()` keys; `EntityMap<T>` parents its entries via
      `T::setParent(this->m_owner)`, so a sequence's intervals are
      true QObject children of `SequenceModel`, which is a child of
      its host `IntervalModel`, which is a child of `Scenario::ProcessModel`,
      etc. Path resolution is unambiguous. (Confirmed via §6.)

---

## 12. Recommendation ranking

If picking up where this leaves off, do in order:

1. **§3.1** — pre-allocate IDs in `AppendSequenceSection` constructor
   (tiny change, fixes a real correctness bug — broken Path<>
   after undo→redo).
2. **§3.4** — read actual current value at `addParameter` time,
   write it as start/end Y of every segment (fixes the "no
   automation" complaint, Bug 1 in `project_sequence_extend_bug.md`).
3. **§3.5** — wire `setISValue` to update both message tree and
   adjacent curve endpoints (currently a no-op stub). **DONE 2026-04-27.**
4. **§3.8** — propagate `setDurationAndGrow / Shrink` to child
   sections (currently silent corruption when parent interval is
   resized — sections desync from sequence duration).
5. **§5** — version the SequenceModel serializer before any release
   ships an unversioned format.
6. **§3.3** — replace the sentinel-IDs + per-frame rollback hack
   with a clean `ExtendSequence_OnGoing` palette state (fixes Bug 2
   in memory + flicker). **DONE 2026-04-27.** ExtendSequence promoted
   to concrete ongoing Command with update(); state machine now calls
   submit<ExtendSequence> each frame instead of rollback+recreate.
7. **§9** — pick a layer-presentation strategy (B nested rack
   recommended; see `missing_design_choices.md` C) and implement.
   **DONE 2026-04-27.** SequenceView handles refactored to
   QGraphicsLineItem children (z=10); SequencePresenter creates one
   TemporalIntervalPresenter per section interval, positioned via
   updateSectionLayout(). Builds clean.
8. **§3.6** — extract the duplicated executor into a templated
   base (459 lines duplicated from ScenarioExecution.cpp).
9. **§8** — split drop logic out of inspector into a registered
   `Sequence::SequenceDropHandler` (use the registration order
   pattern called out there).
10. **§3.7** — replace literal `Id<...>(0)/(1)` with
    `Scenario::startId<>` / `Scenario::endId<>` (small, clean).

**Suggested PR groupings:**
- PR 1: items **1-5** (correctness + serialization safety net) —
  required before any user-visible polish.
- PR 2: item **6** (palette state machine refactor) — independent.
- PR 3: item **7** (presenter/view, nested rack) — likely the
  biggest delta.
- PR 4: items **8-10** (refactor + drop handler + ID hygiene) —
  cleanup, can defer.

---

*End of v2 — see commit history of this file for revisions.*
