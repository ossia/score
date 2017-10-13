README
======

# Basic usage: 

## High-level API: 

### Setup
* Create an empty GameObject, named "OssiaController". 
* Add the script `Ossia/unity3d/Controller.cs` to it.
* Optionally, `Ossia/unity3d/PresetController.cs` can be added for preset and device loading / saving.
* Optionally, `Ossia/unity3d/Logger.cs` can be added for a logging object.

OssiaController should look like this: 

![OssiaController](https://github.com/OSSIA/OSSIA.github.io/blob/dev/source/images/unity/OssiaController.png)

Then once this is done, set the script execution order so that the objects are loaded in the correct order: 

![ScriptOrder](https://github.com/OSSIA/OSSIA.github.io/blob/dev/source/images/unity/ScriptOrder.png)

### Exposing objects 

To expose objects to the API, either : 

* Add the `Ossia/unity3d/ExposeAttributes.cs` to an object. 
  Then, in your own scripts, mark fields and properties with `[Ossia.Expose]` like in the example `ExampleAttribute.cs`: 

![ExposeAttributes](https://github.com/OSSIA/OSSIA.github.io/blob/dev/source/images/unity/ExposeCube.png)

* Or add the `Ossia/unity3d/ExposeComponents.cs` to an object.
  Then, add all the components that you wish to expose to the list, by dragging and dropping them.

![ExposeAttributes](https://github.com/OSSIA/OSSIA.github.io/blob/dev/source/images/unity/ExposeCylinder.png)

## Low-level C# API:
* An example is given in `Ossia/unity3d/examples/CustomDevice.cs`

# TODO

* Materials
* Instance support when loading presets
