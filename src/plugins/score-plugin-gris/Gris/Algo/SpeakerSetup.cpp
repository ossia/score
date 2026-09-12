#include <Gris/Algo/Quaternion.hpp>
#include <Gris/Algo/SpeakerSetup.hpp>

#include <cmath>

namespace Gris
{
std::string_view toString(SpatMode mode) noexcept
{
  switch(mode)
  {
    case SpatMode::vbap:
      return "Dome";
    case SpatMode::mbap:
      return "Cube";
    case SpatMode::hybrid:
      return "Hybrid";
    default:
      return "invalid";
  }
}

SpatMode spatModeFromString(std::string_view str) noexcept
{
  // Both the label used by the SpatGRIS UI/file format and the algorithm name
  // are accepted, since setups in the wild carry either.
  if(str == "Dome" || str == "VBAP" || str == "vbap")
    return SpatMode::vbap;
  if(str == "Cube" || str == "MBAP" || str == "mbap" || str == "LBAP")
    return SpatMode::mbap;
  if(str == "Hybrid" || str == "hybrid")
    return SpatMode::hybrid;
  return SpatMode::invalid;
}

std::string_view toString(SliceState state) noexcept
{
  switch(state)
  {
    case SliceState::muted:
      return "muted";
    case SliceState::solo:
      return "solo";
    case SliceState::normal:
    default:
      return "normal";
  }
}

SliceState sliceStateFromString(std::string_view str) noexcept
{
  if(str == "muted")
    return SliceState::muted;
  if(str == "solo")
    return SliceState::solo;
  return SliceState::normal;
}

//==============================================================================
SpeakersData SpeakerSetup::flattened() const
{
  SpeakersData result;

  for(auto const& group : groups)
  {
    // A group with no rotation leaves its speakers' stored positions alone;
    // this mirrors StructGRIS's SpeakerData::getAbsoluteSpeakerPosition().
    bool const rotated = !(
        std::fpclassify(group.yaw.get()) == FP_ZERO
        && std::fpclassify(group.pitch.get()) == FP_ZERO
        && std::fpclassify(group.roll.get()) == FP_ZERO);

    Quaternion quat{};
    if(rotated)
      quat = getQuaternionFromEulerAngles(
          group.yaw.get(), group.pitch.get(), group.roll.get());

    for(auto const& speaker : group.speakers)
    {
      auto const local = speaker.data.position.getCartesian();

      CartesianVector placed{local};
      if(rotated)
      {
        auto const r = quatRotation({local.x, local.y, local.z}, quat);
        placed = CartesianVector{r[0], r[1], r[2]};
      }

      SpeakerEntry entry{speaker};
      entry.data.position = Position{CartesianVector{
          group.position.x + placed.x, group.position.y + placed.y,
          group.position.z + placed.z}};
      result.push_back(entry);
    }
  }

  return result;
}

int SpeakerSetup::numSpatializedSpeakers() const noexcept
{
  int count{};
  for(auto const& group : groups)
    for(auto const& speaker : group.speakers)
      if(!speaker.data.isDirectOutOnly)
        ++count;
  return count;
}

int SpeakerSetup::maxOutputPatch() const noexcept
{
  int max{};
  for(auto const& group : groups)
    for(auto const& speaker : group.speakers)
      max = std::max(max, speaker.patch.get());
  return max;
}

bool SpeakerSetup::isDomeLike() const noexcept
{
  // Same test as StructGRIS: all spatialized speakers within 1% of the same
  // radius means the setup describes a dome.
  constexpr float tolerance = 0.01f;

  bool first = true;
  float reference{};
  for(auto const& group : groups)
  {
    for(auto const& speaker : group.speakers)
    {
      if(speaker.data.isDirectOutOnly)
        continue;
      auto const radius = speaker.data.position.getPolar().length;
      if(first)
      {
        reference = radius;
        first = false;
        continue;
      }
      if(std::abs(radius - reference) > tolerance)
        return false;
    }
  }
  return !first;
}

} // namespace Gris
