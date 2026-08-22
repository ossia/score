#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <verdigris>

#include <cstdint>

namespace ClipLauncher
{

enum class ExclusivityMode : uint8_t
{
  Exclusive,  // Starting a clip stops the current one in this lane
  Polyphonic, // Multiple clips can run simultaneously
  Crossfade   // Starting a clip crossfades from current to new
};

enum class LaunchMode : uint8_t
{
  Immediate,        // Start right now
  QuantizedBeat,    // Start on next beat boundary
  QuantizedBar,     // Start on next bar boundary
  QuantizedEndClip, // Start when current clip ends
  Queued,           // Queue after current clip
  FaderStart        // Start follows fader value
};

enum class TriggerStyle : uint8_t
{
  Trigger,   // Press to start, press again to stop
  Toggle,    // Press to start, press to stop
  Gate,      // Held = playing, release = stop
  Retrigger, // Press always restarts from beginning
  Legato     // Press restarts but continues time position
};

enum class TemporalMode : uint8_t
{
  FreeRunning,    // Runs at its own pace
  BPMSynced,      // Follows global tempo
  TimecodeLocked, // Locked to timecode
  Interactive     // Manually driven
};

enum class CellState : uint8_t
{
  Empty,
  Stopped,
  Queued,
  Playing,
  Stopping
};

// Values match the Video Mixer ISF shader's mode integers
enum class VideoBlendMode : uint8_t
{
  Add = 1,
  Average = 2,
  ColorBurn = 3,
  ColorDodge = 4,
  Darken = 5,
  Difference = 6,
  Exclusion = 7,
  Glow = 8,
  HardLight = 9,
  HardMix = 10,
  Lighten = 11,
  LinearBurn = 12,
  LinearDodge = 13,
  LinearLight = 14,
  Multiply = 15,
  Negation = 16,
  Normal = 17,
  Overlay = 18,
  Phoenix = 19,
  PinLight = 20,
  Reflect = 21,
  Screen = 22,
  SoftLight = 23,
  Subtract = 24,
  VividLight = 25
};

} // namespace ClipLauncher

// Serialization
inline QDataStream& operator<<(QDataStream& s, ClipLauncher::ExclusivityMode v)
{
  return s << static_cast<uint8_t>(v);
}
inline QDataStream& operator>>(QDataStream& s, ClipLauncher::ExclusivityMode& v)
{
  uint8_t x;
  s >> x;
  v = static_cast<ClipLauncher::ExclusivityMode>(x);
  return s;
}

inline QDataStream& operator<<(QDataStream& s, ClipLauncher::LaunchMode v)
{
  return s << static_cast<uint8_t>(v);
}
inline QDataStream& operator>>(QDataStream& s, ClipLauncher::LaunchMode& v)
{
  uint8_t x;
  s >> x;
  v = static_cast<ClipLauncher::LaunchMode>(x);
  return s;
}

inline QDataStream& operator<<(QDataStream& s, ClipLauncher::TriggerStyle v)
{
  return s << static_cast<uint8_t>(v);
}
inline QDataStream& operator>>(QDataStream& s, ClipLauncher::TriggerStyle& v)
{
  uint8_t x;
  s >> x;
  v = static_cast<ClipLauncher::TriggerStyle>(x);
  return s;
}

inline QDataStream& operator<<(QDataStream& s, ClipLauncher::TemporalMode v)
{
  return s << static_cast<uint8_t>(v);
}
inline QDataStream& operator>>(QDataStream& s, ClipLauncher::TemporalMode& v)
{
  uint8_t x;
  s >> x;
  v = static_cast<ClipLauncher::TemporalMode>(x);
  return s;
}

inline QDataStream& operator<<(QDataStream& s, ClipLauncher::VideoBlendMode v)
{
  return s << static_cast<uint8_t>(v);
}
inline QDataStream& operator>>(QDataStream& s, ClipLauncher::VideoBlendMode& v)
{
  uint8_t x;
  s >> x;
  v = static_cast<ClipLauncher::VideoBlendMode>(x);
  return s;
}

W_REGISTER_ARGTYPE(ClipLauncher::ExclusivityMode)
W_REGISTER_ARGTYPE(ClipLauncher::LaunchMode)
W_REGISTER_ARGTYPE(ClipLauncher::TriggerStyle)
W_REGISTER_ARGTYPE(ClipLauncher::TemporalMode)
W_REGISTER_ARGTYPE(ClipLauncher::CellState)
W_REGISTER_ARGTYPE(ClipLauncher::VideoBlendMode)
