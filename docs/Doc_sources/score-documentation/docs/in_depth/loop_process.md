---
title: Process overview: Loop
---
# Process overview: Loop

## Presentation

The loop is somewhat similar to the scenario.

It is a process that allows to loop other processes.

However no new structures can be created or removed in it.

It is built of :

* A first sync, event, and state
* A time constraint
* A last sync, event, and state

When created, it looks like this :

<img class="alignnone size-full wp-image-1357" src="http://ossia.io/wp-content/uploads/2016/11/loop.png" alt="loop" width="277" height="163" />

## Edition

The last event can be resized :

![Loop resize](../images/loop-resize.gif)



Processes can of course be added to the interval, as well as data to the states.

## Execution

The loop process, as its name tells, loops :

![Loop execution](..images/loop-exec.gif)

To make an infinite loop, one can for instance remove the maximum of its parent interval :

<img class="alignnone size-full wp-image-1360" src="http://ossia.io/wp-content/uploads/2016/11/infinite.png" alt="infinite" width="295" height="191" />

To make an interactive loop, a trigger can instead be added at the first or last trigger of the loop :

![Interactive loop](../images/loop-interact.gif)