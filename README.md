<img src="https://i.imgur.com/BL6J8Jh.png" width="500">

[![Financial Contributors on Open Collective](https://opencollective.com/ossia/all/badge.svg?label=financial+contributors)](https://opencollective.com/ossia) [![Discord](https://img.shields.io/discord/928307671579394179.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/8Hzm4UduaS) [![join the chat at https://gitter.im/ossia/score](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/ossia/score?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge) [![Downloads](https://img.shields.io/badge/dynamic/json?color=success&label=downloads&query=%24.downloads&url=https%3A%2F%2Fossia.io%2Fdownload-stats.json)](https://github.com/ossia/score/releases) [![Version](https://img.shields.io/github/release/ossia/score.svg)](https://github.com/ossia/score/releases) [![OpenHub](https://www.openhub.net/p/score/widgets/project_thin_badge?format=gif)](https://www.openhub.net/p/score)

[**ossia score**](https://ossia.io) is a ***sequencer*** for audio-visual artists, designed to create ***interactive*** shows.

Sequence **OSC, MIDI, DMX, sound, video** and more, between multiple software and hardware. Create ***interactive*** and ***intermedia*** scores, ***script*** and ***live-code*** with JavaScript, [ISF Shaders](https://isf.video), [Faust](https://faust.grame.fr), PureData or C++. Leverage IoT protocols such as CoAP or MQTT, interact with joysticks, Wiimotes, Leapmotions, Web APIs and BLE sensors and integrate programs from a wealth of creative programming languages such as [Structure Synth](https://structuresynth.sourceforge.net/), [Context-Free Art](https://www.contextfreeart.org/) and [Bytebeat](https://dollchan.net/bytebeat/). Load any kind of audio or video format and process visuals through Spout, Syphon, NDI, Shmdata or Sh4lt ; sonify large datasets with CSV and HDF5 support.

Free, open source and runs on desktop, mobile, web and embedded, down to Raspberry Pi Zero 2.

Read more on [https://ossia.io](https://ossia.io), come ask questions on the [forum](https://forum.ossia.io/c/score) or hang out in our [Discord](https://discord.gg/8Hzm4UduaS) or [Matrix room #ossia_score:gitter.im](https://gitter.im/ossia/score), we will be happy to help you !

### Quick download links: [latest official release](https://github.com/ossia/score/releases) / [bleeding edge](https://github.com/ossia/score/releases/tag/continuous)

![ossia score screenshot](/docs/score.png?raw=true)

## Running ossia score

Releases are available for [Windows, Linux (via AppImage) and macOS](https://github.com/ossia/score/releases/latest).

* Windows: install and run.
* macOS: open the `.dmg` and drop `ossia score.app` in Applications.
* Linux: make executable (right click -> permissions or `chmod +x`) and run the AppImage.

## Build status

| Platform                          | Status                                                                                                                                                                           |
|-----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| AppImage (x86-64 & AArch64)       | [![AppImage](https://github.com/ossia/score/actions/workflows/appimage.yml/badge.svg)](https://github.com/ossia/score/actions/workflows/appimage.yml)                            |
| Flatpak (x86-64 & AArch64)        | [![Flatpak](https://github.com/ossia/score/actions/workflows/flatpak.yml/badge.svg)](https://github.com/ossia/score/actions/workflows/flatpak.yml)                               |
| Nix                               | [![Nix](https://github.com/ossia/score/actions/workflows/nix-builds.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/nix-builds.yaml)                           |
| ArchLinux, SUSE, Fedora           | [![Linux distros](https://github.com/ossia/score/actions/workflows/builds.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/builds.yaml)                         |
| Debian (Bookworm, Trixie)         | [![Debian](https://github.com/ossia/score/actions/workflows/debian-builds.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/debian-builds.yaml)                  |
| Ubuntu (22.04, 24.04, 24.10)      | [![Ubuntu](https://github.com/ossia/score/actions/workflows/ubuntu-builds.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/ubuntu-builds.yaml)                  |
| macOS (M1 & Intel)                | [![macOS](https://github.com/ossia/score/actions/workflows/mac-builds.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/mac-builds.yaml)                         |
| Windows (MSYS2, MSVC 2022)        | [![Windows](https://github.com/ossia/score/actions/workflows/win-builds.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/win-builds.yaml)                       |
| FreeBSD                           | [![BSD](https://github.com/ossia/score/actions/workflows/bsd.yml/badge.svg)](https://github.com/ossia/score/actions/workflows/bsd.yml)                                           |
| WebAssembly                       | [![WASM](https://github.com/ossia/score/actions/workflows/wasm.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/wasm.yaml)                                      |
| Plug-in templates                 | [![Template check](https://github.com/ossia/score/actions/workflows/templates.yaml/badge.svg)](https://github.com/ossia/score/actions/workflows/templates.yaml)                  |
 
ossia score uses [CppDepend](https://www.cppdepend.com/) to ensure consistent code quality improvements ; please check it out !

In order to build score, follow the [documentation](https://ossia.io/score-docs/development/build-from-source.html).

## Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/ossia-score.svg?columns=3&header=ossia-score)](https://repology.org/project/ossia-score/versions)

## Contributing

When updating the score repository through the command line, don't forget to update the submodules, they change often.

    cd score
    git pull
    git submodule update --init --recursive

Some useful and easy potential contributions are listed [on the website](https://ossia.io/project.html).

There are also many fixable areas in the code :
* [**TODO**](https://github.com/ossia/score/search?q=TODO) : for simple issues.
* [**FIXME**](https://github.com/ossia/score/search?q=FIXME) : for critical bugs / problems.
* [**MOVEME**](https://github.com/ossia/score/search?q=MOVEME) : when something is not where it should be.
* [**REMOVEME**](https://github.com/ossia/score/search?q=REMOVEME) : when something is redundant.
* [**RENAMEME**](https://github.com/ossia/score/search?q=RENAMEME) : when the class does not match with the file it's in.
* [**OPTIMIZEME**](https://github.com/ossia/score/search?q=OPTIMIZEME) : when an easy-to-write-but-suboptimal algorithm is used.

Finally, one can write its own score plug-ins to add custom features to the software.
An example plug-in with the most common classes reimplemented is provided [here](https://github.com/ossia/score-addon-tutorial).

## Contributors

### Code Contributors

This project exists thanks to all the people who contribute. [[Contribute](CONTRIBUTING.md)].
<a href="https://github.com/ossia/score/graphs/contributors"><img src="https://opencollective.com/ossia/contributors.svg?width=890&button=false" /></a>

### Financial Contributors

Become a financial contributor and help us sustain our community. [[Contribute](https://opencollective.com/ossia/contribute)]

#### Individuals

<a href="https://opencollective.com/ossia"><img src="https://opencollective.com/ossia/individuals.svg?width=890"></a>

#### Organizations

Support this project with your organization. Your logo will show up here with a link to your website. [[Contribute](https://opencollective.com/ossia/contribute)]

<a href="https://opencollective.com/ossia/organization/0/website"><img src="https://opencollective.com/ossia/organization/0/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/1/website"><img src="https://opencollective.com/ossia/organization/1/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/2/website"><img src="https://opencollective.com/ossia/organization/2/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/3/website"><img src="https://opencollective.com/ossia/organization/3/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/4/website"><img src="https://opencollective.com/ossia/organization/4/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/5/website"><img src="https://opencollective.com/ossia/organization/5/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/6/website"><img src="https://opencollective.com/ossia/organization/6/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/7/website"><img src="https://opencollective.com/ossia/organization/7/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/8/website"><img src="https://opencollective.com/ossia/organization/8/avatar.svg"></a>
<a href="https://opencollective.com/ossia/organization/9/website"><img src="https://opencollective.com/ossia/organization/9/avatar.svg"></a>
