---
title: Advanced value addressing
---

# Advanced value addressing

*Score* provides various features to ease the managment of parameters. These are specially useful when writing complex processes (e.g automations).

## Addressing items in arrays

When a parameter of a declared device defines a set of values (e.g parameters defining an xyz position or an rgb color), items in this array can accessed independantly using a special syntaxe: a parameter address may be followed by an integer (starting from zero) definiting position in the array put in brackets.

For example, using `aDevice:/anAddress[1]` as a destination address of an automation will send the automation value to the *second* element in the array.

> Note that without specifying an index, automation sent to array parameter (ie. `vec2f`, `vec3f`, list) will affect all items in the array

## Using unit conversion

Parameter of a declared device may also be specified a particular unit (e.g parameters defining a position in space or a color). *Score* embeds some unit conversion for advanced automations. Unit to execute an automation on can be spet using a similar bracket-based syntaxe.

For example, using `aDevice:/anAddress[angle.radian]` as a destination address of an automation will send the value in this unit (ie. radian). The value will be converted back to the address's original unit upon sending.

## Combining item addressing and unit

 	aDevice:/anAddress[color.rgb.r] : same as before, but for a specific component of an unit.


<!-- TODO
	
	Execution semantics
	The automation tries to send values that are graphically as close as possible as the shown curve.
	For the case where the address has no unit :

	 	If the address is integer or floating point, the behavior is as expected.
	 	If the address is a fixed-width array, then :

	 	If there is no array accessor (such as an:/address[1]), all the values of the array are set to the value of the automation ([1, 1], [2, 2], [3, 3], etc.).
	 	If there is an array accessor, only the accessed value will be set : [1, 1], [1, 2], [1, 3], etc...



	For the case where the address has an unit, the value will be converted to the correct unit afterwards.

	An interesting case is the cascading of units.
	For instance, given this case :



	First, the current value of aDevice:/light will be fetched, converted to the hue-saturation-value color space and its value will be set according to the first curve.
	Then, in the red-green-blue color-space, the red component will be increased.
	The result will be applied.
	-->
	
<!-- TODO
	## Pattern matching
	-->