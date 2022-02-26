# ossia score plug-ins

This add-on provides the groundwork for a stable plug-in API for [ossia score](https://ossia.io).

Unlike most plug-in APIs for multimedia software and APIs such as LV2, VST, PureData or Max objects, ...
it has the particularity of not requiring inheritance, filling of pointer functions, etc.
In particular, it is possible to write a score plug-in without using any library / header at all,
just by defining simple C++ structures. Reflection features of modern C++ are then used to construct
the run-time structures from it.

The following blog post: https://ossia.io/posts/reflection/ explains the rationale and the implementation;
an example is provided below.

There are multiple examples, which can act as a tutorial of sorts ;
the comments in e.g. example 5 expects that you have read examples 1 to 4:

## Simple examples

These examples showcase the basic usage of the API for the most common cases of processors found in
multimedia systems.

- [1 - Hello World](Examples/Empty.hpp)
- [2 - Trivial value generator](Examples/TrivialGenerator.hpp)
- [3 - Trivial value filter](Examples/TrivialFilter.hpp)
- [4 - Simple audio effect](Examples/AudioEffect.hpp)
- [5 - Audio effect with side-chains](Examples/AudioEffectWithSidechains.hpp)
- [6 - MIDI Synthesizer](Examples/Synth.hpp)
- [7 - Texture Generator](Examples/TextureGenerator.hpp)
- [8 - Texture Filter](Examples/TextureFilter.hpp)

## Advanced features

These examples showcase sore more ossia-specific features:

* Sample-accurate control values:
- [8 - Sample-accurate generator](Examples/SampleAccurateGenerator.hpp)
- [9 - Sample-accurate filter](Examples/SampleAccurateFilter.hpp)
- [10 - Porting an effect from Max/MSP: CCC](Examples/CCC.hpp)

* Dynamic multichannel handling:
- [12 - Multichannel distortion](Examples/Distortion.hpp)

* All the GUI controls that can currently be used
- [13 - Control Gallery](Examples/ControlGallery.hpp)

## Going deeper

* How to write a score plug-in without having any dependency on score:
- [14 - Zero-dependency audio effect](Examples/ZeroDependencyAudioEffect.hpp)

* How to, instead, use score's data structures directly (which gives slightly better performance at the expense of interoperability and ease-of-use):
- [15 - Raw ports](Examples/RawPorts.hpp)
