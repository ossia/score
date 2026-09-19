#pragma once

/* The GRIS speaker-setup data model, JUCE-free.
 *
 * Mirrors the subset of StructGRIS's SpeakerData / SpeakerSetup that matters
 * outside SpatGRIS's own application concerns: geometry, output patch, gain,
 * highpass and direct-out. Everything SpatGRIS keeps for its own UI (window
 * positions, recording options, colours) is deliberately absent -- score
 * provides those.
 */

#include <Gris/Algo/Types.hpp>

#include <string>
#include <vector>

namespace Gris
{
//==============================================================================
/** Which family of algorithm a setup is meant for.
 *  Matches gris::SpatMode: DOME -> VBAP, CUBE -> MBAP. */
enum class SpatMode : std::int8_t
{
  invalid = -1,
  vbap = 0, //! "Dome" in the SpatGRIS UI and file format
  mbap,     //! "Cube"
  hybrid    //! per-source choice between the two
};

[[nodiscard]] std::string_view toString(SpatMode) noexcept;
[[nodiscard]] SpatMode spatModeFromString(std::string_view) noexcept;

//==============================================================================
enum class SliceState : std::uint8_t
{
  normal = 0,
  muted,
  solo
};

[[nodiscard]] std::string_view toString(SliceState) noexcept;
[[nodiscard]] SliceState sliceStateFromString(std::string_view) noexcept;

//==============================================================================
struct SpeakerHighpass
{
  float freq{};
};

//==============================================================================
struct SpeakerData
{
  Position position{};
  SliceState state{SliceState::normal};
  /** Gain in dB, as stored in the setup files. */
  float gain{};
  /** 0 means no highpass. */
  float highpassFreq{};
  bool isDirectOutOnly{};
};

//==============================================================================
/** One speaker plus its physical output. Replaces the key/value pair that
 *  StructGRIS's OwnedMap<output_patch_t, SpeakerData> yields when iterated. */
struct SpeakerEntry
{
  output_patch_t patch{};
  SpeakerData data{};
};

/** Replaces gris::SpeakersData for the purposes of the gain computers. Kept
 *  ordered by insertion, which is the layout order in the setup file. */
using SpeakersData = std::vector<SpeakerEntry>;

//==============================================================================
/** A named group of speakers, with the rotation SpatGRIS 4 allows on it.
 *  Groups are flattened before the algorithms see them. */
struct SpeakerGroup
{
  std::string name{};
  CartesianVector position{};
  degrees_t yaw{};
  degrees_t pitch{};
  degrees_t roll{};
  std::vector<SpeakerEntry> speakers{};
};

//==============================================================================
struct SpeakerSetup
{
  SpatMode spatMode{SpatMode::vbap};
  float diffusion{};
  bool generalMute{};
  /** Groups as they appear in the file. A legacy setup yields one anonymous
   *  group holding every speaker. */
  std::vector<SpeakerGroup> groups{};

  /** Every speaker, group transforms applied, in layout order.
   *  This is what gets handed to vbapInit()/mbapInit(). */
  [[nodiscard]] SpeakersData flattened() const;

  /** Speakers that take part in spatialisation (i.e. not direct-out-only). */
  [[nodiscard]] int numSpatializedSpeakers() const noexcept;

  /** Highest output patch in the setup, i.e. how many audio channels the
   *  process has to produce. 0 when empty. */
  [[nodiscard]] int maxOutputPatch() const noexcept;

  /** true when every speaker sits at roughly the same radius: VBAP needs it,
   *  MBAP does not care. */
  [[nodiscard]] bool isDomeLike() const noexcept;
};

} // namespace Gris
