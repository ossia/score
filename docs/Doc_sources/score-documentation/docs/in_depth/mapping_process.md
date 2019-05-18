---
title: Process overview: Mapping
---

# Process overview: Mapping

## Presentation

The mapping process allows to map  input parameters to output parameters in real-time.
It is a simple transfer function.
It is based on a curve, just like the automation.
However, it is a time-independent process : instead of mapping the time to a value, it maps an input value to an output value.

Its curve is purple.

<img class="alignnone size-full wp-image-1342" src="http://ossia.io/wp-content/uploads/2016/11/mapping.png" alt="mapping" width="241" height="158" />

## Edition

From an edition point of view, a mapping is identical to an automation : it is just a curve.
However, since it is not temporal, growing the process with the Grow mode has no effect : it will always be rescaled.

## Inspector

The mapping inspector is very simple :

<img class="alignnone size-medium wp-image-1343" src="http://ossia.io/wp-content/uploads/2016/11/mapping_inspector-300x127.png" alt="mapping_inspector" width="300" height="127" />

The parameters are :

* Source address : its value will be fetched at each tick. Like elsewhere in the software, accessing a single value of an array is supported, as well as unit conversions
* Source min / max : the values in which the input is assumed to be
* Target address : the address that will be written to
* Target min / max : the output range

## Execution
The mapping behaves as follows at each tick :

* The current value of the source address is fetched
* It is mapped to the X axis of the curve according to the source min-max
* The corresponding point is taken on the Y axis
* This point is scaled according to the target min-max
* The resulting value is sent to the target address

For instance, the following curve with identical min-max for the source and target would just
copy its input to its output at each tick :

<img class="alignnone size-full wp-image-1344" src="http://ossia.io/wp-content/uploads/2016/11/simple-mapping.png" alt="simple-mapping" width="234" height="152" />

If the target max is set to twice the source max (for instance from (0, 1) to (0, 2)), all the input values will be multiplied by two.

The following curve will invert the input values.

<img class="alignnone size-full wp-image-1345" src="http://ossia.io/wp-content/uploads/2016/11/mapping-rev.png" alt="mapping-rev" width="232" height="150" />

More complex curves will of course have more complex effects.
