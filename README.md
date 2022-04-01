<img src="https://i.imgur.com/BL6J8Jh.png" width="500">

[![Financial Contributors on Open Collective](https://opencollective.com/ossia/all/badge.svg?label=financial+contributors)](https://opencollective.com/ossia) [![join the chat at https://gitter.im/ossia/score](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/ossia/score?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge) [![Downloads](https://img.shields.io/badge/dynamic/json?color=success&label=downloads&query=%24.downloads&url=https%3A%2F%2Fossia.io%2Fdownload-stats.json)](https://github.com/ossia/score/releases) [![Version](https://img.shields.io/github/release/ossia/score.svg)](https://github.com/ossia/score/releases)

[**ossia score**](https://ossia.io) is a ***sequencer*** for audio-visual artists, designed to create ***interactive*** shows.

Sequence **OSC, MIDI, DMX, sound, video** and more, between multiple software and hardware, create ***interactive*** and ***intermedia*** scores and ***script*** with JavaScript, PureData or C++ to customize your score.

Free, open source and runs on desktop, mobile, web and embedded.

Read more on [https://ossia.io](https://ossia.io), come ask questions on the [forum](https://forum.ossia.io/c/score) or hang out in our [chat](https://gitter.im/ossia/score), we will be happy to help you !

### Quick download links: [latest official release](https://github.com/ossia/score/releases/) / [bleeding edge](https://github.com/ossia/score/releases/tag/continuous)

![ossia score screenshot](/docs/score.png?raw=true)

## Releases

Releases are available for [Windows, Linux (via AppImage) and Mac OS X](https://github.com/ossia/score/releases/latest).

* Windows: install and run.
* OS X: extract and run ossia score.app.
* Linux: make executable (right click -> permissions or `chmod +x`) and run the AppImage.

## Build status
* Official Win/Mac/Linux releases: [![Azure Pipelines](https://img.shields.io/azure-devops/build/ossia/f914424f-63a4-43f7-b424-67c9dc58ae05/2)](https://dev.azure.com/ossia/libossia/_build?definitionId=2)
* ArchLinux, OpenSUSE, Fedora, Debian, WASM, Raspberry Pi: [![Linux distro build](https://github.com/ossia/score/workflows/Linux%20distro%20build/badge.svg)](https://github.com/ossia/score/actions?query=workflow%3A%22Linux+distro+build%22)
* Ubuntu Bionic, Focal on GCC, Clang: [![Ubuntu build](https://github.com/ossia/score/workflows/Ubuntu%20build/badge.svg)](https://github.com/ossia/score/actions?query=workflow%3A%22Ubuntu+build%22)
* macOS (with Brew): [![macOS build](https://github.com/ossia/score/workflows/macOS%20build/badge.svg)](https://github.com/ossia/score/actions?query=workflow%3A%22macOS+build%22)
* [![OpenHub](https://www.openhub.net/p/score/widgets/project_thin_badge?format=gif)](https://www.openhub.net/p/score)
* ossia score uses [CppDepend](https://www.cppdepend.com/) to ensure consistent code quality improvements ; please check it out !

In order to build score, follow the [documentation](https://ossia.io/score-docs/development/build-from-source.html).

## Contributing

When updating the score repository through the command line, don't forget to update the submodules, they change often.

    cd score
    git pull
    git submodule update --init --recursive

Some useful and easy potential contributions are listed [on the website](https://ossia.io/contributor-guide.html).

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
