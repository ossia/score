i-score
=======


i-score is an interactive intermedia sequencer - read more on http://i-score.org/

![i-score screenshot](/Documentation/iscore.png?raw=true)

Easiest way is to grab a release in Github releases.

In order to build i-score, follow the instructions in INSTALL.md.
Current builds tested on Mac OS X, Ubuntu 14.04, Debian GNU/Linux (Jessie, Sid), and ArchLinux.

An installer for the previous version is available on http://i-score.org/
The previous version source is located at : http://github.com/i-score/i-score

## Status

[![Join the chat at https://gitter.im/OSSIA/i-score](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/OSSIA/i-score?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![OpenHub](https://www.openhub.net/p/i-score/widgets/project_thin_badge.gif)](https://www.openhub.net/p/i-score)

### Build
* Linux, OS X : [![Travis Status](https://travis-ci.org/OSSIA/i-score.svg?branch=master)](https://travis-ci.org/OSSIA/i-score)
* Windows : [![Appveyor Status](https://ci.appveyor.com/api/projects/status/github/OSSIA/i-score?branch=master&svg=true)](https://ci.appveyor.com/project/JeanMichalCelerier/i-score)
[//]: # (* Coverity : [![Coverity Status](https://scan.coverity.com/projects/3356/badge.svg)](https://scan.coverity.com/projects/3356))
* Coveralls : [![Coverage Status](https://coveralls.io/repos/OSSIA/i-score/badge.svg?branch=&service=github)](https://coveralls.io/github/OSSIA/i-score?branch=)

### Quick install
* Arch Linux : (in the AUR4)
`yaourt -S jamomacore-git iscore-git`

* Ubuntu 14.04 :

Installation of some dependencies is required :

    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test

    sudo apt-get update -qq
    sudo apt-get dist-upgrade
    sudo apt-get install -qq g++-5 libxml2 libportmidi0 libportaudio2 libsndfile1 

    wget https://www.dropbox.com/s/0pmy14zlpqpyaq6/JamomaCore-0.6-dev-Linux.deb?dl=1 -O jamoma.deb
    sudo dpkg -i jamoma.deb

Then [fetch the release here](https://github.com/OSSIA/i-score/releases/latest).

* Windows, OS X : [Latest release](https://github.com/OSSIA/i-score/releases/latest)
