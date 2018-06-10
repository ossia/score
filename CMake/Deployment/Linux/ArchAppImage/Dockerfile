FROM archlinux/base:latest

RUN pacman -Syyu --noconfirm
RUN pacman -S intel-tbb lilv ffmpeg portaudio jack2 libx264 boost make gcc libglvnd qt5-svg qt5-base qt5-imageformats qt5-declarative qt5-quickcontrols2 qt5-serialport qt5-websockets qt5-tools cmake git ninja --noconfirm
RUN git clone -j16 --recursive --depth=1 https://github.com/OSSIA/score -b master
