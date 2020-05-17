#pragma once
#include <ossia/audio/portaudio_protocol.hpp>

#include <QString>

namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO)
struct PortAudioScope
{
  PortAudioScope() { Pa_Initialize(); }
  ~PortAudioScope() { Pa_Terminate(); }
};

struct PortAudioCard
{
  QString api;
  QString raw_name;
  QString pretty_name;
  PaDeviceIndex dev_idx{};

  int inputChan{};
  int outputChan{};

  PaHostApiTypeId hostapi{};

  double rate{};

  int in_index{-1};
  int out_index{-1};
};

#endif
}
