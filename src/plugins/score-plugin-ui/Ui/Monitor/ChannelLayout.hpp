#pragma once
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string_view>

/**
 * Speaker layouts: the name and position of each channel, for labels, the
 * surround views, and loudness channel weighting (ITU-R BS.1770).
 */
namespace Ui::Monitor
{
enum class ChannelLayout
{
  Auto,
  Numbered,
  Stereo,
  Surround51,
  Surround71,
  Surround714,
  Ambisonic
};

inline constexpr std::string_view channel_layout_names[]{
    "Auto", "Numbered", "Stereo", "5.1", "7.1", "7.1.4", "Ambisonic"};

struct Speaker
{
  std::string_view name;
  //! Degrees, 0 in front, positive to the right
  double azimuth{};
  //! Degrees above the listener
  double elevation{};
  bool lfe{};
};

namespace detail
{
inline constexpr Speaker stereo[]{{"L", -30}, {"R", 30}};
inline constexpr Speaker s51[]{
    {"L", -30}, {"R", 30}, {"C", 0}, {"LFE", 0, 0, true}, {"Ls", -110}, {"Rs", 110}};
inline constexpr Speaker s71[]{
    {"L", -30},   {"R", 30},   {"C", 0},     {"LFE", 0, 0, true},
    {"Lss", -90}, {"Rss", 90}, {"Lrs", -150}, {"Rrs", 150}};
inline constexpr Speaker s714[]{
    {"L", -30},       {"R", 30},       {"C", 0},         {"LFE", 0, 0, true},
    {"Lss", -90},     {"Rss", 90},     {"Lrs", -150},    {"Rrs", 150},
    {"Ltf", -45, 45}, {"Rtf", 45, 45}, {"Ltr", -135, 45}, {"Rtr", 135, 45}};
}

//! The layout `l` resolves to for `channels` channels: Auto picks the
//! standard layout with exactly that many channels.
inline ChannelLayout resolve(ChannelLayout l, int channels) noexcept
{
  if(l != ChannelLayout::Auto)
    return l;
  switch(channels)
  {
    case 2:
      return ChannelLayout::Stereo;
    case 6:
      return ChannelLayout::Surround51;
    case 8:
      return ChannelLayout::Surround71;
    case 12:
      return ChannelLayout::Surround714;
    default:
      return ChannelLayout::Numbered;
  }
}

//! The speakers of a layout, empty when it has no fixed positions.
inline std::span<const Speaker> speakers(ChannelLayout l, int channels) noexcept
{
  switch(resolve(l, channels))
  {
    case ChannelLayout::Stereo:
      return detail::stereo;
    case ChannelLayout::Surround51:
      return detail::s51;
    case ChannelLayout::Surround71:
      return detail::s71;
    case ChannelLayout::Surround714:
      return detail::s714;
    default:
      return {};
  }
}

inline QString channelLabel(ChannelLayout l, int channels, int c)
{
  const auto sp = speakers(l, channels);
  if(c < int(sp.size()))
    return QString::fromLatin1(sp[c].name.data(), sp[c].name.size());
  if(resolve(l, channels) == ChannelLayout::Ambisonic)
  {
    // ACN order: degree n holds channels n^2 .. (n+1)^2 - 1
    const int n = int(std::sqrt(double(c)));
    return QStringLiteral("%1.%2").arg(n).arg(c - n * n - n);
  }
  return QString::number(c + 1);
}

/**
 * BS.1770 channel weight: 0 for LFE, 1.41 (+1.5 dB) for channels below 30
 * degrees of elevation and between 60 and 120 degrees of azimuth, 1
 * otherwise. Ambisonic channels are not speakers: only W, the omnidirectional
 * one, is measured.
 */
inline double loudnessWeight(ChannelLayout l, int channels, int c) noexcept
{
  if(resolve(l, channels) == ChannelLayout::Ambisonic)
    return c == 0 ? 1. : 0.;
  const auto sp = speakers(l, channels);
  if(c >= int(sp.size()))
    return 1.;
  if(sp[c].lfe)
    return 0.;
  const double a = std::abs(sp[c].azimuth);
  return (a >= 60. && a <= 120. && std::abs(sp[c].elevation) < 30.) ? 1.41 : 1.;
}

/**
 * The channels of a layout around the listener, in increasing azimuth,
 * without LFE and height channels: the ring the surround polygon draws.
 * Numbered layouts are taken as evenly spaced, in order.
 */
struct Ring
{
  std::array<int, 64> channel{};
  std::array<double, 64> azimuth{};
  int size{};
};

inline Ring ring(ChannelLayout l, int channels) noexcept
{
  Ring r;
  const auto sp = speakers(l, channels);
  if(!sp.empty())
  {
    for(int c = 0; c < int(sp.size()) && c < channels; c++)
    {
      if(sp[c].lfe || sp[c].elevation > 0)
        continue;
      r.channel[r.size] = c;
      r.azimuth[r.size] = sp[c].azimuth;
      r.size++;
    }
    // Insertion sort by azimuth: at most a dozen entries
    for(int i = 1; i < r.size; i++)
      for(int j = i; j > 0 && r.azimuth[j - 1] > r.azimuth[j]; j--)
      {
        std::swap(r.azimuth[j - 1], r.azimuth[j]);
        std::swap(r.channel[j - 1], r.channel[j]);
      }
    return r;
  }
  r.size = std::min(channels, 64);
  for(int c = 0; c < r.size; c++)
  {
    r.channel[c] = c;
    r.azimuth[c] = -180. + 360. * (c + 0.5) / r.size;
  }
  return r;
}
}
