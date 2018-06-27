---
title: Process overview: Interpolation
---

<span style="color:red;font-weight: bold">DISCLAMER: as of 2.0.0.a6, interpolation process is broken as new implementation is in progress</span>

# Process overview: Interpolation

## Presentation
The interpolation is very similar to the automation.
It sends a set of values over time, given by a curve, and it is green instead of red.
However, instead of sending values that graphically looks like the curve, it sends values
that are interpolated between the first and last state of the parent interval for its address.

<img class="wp-image-1334 size-full" src="http://ossia.io/wp-content/uploads/2016/11/interpol.png" alt="interpol" width="237" height="154" />

## Edition
From an edition point of view, an interpolation is identical to an automation : it is just a curve.

## Execution

In order to work, an interpolation requires :

* An address
* A start state with a value for this address
* An end state with a value for this address

For instance, with comment blocks to show what's in the states :

<img class="alignnone size-medium wp-image-1335" src="http://ossia.io/wp-content/uploads/2016/11/interpol-2-300x174.png" alt="interpol-2" width="300" height="174" />

For now, this curve behaves exactly like an automation : at t=0, the value 0 will be sent, at mid-course, the value 25, and at the end, the value 50.

A first difference arises if the states are in the other order :

<img class="alignnone size-medium wp-image-1336" src="http://ossia.io/wp-content/uploads/2016/11/interpol-3-300x181.png" alt="interpol-3" width="300" height="181" />

Here, even though the curve seems to increase, the value will actually do 50, 49, ..., 25, ..., 0.
Basically:

* A point at the <strong>bottom</strong> of the curve has the value of the <strong>start</strong> start
* A point at the <strong>top</strong> of the curve has the value of the <strong>end</strong> state

The interpolation is also useful with arrays : each array value will be interpolated one-by-one.
For instance, in this case :

<img class="alignnone size-medium wp-image-1337" src="http://ossia.io/wp-content/uploads/2016/11/interpol-array-300x189.png" alt="interpol-array" width="300" height="189" />

The sent values will look like [ 10, 0, 5 ],  [ 9, 1, 5 ], ..., [ 5, 5, 5 ], ..., [ 0, 10, 5 ].

Values of non-interpolable types (strings) will just be copied for each sent message.