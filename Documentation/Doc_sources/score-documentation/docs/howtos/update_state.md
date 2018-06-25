---
title: How to update parameters value
---

# How to update parameters value
## Using the refresh function

When using Score with a device providing bidirectional protocol such as OSCquery, Score listens to parameters changes in the controlled device. Stored states can then be updated by selecting the state to update on the timeline and click the 'Refresh state' button in the toolbar or use `cmd + u` on macOS or `ctrl + u` on Windows.

If your device can only receive messages from Score, in the Device explorer, double-click on a parameter's value textfield and type the desired value. Now lick the 'Refresh state' button in the toolbar or use `cmd + u` on macOS or `ctrl + u` on Windows.

![Update state](../images/update_state.gif)

## Using the inspector

Alternatively, stored states can be edited from a state inspector.

On the timeline, select the state to edit to display its inspector. From the tree view of the stored addresses and value, double-click on the value textfield and type the desired value.

## Drag’n’dropping from the Device Explorer

On can also update the existing parameters from a state by drag’n’dropping them from the Device Explorer.

This allows to:
- only update a portion of the parameters
- add new parameters while updating those already present.

When doing so, the parameters already present will be updated, when applicable. Those that were not present beforehands will be added with the current value from the Device Explorer