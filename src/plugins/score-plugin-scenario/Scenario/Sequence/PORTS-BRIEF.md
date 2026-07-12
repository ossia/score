# Brief for JM — Sequence per-parameter ports: the dynamic-port problem

## Context

The Sequence process (`Scenario/Sequence/`) now follows the "one port per
automation for the whole sequence" model you suggested:

- `SequenceModel` owns one `Process::ValueOutlet` per namespace parameter
  (plus an `AudioOutlet` at index 0). Port ids are serialized so cables
  survive save/load; the ports are flagged `displayHandledExplicitly` and
  drawn by the sequence presenter **once per slot row**, standing for all
  the per-section instances of that row's process. The per-section
  automations'/gradients' own ports are hidden the same way.

- Execution: the sequence's process node (an `ossia::scenario`'s
  `forward_node`) is substituted with a subclass carrying one value
  inlet/outlet pair per parameter. At `lazy_init`, one
  `immediate_glutton_connection` edge is created from each section
  automation/gradient node output to the matching value inlet; the node
  copies inlets → outlets each tick. Since sections play sequentially, each
  outlet carries the merged, continuous stream of the whole sequence for
  that parameter. Document cables bind generically through the standard
  index-matched `register_node`.

This is the same shape as your clip-launcher lanes (one port for N
time-distributed instances). When the generalized `Outlet::propagate` from
PR #1989 lands, the hand-made edges here should be replaced by
`setPropagate(true)` on the instance outlets — happy to do that migration.

## The open problem: ports added during playback

Adding a parameter to the sequence **while the transport runs** updates the
document model (new `ValueOutlet`, new per-section automations — the live
structural mutation itself works, sections and processes are picked up),
but the *execution* node's port array is fixed at play start:

1. `sequence_node` is constructed with `parameterNamespace().size()` pins in
   the component constructor; its `m_inlets`/`m_outlets` vectors are read by
   the audio thread every tick. Growing them live is a data race unless done
   through the execution queue *and* the graph re-sorts its port topology.
2. `register_node` has already bound document ports ↔ node ports by index;
   a new document outlet has no ossia counterpart until re-registration.
3. The forwarding edges for the new parameter's per-section automations
   would also need creating against components that were built mid-play.

Current behavior: the new parameter plays correctly (device writes go
through the automations as usual) but its sequence port only becomes
cable-able after a transport restart. Acceptable for now, and consistent
with other structural-change-during-play limitations.

## Questions

1. Is there an existing sanctioned path for growing a live `graph_node`'s
   port arrays through the execution queue (something the JS process or
   Faust reload path uses), or is full component re-creation
   (`unregister_node` → new node → `register_node` → re-edge) the intended
   move?
2. Does the #1989 `propagate` generalization change the answer — i.e. will
   propagated outlets route through parent forward nodes without the parent
   needing a pre-sized pin per stream? If so, the sequence could drop its
   fixed pin array entirely and this problem disappears.
3. Minor: `value_port` merging — multiple edges into one value inlet (one
   per section instance) relies on ordinary port merging; sections are
   temporally disjoint so there's no simultaneous-writer case, but flag it
   if that assumption is unsafe with `immediate_glutton_connection`.

— Pia & Claude, 2026-07-12
