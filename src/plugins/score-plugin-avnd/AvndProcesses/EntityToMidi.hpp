#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "PointTracker.hpp"

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/math/filters.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <halp/audio.hpp>
#include <halp/controls.enums.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace avnd_tools
{

// Entity to MIDI - turns the identified, tracked entities produced by
// Point Tracker 2D / 3D into musically usable MIDI.
//
// The central design decision: THE NOTE IS THE ENTITY, NOT AN EVENT.
// One note-on when a track is confirmed, one note-off when it expires;
// everything in between is per-note expression (MPE pitch bend, channel
// pressure, CC74). Note spam is structurally impossible and stuck notes
// reduce to a single invariant: every held voice has a live entity behind it.
//
// Three layers, each owning its failure modes:
//   Voice    - allocation, stealing, stuck notes
//   Mapping  - ranges, curves, smoothing, scale quantisation
//   Emission - rate, byte budget, ordering, panic
//
// The mapping layer never generates note events; only lifecycle transitions
// (and the explicit Triggered mode) do.

enum class E2MOutputMode
{
  MPE,
  ChannelPerEntity,
  SingleChannel
};
enum class E2MZone
{
  Lower,
  Upper
};
enum class E2MChannelReuse
{
  LRU,
  Immediate
};
enum class E2MNoteModel
{
  Sustained,
  Triggered
};
enum class E2MTriggerOn
{
  Confirmed,
  FirstDetection
};
enum class E2MPriority
{
  ConfidenceAge,
  StealOldest,
  StealNewest,
  StealSlowest,
  StealLeastConfident
};
enum class E2MCoast
{
  Freeze,
  Follow,
  Fade
};
enum class E2MAxis
{
  X,
  Y,
  Z
};
//! How to read bare vectors on the Entities inlet. A vec3 is genuinely
//! ambiguous - the 2D chain emits (x, y, confidence), the 3D chain (x, y, z) -
//! and Point Tracker resolves it with its N template parameter, which this
//! process does not have. It is declared, not guessed: the two readings differ
//! only in data that also looks plausible under the other one, so any
//! heuristic would silently pick wrong on some perfectly ordinary input.
enum class E2MCoords
{
  TwoD,
  ThreeD
};
enum class E2MPitchTracking
{
  ContinuousBend,
  Latched,
  Retrigger
};
enum class E2MScale
{
  None,
  Chromatic,
  Major,
  NaturalMinor,
  HarmonicMinor,
  MajorPentatonic,
  MinorPentatonic,
  Blues,
  Dorian,
  Phrygian,
  Lydian,
  Mixolydian,
  WholeTone
};
enum class E2MRoot
{
  C,
  Cs,
  D,
  Ds,
  E,
  F,
  Fs,
  G,
  Gs,
  A,
  As,
  B
};
enum class E2MVelSource
{
  EntrySpeed,
  Fixed,
  Confidence
};
//! Everything a mapping can be driven from. One enum for every "source"
//! selector: pressure and timbre used to have their own, with the same members
//! in a different order, which made a preset's stored value mean two different
//! things depending on which slot it landed in.
//!
//! Ordering is stable and additive: new descriptors go at the end, so a stored
//! preset value keeps its meaning.
enum class E2MSource
{
  None,
  // --- position ---
  X,
  Y,
  Z,
  Radius,           //!< distance from the centre of the Position Min/Max window
  // --- motion ---
  Speed,            //!< |velocity|
  VelocityX,        //!< signed, bipolar around the centre of the range
  VelocityY,
  VelocityZ,
  SpeedX,           //!< |velocity| per axis
  SpeedY,
  SpeedZ,
  Acceleration,     //!< |dv/dt|
  AccelX,           //!< signed
  AccelY,
  AccelZ,
  Jerk,             //!< |da/dt|
  // --- path shape ---
  TurnRate,         //!< |dheading/dt|, rad/s
  Curvature,        //!< turn rate per unit distance travelled
  HeadingSin,       //!< direction of travel, as a pair, because the angle wraps
  HeadingCos,
  Agitation,        //!< how erratic the motion is, independently of its speed
  // --- relations between entities ---
  NearestNeighbour, //!< distance to the closest other entity
  CentroidDistance, //!< distance to the centre of mass of all entities
  Density,          //!< how many others are within Neighbour Range
  // --- lifecycle ---
  Age,
  Confidence
};

//! Per-entity analysis for one frame. Every field is already normalised to
//! 0..1 against the relevant reference control, so a mapping never has to know
//! the units it came from.
struct entity_descriptors
{
  float x{}, y{}, z{}, radius{};
  float speed{};
  float vx{}, vy{}, vz{};    // signed, 0.5 = still
  float sx{}, sy{}, sz{};    // magnitudes
  float accel{};
  float ax{}, ay{}, az{};    // signed, 0.5 = no acceleration
  float jerk{};
  float turn_rate{}, curvature{};
  float heading_sin{0.5f}, heading_cos{1.f};
  float agitation{};
  float nearest{1.f}, centroid_dist{}, density{};
  float age{}, confidence{};
};
enum class E2MQuantTargets
{
  Off,
  Onsets,
  OnsetsAndOffsets
};
enum class E2MGrid
{
  Whole,
  Half,
  Quarter,
  Eighth,
  Sixteenth,
  EighthTriplet,
  SixteenthTriplet
};

struct EntityToMidi
{
  halp_meta(name, "Entity To MIDI")
  halp_meta(c_name, "avnd_entity_to_midi")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia team")
  halp_meta(
      description,
      "Turn tracked entities (people, blobs, hands) into MIDI: each entity "
      "becomes one held MPE note whose pitch bend, pressure and timbre follow "
      "its motion for its whole lifetime. No note spam, no stuck notes.")
  halp_meta(uuid, "e977241d-439f-4316-9de8-e316a1651b2e")
  halp_flag(process_exec);

  // The exact record emitted by Point Tracker 2D / 3D. Shared on purpose so a
  // cable needs no adapter: the 3D variant is used as the common shape; a 2D
  // tracker's vec2f positions decode into it with z = 0.
  using track_record = PointTrackerBase<3>::track_record;

  struct ins
  {
    struct : halp::val_port<"Entities", std::vector<ossia::value>>
    {
      halp_meta(
          description,
          "One element per entity, in the same formats Point Tracker accepts on "
          "its Points inlet: full track records (as emitted by Point Tracker's "
          "Tracks output), {position, id, confidence, ...} maps, vec2f / vec3f / "
          "vec4f, sub-lists [x, y(, z)(, confidence)], or one flat list of "
          "numbers. Records carrying an id keep it; bare positions are "
          "identified by their index in the list, so such a source must emit "
          "entities in a stable order - otherwise cable Point Tracker in "
          "between, which is what assigns persistent ids.")
      void update(auto& self) { self.tracks_dirty = true; }
    } tracks;

    struct : halp::combobox_t<"Coordinates", E2MCoords>
    {
      halp_meta(
          description,
          "How to read bare vectors and flat number lists. 2D: (x, y), a third "
          "component is confidence, flat lists stride by 2. 3D: (x, y, z), a "
          "fourth component is confidence, flat lists stride by 3. Full records "
          "are unaffected - they name their fields.")
      struct range
      {
        std::string_view values[2]{"2D", "3D"};
        E2MCoords init{E2MCoords::TwoD};
      };
    } coords;

    // ------------------------------------------------------------- Output
    struct : halp::combobox_t<"Output Mode", E2MOutputMode>
    {
      halp_meta(
          description,
          "MPE gives every entity its own channel with per-note pitch bend, "
          "pressure and CC74 - use it with any MPE synth. Channel per entity "
          "is the same channel pinning without the MPE handshake, for "
          "hardware or DAW routing. Single channel puts every note on one "
          "channel: per-note bend is impossible there, so pitch is latched "
          "and pressure goes out as polyphonic aftertouch.")
      struct range
      {
        std::string_view values[3]{"MPE", "Channel per entity", "Single channel"};
        E2MOutputMode init{E2MOutputMode::MPE};
      };
    } output_mode;

    struct : halp::combobox_t<"MPE Zone", E2MZone>
    {
      halp_meta(
          description,
          "Lower zone: channel 1 is the master, notes use channels 2 and up. "
          "Upper zone: channel 16 is the master, notes go down from 15. Use "
          "Upper only to share the port with another Lower-zone device.")
      struct range
      {
        std::string_view values[2]{"Lower", "Upper"};
        E2MZone init{E2MZone::Lower};
      };
    } mpe_zone;

    struct : halp::spinbox_i32<"Member Channels", halp::irange{1, 15, 15}>
    {
      halp_meta(
          description,
          "How many note channels the MPE zone uses (sent in the MPE "
          "configuration). Also the channel count in Channel-per-entity mode. "
          "Lower it to leave channels free for other gear on the same port.")
    } member_channels;

    struct : halp::spinbox_i32<"Bend Range", halp::irange{1, 96, 48}>
    {
      halp_meta(
          description,
          "Per-note pitch bend range in semitones, sent to the receiver at "
          "start. 48 is the MPE default and gives eight octaves of glide, so "
          "a moving entity never needs a retrigger. Must match the receiver "
          "in non-MPE modes - set its bend range to the same value.")
    } bend_range;

    struct : halp::toggle<"Send MPE Config", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "On start, send the MPE configuration (RPN 6) and per-channel bend "
          "range (RPN 0) so the receiver sets itself up. Turn off if the "
          "synth is already configured or mangles RPNs.")
    } send_config;

    struct : halp::spinbox_i32<"Channel", halp::irange{1, 16, 1}>
    {
      halp_meta(
          description,
          "The MIDI channel used in Single-channel mode. Ignored in the "
          "other modes.")
    } single_channel;

    struct : halp::combobox_t<"Channel Reuse", E2MChannelReuse>
    {
      halp_meta(
          description,
          "What happens when a note ends and its channel could serve a new "
          "entity. Least recently used waits for release tails to fade "
          "before a channel is reused, so a new note's bend reset does not "
          "detune a still-ringing release. Immediate reuses right away.")
      struct range
      {
        std::string_view values[2]{"Least recently used", "Immediate"};
        E2MChannelReuse init{E2MChannelReuse::LRU};
      };
    } channel_reuse;

    struct : halp::spinbox_f32<"Release Reserve", halp::range{0., 5000., 500.}>
    {
      halp_meta(
          description,
          "Milliseconds a freed channel stays reserved for the previous "
          "note's release tail. Raise it for long synth releases; 0 to "
          "disable the reservation entirely.")
    } release_reserve;

    // ------------------------------------------------------------- Voice
    struct : halp::combobox_t<"Note Model", E2MNoteModel>
    {
      halp_meta(
          description,
          "Sustained: the note IS the entity - one note-on when it appears, "
          "one note-off when it leaves, expression in between. Triggered: "
          "notes are short events fired when an entity appears (and "
          "re-fired on pitch change in Retrigger tracking), for percussive "
          "material. The two have different failure profiles on purpose.")
      struct range
      {
        std::string_view values[2]{"Sustained", "Triggered"};
        E2MNoteModel init{E2MNoteModel::Sustained};
      };
    } note_model;

    struct : halp::combobox_t<"Trigger On", E2MTriggerOn>
    {
      halp_meta(
          description,
          "Confirmed waits until the tracker has real evidence (~100 ms) - "
          "no ghost notes from noise. First detection fires on the very "
          "first sighting for the lowest possible onset latency, at the "
          "price of occasional false starts.")
      struct range
      {
        std::string_view values[2]{"Confirmed", "First detection"};
        E2MTriggerOn init{E2MTriggerOn::Confirmed};
      };
    } trigger_on;

    struct : halp::spinbox_i32<"Max Voices", halp::irange{1, 16, 8}>
    {
      halp_meta(
          description,
          "How many entities sound at once. Fewer audible voices than "
          "trackable entities keeps the result legible - the audience can "
          "follow one person. Kept below the MPE member count so channel "
          "sharing (which fuses two people into one voice) never happens.")
    } max_voices;

    struct : halp::combobox_t<"Steal Policy", E2MPriority>
    {
      halp_meta(
          description,
          "Who loses their voice when a new entity arrives and all voices "
          "are busy. Confidence x Age protects long-standing, well-tracked "
          "entities - a newcomer is the least musically committed. The "
          "others name the victim directly. A confirmed entity is never "
          "stolen for a provisional one.")
      struct range
      {
        std::string_view values[5]{
            "Confidence x Age", "Steal oldest", "Steal newest", "Steal slowest",
            "Steal least confident"};
        E2MPriority init{E2MPriority::ConfidenceAge};
      };
    } priority;

    struct : halp::toggle<"Allow Stealing", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Off: when every voice is busy, new entities are denied (and "
          "counted on the Denied output) instead of interrupting a playing "
          "note.")
    } allow_steal;

    struct : halp::knob_f32<"Steal Margin", halp::range{0., 1., 0.1}>
    {
      halp_meta(
          description,
          "A newcomer must beat the weakest voice's priority by this much "
          "to steal it. Without the margin, two near-equal entities "
          "ping-pong one voice into a stutter.")
    } steal_margin;

    struct : halp::spinbox_f32<"Lost Grace", halp::range{0., 5000., 250.}>
    {
      halp_meta(
          description,
          "Milliseconds a note is held after its entity vanishes from the "
          "tracker. If the entity comes back within the grace it resumes "
          "the same note on the same channel - no retrigger, no channel "
          "jump. Raise it for flickery detectors.")
    } lost_grace;

    struct : halp::spinbox_f32<"Min Note", halp::range{0., 2000., 80.}>
    {
      halp_meta(
          description,
          "Shortest note the object will ever emit, in ms. An entity that "
          "blinks out immediately still produces an audible note instead "
          "of an unmusical click.")
    } min_note;

    struct : halp::spinbox_f32<"Max Note", halp::range{0., 60000., 0.}>
    {
      halp_meta(
          description,
          "Hard ceiling on note length in ms; the note is released even if "
          "the entity stays. 0 = unlimited (the note lives as long as the "
          "entity).")
    } max_note;

    struct : halp::spinbox_f32<"Trigger Length", halp::range{10., 5000., 200.}>
    {
      halp_meta(
          description,
          "Note duration in Triggered mode, in ms. Ignored in Sustained "
          "mode, where the entity itself decides.")
    } trigger_duration;

    struct : halp::spinbox_f32<"Retrigger Lockout", halp::range{0., 2000., 120.}>
    {
      halp_meta(
          description,
          "Minimum ms between two note-ons for the same entity. The "
          "machine-gun brake for Triggered mode and for entities that "
          "flicker in and out beyond the lost grace.")
    } retrig_lockout;

    struct : halp::spinbox_f32<"Watchdog", halp::range{0., 10000., 1000.}>
    {
      halp_meta(
          description,
          "Independent safety net: if no tracking data at all arrives for a "
          "held note for this many ms (tracker crashed, cable unplugged), "
          "the note is released anyway. 0 disables it. This is deliberately "
          "separate from the tracker's own lifecycle.")
    } watchdog;

    struct : halp::combobox_t<"While Coasting", E2MCoast>
    {
      halp_meta(
          description,
          "What expression does while the tracker is coasting (predicting "
          "through an occlusion). Freeze holds the last real values - "
          "nothing moves that no one moved. Follow trusts the prediction. "
          "Fade lets pressure sink towards silence until the entity is "
          "seen again.")
      struct range
      {
        std::string_view values[3]{"Freeze", "Follow", "Fade"};
        E2MCoast init{E2MCoast::Freeze};
      };
    } coast;

    // ------------------------------------------------------------- Pitch
    struct : halp::combobox_t<"Pitch Axis", E2MAxis>
    {
      halp_meta(
          description,
          "Which movement axis drives pitch. Vertical (Y) reads as "
          "high-note/high-position without explanation - the most legible "
          "default for an audience.")
      struct range
      {
        std::string_view values[3]{"X", "Y", "Z"};
        E2MAxis init{E2MAxis::Y};
      };
    } pitch_axis;

    struct : halp::toggle<"Invert Axis", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Flip the axis. On by default because camera coordinates grow "
          "downwards: inverted, standing tall plays high.")
    } pitch_invert;

    struct : halp::spinbox_f32<"Position Min", halp::range{-1000., 1000., 0.}>
    {
      halp_meta(
          description,
          "Axis value that maps to the lowest pitch. The computer-vision "
          "chain normalises positions to 0..1, hence the default.")
    } in_lo;

    struct : halp::spinbox_f32<"Position Max", halp::range{-1000., 1000., 1.}>
    {
      halp_meta(
          description,
          "Axis value that maps to the highest pitch. Narrow the min/max "
          "window to make a small stage area cover the whole range.")
    } in_hi;

    struct : halp::spinbox_i32<"Lowest Pitch", halp::irange{0, 127, 48}>
    {
      halp_meta(description, "Bottom of the pitch range (MIDI note, 48 = C3).")
    } pitch_lo;

    struct : halp::spinbox_i32<"Highest Pitch", halp::irange{0, 127, 84}>
    {
      halp_meta(description, "Top of the pitch range (MIDI note, 84 = C6).")
    } pitch_hi;

    struct : halp::combobox_t<"Pitch Tracking", E2MPitchTracking>
    {
      halp_meta(
          description,
          "Continuous bend: the note number is set once and per-note pitch "
          "bend follows the entity - glissando or, with a scale, glides "
          "that land exactly on scale notes. Latched: pitch is fixed at "
          "note-on and never moves. Retrigger fires a new note on every "
          "scale-step change and is only meaningful in Triggered mode.")
      struct range
      {
        std::string_view values[3]{"Continuous bend", "Latched", "Retrigger"};
        E2MPitchTracking init{E2MPitchTracking::ContinuousBend};
      };
    } pitch_tracking;

    struct : halp::spinbox_f32<"Glide", halp::range{0., 2000., 60.}>
    {
      halp_meta(
          description,
          "Slew time in ms of the pitch bend towards its target. Short = "
          "tight tracking; long = portamento. Per the MPE spec the slew "
          "stops the instant the note is released.")
    } glide;

    struct : halp::combobox_t<"Scale", E2MScale>
    {
      halp_meta(
          description,
          "Snap pitch to a scale. None is a theremin: raw continuous "
          "pitch. With a scale, position picks the nearest scale note and "
          "the bend glides between them - continuous and quantised pitch "
          "are the same mechanism. Pentatonic scales make any combination "
          "of entities consonant, which is why installations use them.")
      struct range
      {
        std::string_view values[13]{
            "None", "Chromatic", "Major", "Natural minor", "Harmonic minor",
            "Major pentatonic", "Minor pentatonic", "Blues", "Dorian", "Phrygian",
            "Lydian", "Mixolydian", "Whole tone"};
        E2MScale init{E2MScale::MinorPentatonic};
      };
    } scale;

    struct : halp::combobox_t<"Root", E2MRoot>
    {
      halp_meta(description, "Root note of the scale.")
      struct range
      {
        std::string_view values[12]{
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        E2MRoot init{E2MRoot::C};
      };
    } root;

    struct : halp::knob_f32<"Snap Hysteresis", halp::range{0., 0.5, 0.15}>
    {
      halp_meta(
          description,
          "Fraction of a scale step an entity must travel past the "
          "boundary before the quantiser switches notes. Stops a body "
          "hovering on a boundary from machine-gunning between two "
          "pitches - the hysteresis lives here at the quantiser, where the "
          "chatter actually happens.")
    } quant_hyst;

    // ------------------------------------------------------------- Velocity
    struct : halp::combobox_t<"Velocity From", E2MVelSource>
    {
      halp_meta(
          description,
          "What sets note velocity. Entry speed: how fast the entity was "
          "moving when it appeared - a run onto the stage hits hard, a "
          "drift in whispers. Fixed: constant. Confidence: the tracker's "
          "certainty about the entity.")
      struct range
      {
        std::string_view values[3]{"Entry speed", "Fixed", "Confidence"};
        E2MVelSource init{E2MVelSource::EntrySpeed};
      };
    } vel_source;

    struct : halp::spinbox_i32<"Fixed Velocity", halp::irange{1, 127, 100}>
    {
      halp_meta(description, "Velocity used when Velocity From is Fixed.")
    } vel_fixed;

    struct : halp::spinbox_i32<"Velocity Min", halp::irange{1, 127, 40}>
    {
      halp_meta(
          description,
          "Softest velocity. 40 keeps even the gentlest entrance audible.")
    } vel_lo;

    struct : halp::spinbox_i32<"Velocity Max", halp::irange{1, 127, 110}>
    {
      halp_meta(description, "Hardest velocity, reached at Speed Reference.")
    } vel_hi;

    struct : halp::spinbox_f32<"Speed Reference", halp::range{0.01, 100., 2.}>
    {
      halp_meta(
          description,
          "Speed (in tracker coordinate units/s) that maps to maximum "
          "velocity and full Speed expression. In normalised camera space, "
          "2.0 means crossing the whole frame in half a second.")
    } speed_ref;

    struct : halp::spinbox_f32<"Speed Window", halp::range{0., 500., 100.}>
    {
      halp_meta(
          description,
          "Entry-speed velocity uses the PEAK speed over this many ms "
          "before the note starts. Measuring at the exact instant of "
          "confirmation catches the tracker's smoother still converging "
          "and every note comes out the same - the peak over a short "
          "window is the real gesture.")
    } preroll;

    // ------------------------------------------------------------- Expression
    struct : halp::combobox_t<"Pressure From", E2MSource>
    {
      halp_meta(
          description,
          "What drives per-note pressure (the loudness/intensity dimension "
          "on MPE synths). Speed: moving fast presses hard - stillness is "
          "silence, motion is sound. In Single-channel mode this goes out "
          "as polyphonic aftertouch.")
      struct range
      {
        // Order must match E2MSource exactly.
        std::string_view values[27]{
            "None",           "X",
            "Y",              "Z",
            "Radius",         "Speed",
            "Velocity X",     "Velocity Y",
            "Velocity Z",     "|Velocity X|",
            "|Velocity Y|",   "|Velocity Z|",
            "Acceleration",   "Accel X",
            "Accel Y",        "Accel Z",
            "Jerk",           "Turn rate",
            "Curvature",      "Heading sin",
            "Heading cos",    "Agitation",
            "Nearest neighbour", "Distance to centroid",
            "Density",        "Age",
            "Confidence"};

        E2MSource init{E2MSource::Speed};
      };
    } pressure_src;

    struct : halp::combobox_t<"Timbre From", E2MSource>
    {
      halp_meta(
          description,
          "What drives the timbre control (CC74, the brightness dimension "
          "on MPE synths). Defaults to X so left-right position colours "
          "the sound while height plays pitch.")
      struct range
      {
        // Order must match E2MSource exactly.
        std::string_view values[27]{
            "None",           "X",
            "Y",              "Z",
            "Radius",         "Speed",
            "Velocity X",     "Velocity Y",
            "Velocity Z",     "|Velocity X|",
            "|Velocity Y|",   "|Velocity Z|",
            "Acceleration",   "Accel X",
            "Accel Y",        "Accel Z",
            "Jerk",           "Turn rate",
            "Curvature",      "Heading sin",
            "Heading cos",    "Agitation",
            "Nearest neighbour", "Distance to centroid",
            "Density",        "Age",
            "Confidence"};

        E2MSource init{E2MSource::X};
      };
    } timbre_src;

    struct : halp::spinbox_i32<"Timbre CC", halp::irange{0, 127, 74}>
    {
      halp_meta(
          description,
          "Controller number for the timbre dimension. 74 is the MPE "
          "standard; change it only for non-MPE receivers.")
    } timbre_cc;

    struct : halp::spinbox_f32<"Rise", halp::range{0., 2000., 20.}>
    {
      halp_meta(
          description,
          "Smoothing time when expression values increase. Fast attack "
          "with slow release is the difference between responsive and "
          "twitchy - a symmetric filter cannot give you both.")
    } rise;

    struct : halp::spinbox_f32<"Fall", halp::range{0., 2000., 80.}>
    {
      halp_meta(
          description,
          "Smoothing time when expression values decrease. Longer than "
          "Rise so gestures speak instantly and decay musically.")
    } fall;

    struct : halp::spinbox_f32<"Expression Rate", halp::range{1., 200., 50.}>
    {
      halp_meta(
          description,
          "Updates per second for each expression dimension of each note. "
          "50 Hz is imperceptible from continuous; lower it for DIN ports "
          "or receivers that choke on dense CC streams.")
    } expr_rate;

    struct : halp::knob_f32<"Deadband", halp::range{0., 0.1, 0.005}>
    {
      halp_meta(
          description,
          "Suppress expression changes smaller than this fraction of full "
          "scale. Kills the byte stream of an entity standing still "
          "without affecting real motion.")
    } deadband;

    // ------------------------------------------------------------- Rhythm
    struct : halp::combobox_t<"Beat Quantize", E2MQuantTargets>
    {
      halp_meta(
          description,
          "Snap notes to the musical grid of the score (which a Beat "
          "Tracker can be driving live from a drummer). Onsets delays each "
          "note-on to the nearest grid point - at most half a division. "
          "Adding offsets snaps releases too. A note-off is never allowed "
          "to land before its note-on.")
      struct range
      {
        std::string_view values[3]{"Off", "Onsets", "Onsets + offsets"};
        E2MQuantTargets init{E2MQuantTargets::Off};
      };
    } quant_mode;

    struct : halp::combobox_t<"Grid", E2MGrid>
    {
      halp_meta(
          description,
          "Grid division notes snap to. Finer grids mean less hold "
          "latency: at 120 BPM, 1/8 holds at most 125 ms, 1/16 at most "
          "62 ms.")
      struct range
      {
        std::string_view values[7]{"1/1",  "1/2",  "1/4",   "1/8",
                                   "1/16", "1/8T", "1/16T"};
        E2MGrid init{E2MGrid::Eighth};
      };
    } grid;

    struct : halp::knob_f32<"Quantize Strength", halp::range{0., 1., 1.}>
    {
      halp_meta(
          description,
          "Blend between the gesture's own timing (0) and the grid (1). "
          "Partial values keep the human feel while tightening it.")
    } strength;

    struct : halp::spinbox_f32<"Max Hold", halp::range{0., 2000., 250.}>
    {
      halp_meta(
          description,
          "Longest a note will wait for its grid point, in ms. If the "
          "transport stalls or the beat tracker drops out, the note plays "
          "anyway instead of being stranded forever.")
    } max_hold;

    // ------------------------------------------------------------- Safety
    struct : halp::impulse_button<"Panic">
    {
      halp_meta(
          description,
          "Release every held note now: per-voice note-offs, then All "
          "Sound Off / All Notes Off / Reset Controllers on every touched "
          "channel, then bend/pressure/timbre reset - messages spaced so a "
          "DIN receiver is not overrun at the exact moment it must work.")
      void update(auto& self) { self.panic_requested = true; }
    } panic;

    struct : halp::toggle<"Panic On Stop", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Run the panic routine when the transport stops or jumps, so "
          "stopping the score never leaves a synth droning. Leave on "
          "unless you are debugging the routine itself.")
    } panic_on_stop;

    struct : halp::spinbox_f32<"Panic Spacing", halp::range{0., 20., 1.}>
    {
      halp_meta(
          description,
          "Milliseconds between consecutive panic and configuration "
          "messages. 1 ms keeps a DIN receiver from being overrun by the "
          "burst; 0 sends everything back-to-back.")
    } spacing;

    // ------------------------------------------------- Analysis references
    //
    // Declared last on purpose: the flattened input order is what .scp presets
    // index into, so appending leaves every existing preset entry pointing at
    // the control it was written for. Their place in the UI is set by the
    // layout below, not by this order.

    struct : halp::spinbox_f32<"Acceleration Ref", halp::range{0.01, 200., 8.}>
    {
      halp_meta(
          description,
          "Acceleration, in coordinate units per second squared, that maps to "
          "the top of the range. Much larger than the speed reference: a "
          "gesture that reverses within a tenth of a second is already tens of "
          "units per second squared.")
    } accel_ref;

    struct : halp::spinbox_f32<"Jerk Ref", halp::range{0.1, 5000., 200.}>
    {
      halp_meta(
          description,
          "Jerk (units per second cubed) that maps to the top of the range. "
          "Jerk is the sharpest onset cue there is, and also the noisiest "
          "descriptor - raise Descriptor Smoothing with it.")
    } jerk_ref;

    struct : halp::spinbox_f32<"Turn Rate Ref", halp::range{0.1, 50., 6.28}>
    {
      halp_meta(
          description,
          "Turn rate, in radians per second, that maps to the top of the "
          "range. The default is one full turn per second.")
    } turn_ref;

    struct : halp::spinbox_f32<"Neighbour Range", halp::range{0.001, 100., 0.25}>
    {
      halp_meta(
          description,
          "Radius, in coordinate units, within which another entity counts as "
          "a neighbour. Sets the scale of both Nearest Neighbour and Density: "
          "roughly how close two people have to be to read as together.")
    } neighbour_range;

    struct : halp::spinbox_f32<"Age Ref", halp::range{0.1, 600., 10.}>
    {
      halp_meta(
          description,
          "How long an entity must have been present, in seconds, for Age to "
          "reach the top of the range - the length of the arc a mapping from "
          "Age draws.")
    } age_ref;

    struct : halp::spinbox_f32<"Descriptor Smoothing", halp::range{0., 2000., 80.}>
    {
      halp_meta(
          description,
          "Time constant, in milliseconds, applied to the derivative-based "
          "descriptors (acceleration, jerk, turn rate, agitation). Each "
          "derivative multiplies the detector's position noise by about 1/dt, "
          "so acceleration carries it squared and jerk cubed: without "
          "smoothing they are noise. Raise it until the mapping is playable.")
    } desc_smooth;
  } inputs;

  struct outs
  {
    struct : halp::midi_out_bus<"MIDI">
    {
      halp_meta(
          description,
          "The MIDI stream: notes, per-note expression, MPE configuration "
          "and panic messages, sample-accurately timestamped. Cable it to "
          "a MIDI device or another process.")
      ossia::net::node_base* ossia_node{};
    } midi;

    struct
    {
      halp_meta(name, "Active Voices")
      halp_meta(
          description,
          "Number of notes currently sounding (including notes held "
          "through a tracking dropout).")
      int value{};
    } active;

    struct
    {
      halp_meta(name, "Denied")
      halp_meta(
          description,
          "Running count of entities that wanted a voice and were refused "
          "(voices full and stealing off or not worth it). If this climbs "
          "during a show, raise Max Voices or loosen the steal policy - "
          "silent drops are how installations get debugged at 2 am.")
      int value{};
    } denied;

  } outputs;

  using tick = halp::tick_musical;

  // ======================================================== implementation

  static constexpr int k_max_voices = 16;

  enum class vstate : uint8_t
  {
    off,        // free slot
    pending_on, // waiting for grid point / not yet emitted
    sounding,   // note-on on the wire
  };

  struct voice
  {
    int32_t id = -1;
    int8_t channel = -1;
    uint8_t note = 60;
    vstate st = vstate::off;
    bool on_wire = false;         // note-on emitted, note-off not yet
    bool entity_confirmed = false;
    bool missing = false;         // entity absent from the last track list
    bool coasting = false;        // tracker reports coasting
    bool off_scheduled = false;
    //! A retrigger has been requested and the follow-up note is queued in
    //! m_retrig_ids. Guards against requesting it twice while the shortened
    //! note-off drains.
    bool retrig_pending = false;
    bool note_suppressed = false; // single-channel (channel,pitch) collision
    double missing_since = 0.;
    double last_seen = 0.;        // watchdog: last time data for id arrived
    double on_time = 0.;          // when the note-on was emitted
    double on_raw_t = 0.;         // when the trigger happened (pre-quantise)
    double on_target_q = -1.;     // grid target in quarters, < 0 = immediate
    double off_raw_t = 0.;
    double off_target_q = -1.;
    float priority = 0.f;
    // mapping state
    float pitch_target = 60.f;    // quantised, continuous semitones
    float quant_prev = -1.f;      // scale quantiser memory
    float bend_semis = 0.f;       // current slewed bend offset
    float pressure_v = 0.f;       // smoothed 0..1
    float timbre_v = 0.f;
    float pressure_t = 0.f;       // targets 0..1
    float timbre_t = 0.f;
    int last_bend = -1;           // last emitted values, for deadband
    int last_pressure = -1;
    int last_timbre = -1;
    double expr_acc = 1e9;        // time since last expression send
    uint8_t velocity = 100;
  };

  struct chan_info
  {
    bool in_use = false;
    double freed_at = -1e18;
  };

  struct timed_msg
  {
    double t;
    uint8_t n;
    uint8_t bytes[3];
  };

  struct out_msg
  {
    int64_t ts;
    uint8_t prio; // 0 off/panic, 1 on, 2 expression, 3 global
    uint8_t n;
    uint8_t bytes[3];
  };

  struct speed_hist
  {
    static constexpr int N = 24;
    double t[N] = {};
    float s[N] = {};
    int head = 0;
    double last_t = 0.;
    void push(double time, float speed) noexcept
    {
      t[head] = time;
      s[head] = speed;
      head = (head + 1) % N;
      last_t = time;
    }
    float peak_since(double t0) const noexcept
    {
      float m = 0.f;
      for(int i = 0; i < N; i++)
        if(t[i] >= t0 && s[i] > m)
          m = s[i];
      return m;
    }
  };

  //! Per-id bookkeeping for entities that arrive as bare positions: what the
  //! source does not send (age, velocity) is differenced from these.
  struct synth_state
  {
    double time = 0.;
    double first_seen = 0.;
    //! Seconds since the previous frame this entity appeared in. Recorded by
    //! parse_entities before it advances `time`, because the analysis runs
    //! afterwards and would otherwise only ever see a zero interval.
    double last_dt = 0.;
    decltype(track_record::position) pos{};

    // --- analysis history ---
    //
    // Each derivative multiplies the detector's position noise by roughly
    // 1/dt, so acceleration carries it squared and jerk cubed. At 60 Hz that
    // turns a millimetre of jitter into metres per second squared. Every one
    // of them is therefore low-passed before it is offered as a mapping
    // source; the raw values would be unplayable.
    decltype(track_record::velocity) vel{};   //!< previous velocity
    decltype(track_record::velocity) accel{}; //!< previous acceleration
    float heading = 0.f;                      //!< unwrapped, so it can be smoothed
    bool has_heading = false;

    ossia::one_pole_filter<float> f_accel, f_jerk, f_turn, f_agitation;
    ossia::one_pole_filter<float> f_ax, f_ay, f_az;
    //! Mean velocity, against which the deviation that defines agitation is
    //! measured: a fast but steady walk is not agitated, a slow fidget is.
    ossia::one_pole_filter<float> f_vx, f_vy, f_vz;
  };
  ossia::flat_map<int, synth_state> m_synth;
  std::vector<track_record> m_recs;

  //! Per-entity analysis for the current frame, keyed by entity id so the
  //! mapping lookups can stay in terms of the record they already hold.
  ossia::flat_map<int, entity_descriptors> m_desc;

  bool tracks_dirty = false;
  bool panic_requested = false;

  // What the stop-time panic sent (or would have sent, if no device is
  // bound): raw 3-byte messages, in order. Public for tests and post-mortems.
  std::vector<std::array<uint8_t, 3>> last_direct_panic;

  // exec-state hookup: lets the binding resolve outputs.midi.ossia_node so
  // the stop-time panic can reach the device even though ticks have ended.
  ossia::exec_state_facade ossia_state;
  std::atomic<ossia::net::midi::midi_protocol*> midi_out{};

  void prepare(halp::setup s)
  {
    if(s.rate > 0)
      m_rate = s.rate;
  }

  void start()
  {
    m_do_config = true;
    m_started = true;
  }

  void stop()
  {
    if(inputs.panic_on_stop)
      panic_direct();
    hard_reset();
    m_started = false;
  }

  void pause()
  {
    if(inputs.panic_on_stop)
      panic_direct();
    hard_reset();
  }

  void resume() { m_do_config = true; }

  void transport(auto flicks)
  {
    // A transport jump mid-performance: release everything cleanly on the
    // next tick; live entities immediately re-acquire their notes.
    if(inputs.panic_on_stop)
      panic_requested = true;
  }

  void operator()(const halp::tick_musical& tk)
  {
    const int frames = std::max(tk.frames, 1);
    const double dt = frames / m_rate;
    const double now = m_now;
    const double t_end = now + dt;

    resolve_protocol();

    m_msgs.clear();

    // Detect an output-config change while running: panic + reconfigure.
    const uint64_t sig = config_signature();
    if(sig != m_config_sig)
    {
      if(m_config_sig != 0 && held_count() > 0)
        queue_panic();
      m_config_sig = sig;
      m_do_config = true;
    }

    if(m_do_config)
    {
      m_do_config = false;
      if(inputs.send_config && inputs.output_mode.value == E2MOutputMode::MPE)
        queue_mpe_config();
    }

    if(panic_requested)
    {
      panic_requested = false;
      queue_panic();
    }

    if(tracks_dirty)
    {
      tracks_dirty = false;
      parse_entities(now);
      analyse(now);
      ingest(now);
    }

    lifecycle(now, tk);
    emit_pending_notes(now, dt, frames, tk);
    update_expression(now, dt, frames);
    flush_timed(now, dt, frames);

    // Sort by timestamp; at equal timestamps note-offs go before note-ons
    // before expression, so a steal never produces on/on/off.
    std::stable_sort(m_msgs.begin(), m_msgs.end(), [](const out_msg& a, const out_msg& b) {
      return a.ts != b.ts ? a.ts < b.ts : a.prio < b.prio;
    });
    for(const auto& m : m_msgs)
    {
      auto& msg = outputs.midi.midi_messages.emplace_back();
      msg.bytes.assign(m.bytes, m.bytes + m.n);
      msg.timestamp = std::clamp<int64_t>(m.ts, 0, frames - 1);
    }

    // Invariants 2 and 4, asserted on the wire itself: replay this tick's
    // messages, in their final order, into a persistent model of what the
    // receiver holds. A note-on to a (channel, pitch) still sounding - e.g.
    // to a voice whose note-off is later in the buffer - or an off for a
    // pitch not held is recorded and reported by check_invariants().
    for(const auto& m : m_msgs)
    {
      const uint8_t st = m.bytes[0] & 0xF0;
      const int ch = m.bytes[0] & 0x0F;
      if(st == 0x90 && m.bytes[2] > 0)
      {
        if(m_wire[ch].test(m.bytes[1]))
          m_wire_err = "note-on while the (channel, pitch) is still held on the wire";
        m_wire[ch].set(m.bytes[1]);
      }
      else if(st == 0x80 || (st == 0x90 && m.bytes[2] == 0))
      {
        if(!m_wire[ch].test(m.bytes[1]))
          m_wire_err = "note-off for a (channel, pitch) not held on the wire";
        m_wire[ch].reset(m.bytes[1]);
      }
    }

    outputs.active.value = held_count();
    outputs.denied.value = m_denied;

    m_now = t_end;
  }

  // ------------------------------------------------------------- invariants
  // Callable from tests: returns false and fills err on the first violation.
  bool check_invariants(std::string* err = nullptr) const
  {
    // 2 & 4, wire form: the replayed receiver model caught an on to a
    //    still-sounding (channel, pitch) or an off for a free one.
    if(!m_wire_err.empty())
    {
      if(err)
        *err = m_wire_err;
      return false;
    }
    // 1. every on-wire note belongs to a live voice, and the outstanding
    //    counter matches.
    int on_wire = 0;
    for(const auto& v : m_voices)
      if(v.on_wire)
      {
        on_wire++;
        if(v.st == vstate::off)
        {
          if(err)
            *err = "note on wire for a dead voice";
          return false;
        }
      }
    if(on_wire != m_outstanding)
    {
      if(err)
        *err = "outstanding note count mismatch";
      return false;
    }
    // 2. at most one outstanding note per (channel, pitch)
    for(int i = 0; i < k_max_voices; i++)
      for(int j = i + 1; j < k_max_voices; j++)
        if(m_voices[i].on_wire && m_voices[j].on_wire
           && m_voices[i].channel == m_voices[j].channel
           && m_voices[i].note == m_voices[j].note)
        {
          if(err)
            *err = "duplicate (channel, pitch) on wire";
          return false;
        }
    return true;
  }

  int held_count() const noexcept
  {
    int n = 0;
    for(const auto& v : m_voices)
      if(v.on_wire)
        n++;
    return n;
  }

  int outstanding() const noexcept { return m_outstanding; }

  //! The entities as decoded from the inlet, for tests: the decoding is where
  //! a source's data is either preserved or quietly lost, and asserting on the
  //! emitted MIDI alone cannot tell a dropped field from a deliberate one.
  const std::vector<track_record>& parsed_entities() const noexcept { return m_recs; }

  //! The analysis for the current frame, for tests: a descriptor that is
  //! subtly wrong still produces plausible MIDI, so asserting on the notes
  //! alone cannot tell a working mapping from a broken one.
  const ossia::flat_map<int, entity_descriptors>& descriptors() const noexcept
  {
    return m_desc;
  }

private:
  // ------------------------------------------------------------- channels
  bool is_mpe() const noexcept
  {
    return inputs.output_mode.value == E2MOutputMode::MPE;
  }
  bool is_single() const noexcept
  {
    return inputs.output_mode.value == E2MOutputMode::SingleChannel;
  }

  int master_channel() const noexcept
  {
    return inputs.mpe_zone.value == E2MZone::Lower ? 0 : 15;
  }

  int member_count() const noexcept
  {
    if(is_single())
      return 1;
    int n = std::clamp(inputs.member_channels.value, 1, is_mpe() ? 15 : 16);
    return n;
  }

  // The i-th note channel (0-based MIDI channel number).
  int member_channel(int i) const noexcept
  {
    if(is_single())
      return std::clamp(inputs.single_channel.value - 1, 0, 15);
    if(is_mpe())
      return inputs.mpe_zone.value == E2MZone::Lower ? 1 + i : 14 - i;
    return i; // channel-per-entity: channels 1..N (0-based 0..N-1)
  }

  int alloc_channel(double now) noexcept
  {
    if(is_single())
      return member_channel(0);

    const int n = member_count();
    const bool lru = inputs.channel_reuse.value == E2MChannelReuse::LRU;
    const double reserve = inputs.release_reserve.value * 1e-3;

    int best = -1;
    double best_score = 0.;
    bool best_ok = false;
    for(int i = 0; i < n; i++)
    {
      const int c = member_channel(i);
      const auto& ci = m_chans[c];
      if(ci.in_use)
        continue;
      const double idle = now - ci.freed_at;
      const bool ok = !lru || idle >= reserve;
      // Prefer channels past the release reserve; among those (or among
      // reserved ones if none qualify) pick the least recently freed.
      if(best < 0 || (ok && !best_ok) || (ok == best_ok && idle > best_score))
      {
        best = c;
        best_score = idle;
        best_ok = ok;
      }
    }
    if(best >= 0)
      m_chans[best].in_use = true;
    return best;
  }

  void free_channel(int c, double t) noexcept
  {
    if(c >= 0 && c < 16)
    {
      m_chans[c].in_use = false;
      m_chans[c].freed_at = t;
      m_touched[c] = true;
    }
  }

  // ------------------------------------------------------------- mapping
  static float axis_of(const track_record& r, E2MAxis a) noexcept
  {
    switch(a)
    {
      case E2MAxis::X:
        return r.position.x;
      case E2MAxis::Y:
        return r.position.y;
      case E2MAxis::Z:
        return r.position.z;
    }
    return 0.f;
  }

  float norm_axis(const track_record& r, E2MAxis a, bool invert) const noexcept
  {
    const float lo = inputs.in_lo.value, hi = inputs.in_hi.value;
    float n = hi != lo ? (axis_of(r, a) - lo) / (hi - lo) : 0.f;
    n = std::clamp(n, 0.f, 1.f);
    return invert ? 1.f - n : n;
  }

  float speed_of(const track_record& r) const noexcept
  {
    const float vx = r.velocity.x, vy = r.velocity.y, vz = r.velocity.z;
    return std::sqrt(vx * vx + vy * vy + vz * vz);
  }

  float norm_speed(const track_record& r) const noexcept
  {
    const float ref = std::max(inputs.speed_ref.value, 1e-3f);
    return std::clamp(speed_of(r) / ref, 0.f, 1.f);
  }

  float raw_pitch(const track_record& r) const noexcept
  {
    const float n = norm_axis(r, inputs.pitch_axis.value, inputs.pitch_invert.value);
    const float lo = float(inputs.pitch_lo.value);
    const float hi = float(inputs.pitch_hi.value);
    return lo + n * (hi - lo);
  }

  static const uint16_t* scale_mask(E2MScale s) noexcept
  {
    // Bit i set = pitch class i (relative to root) is in the scale.
    static constexpr uint16_t masks[13] = {
        0,                // None (unused)
        0b111111111111,   // Chromatic
        0b101010110101,   // Major:            0 2 4 5 7 9 11
        0b010110101101,   // Natural minor:    0 2 3 5 7 8 10
        0b100110101101,   // Harmonic minor:   0 2 3 5 7 8 11
        0b001010010101,   // Major pentatonic: 0 2 4 7 9
        0b010010101001,   // Minor pentatonic: 0 3 5 7 10
        0b010011101001,   // Blues:            0 3 5 6 7 10
        0b011010101101,   // Dorian:           0 2 3 5 7 9 10
        0b010110101011,   // Phrygian:         0 1 3 5 7 8 10
        0b101011010101,   // Lydian:           0 2 4 6 7 9 11
        0b011010110101,   // Mixolydian:       0 2 4 5 7 9 10
        0b010101010101,   // Whole tone:       0 2 4 6 8 10
    };
    return &masks[int(s)];
  }

  bool in_scale(int note) const noexcept
  {
    const auto s = inputs.scale.value;
    if(s == E2MScale::None || s == E2MScale::Chromatic)
      return true;
    const int root = int(inputs.root.value);
    const int pc = ((note - root) % 12 + 12) % 12;
    return (*scale_mask(s) >> pc) & 1;
  }

  float nearest_scale_note(float raw) const noexcept
  {
    const auto s = inputs.scale.value;
    if(s == E2MScale::None)
      return raw;
    if(s == E2MScale::Chromatic)
      return std::round(raw);
    float best = std::round(raw);
    float best_d = 1e9f;
    const int c = int(std::floor(raw));
    for(int k = c - 12; k <= c + 13; k++)
    {
      if(k < 0 || k > 127 || !in_scale(k))
        continue;
      const float d = std::abs(raw - float(k));
      if(d < best_d)
      {
        best_d = d;
        best = float(k);
      }
    }
    return best;
  }

  // Scale quantiser with hysteresis at the quantiser (not at the sensor):
  // switching to a new scale note requires the raw pitch to travel past the
  // midpoint by quant_hyst * the distance between the two candidates.
  float quantize_pitch(float raw, voice& v) const noexcept
  {
    if(inputs.scale.value == E2MScale::None)
    {
      v.quant_prev = -1.f;
      return raw;
    }
    const float cand = nearest_scale_note(raw);
    if(v.quant_prev < 0.f)
    {
      v.quant_prev = cand;
      return cand;
    }
    if(cand != v.quant_prev)
    {
      const float step = std::abs(cand - v.quant_prev);
      const float mid = 0.5f * (cand + v.quant_prev);
      const float h = inputs.quant_hyst.value * step;
      if((cand > v.quant_prev && raw > mid + h) || (cand < v.quant_prev && raw < mid - h))
        v.quant_prev = cand;
    }
    return v.quant_prev;
  }

  //! Look a descriptor up for an entity. Everything the analysis produced is
  //! already normalised, so this is a pure selection; the position and
  //! confidence cases stay computed here because they need no history and so
  //! are correct even before the first analysis frame.
  //! Largest frame the pairwise relations are computed over. Nearest-neighbour
  //! and density are O(n^2); past this the entities are far more numerous than
  //! the voices that could ever sound anyway, so the relational descriptors
  //! hold their previous value rather than costing quadratic time on the audio
  //! thread.
  static constexpr std::size_t max_pairwise = 64;

  //! Turn the parsed entities into the normalised descriptor set the mappings
  //! read. Runs once per frame, after parse_entities has filled m_recs.
  void analyse(double now)
  {
    m_desc.clear();
    if(m_recs.empty())
      return;

    const float in_lo = inputs.in_lo.value, in_hi = inputs.in_hi.value;
    const float half = std::max(0.5f * (in_hi - in_lo), 1e-6f);
    // The Position Min/Max window applies to every axis, so one centre and one
    // half-extent describe the field.
    const float centre = 0.5f * (in_lo + in_hi);
    const bool is_3d = inputs.coords.value == E2MCoords::ThreeD;

    const float sref = std::max(inputs.speed_ref.value, 1e-6f);
    const float aref = std::max(inputs.accel_ref.value, 1e-6f);
    const float jref = std::max(inputs.jerk_ref.value, 1e-6f);
    const float tref = std::max(inputs.turn_ref.value, 1e-6f);
    const float nrange = std::max(inputs.neighbour_range.value, 1e-6f);
    const float tau = std::max(inputs.desc_smooth.value * 1e-3f, 1e-4f);

    // Centre of mass, for CentroidDistance.
    float gx = 0.f, gy = 0.f, gz = 0.f;
    for(const auto& r : m_recs)
    {
      gx += r.position.x;
      gy += r.position.y;
      gz += r.position.z;
    }
    const float inv_n = 1.f / float(m_recs.size());
    gx *= inv_n;
    gy *= inv_n;
    gz *= inv_n;

    const bool pairwise = m_recs.size() <= max_pairwise;

    for(std::size_t i = 0; i < m_recs.size(); i++)
    {
      const auto& r = m_recs[i];
      entity_descriptors d;

      auto st_it = m_synth.find(r.id);
      const double dt = (st_it != m_synth.end()) ? st_it->second.last_dt : 0.;
      const float fdt = float(dt > 1e-6 ? dt : 0.);
      const float alpha = fdt > 0.f ? ossia::lag_alpha(tau, fdt) : 1.f;

      // --- position ---
      d.x = norm_axis(r, E2MAxis::X, false);
      d.y = norm_axis(r, E2MAxis::Y, false);
      d.z = norm_axis(r, E2MAxis::Z, false);
      {
        // Only over the axes the source actually uses: in a 2D scene z is 0,
        // which is not the centre of the Position Min/Max window, so folding
        // it in would put every entity at full radius no matter where it is.
        const float dx = r.position.x - centre, dy = r.position.y - centre;
        float r2 = dx * dx + dy * dy;
        if(is_3d)
        {
          const float dz = r.position.z - centre;
          r2 += dz * dz;
        }
        d.radius = std::clamp(std::sqrt(r2) / half, 0.f, 1.f);
      }
      d.confidence = std::clamp(r.confidence, 0.f, 1.f);
      d.age = std::clamp(r.age / std::max(inputs.age_ref.value, 1e-6f), 0.f, 1.f);

      // --- velocity ---
      const float vx = r.velocity.x, vy = r.velocity.y, vz = r.velocity.z;
      const float sp = std::sqrt(vx * vx + vy * vy + vz * vz);
      d.speed = std::clamp(sp / sref, 0.f, 1.f);
      d.vx = bipolar(vx / sref);
      d.vy = bipolar(vy / sref);
      d.vz = bipolar(vz / sref);
      d.sx = std::clamp(std::abs(vx) / sref, 0.f, 1.f);
      d.sy = std::clamp(std::abs(vy) / sref, 0.f, 1.f);
      d.sz = std::clamp(std::abs(vz) / sref, 0.f, 1.f);

      // --- acceleration, jerk, heading: all need a previous frame ---
      if(st_it != m_synth.end() && fdt > 0.f)
      {
        auto& st = st_it->second;

        const float ax = (vx - st.vel.x) / fdt;
        const float ay = (vy - st.vel.y) / fdt;
        const float az = (vz - st.vel.z) / fdt;
        const float amag = std::sqrt(ax * ax + ay * ay + az * az);

        d.accel = std::clamp(st.f_accel(amag, alpha) / aref, 0.f, 1.f);
        d.ax = bipolar(st.f_ax(ax, alpha) / aref);
        d.ay = bipolar(st.f_ay(ay, alpha) / aref);
        d.az = bipolar(st.f_az(az, alpha) / aref);

        const float jx = (ax - st.accel.x) / fdt;
        const float jy = (ay - st.accel.y) / fdt;
        const float jz = (az - st.accel.z) / fdt;
        const float jmag = std::sqrt(jx * jx + jy * jy + jz * jz);
        d.jerk = std::clamp(st.f_jerk(jmag, alpha) / jref, 0.f, 1.f);

        // Heading on the ground plane: that is the direction an audience
        // reads, and it stays defined when an entity only moves in x/y.
        if(sp > 1e-5f)
        {
          const float raw = std::atan2(vy, vx);
          const float unwrapped
              = st.has_heading ? ossia::unwrap_angle(raw, st.heading) : raw;
          const float rate = st.has_heading ? (unwrapped - st.heading) / fdt : 0.f;

          d.turn_rate = std::clamp(std::abs(st.f_turn(rate, alpha)) / tref, 0.f, 1.f);
          // Curvature is turn per distance travelled rather than per second,
          // so a slow careful arc and a fast one read the same.
          d.curvature = std::clamp(std::abs(rate) / (sp * tref) * sref, 0.f, 1.f);
          d.heading_sin = bipolar(std::sin(raw));
          d.heading_cos = bipolar(std::cos(raw));

          st.heading = unwrapped;
          st.has_heading = true;
        }
        else
        {
          d.heading_sin = bipolar(std::sin(st.heading));
          d.heading_cos = bipolar(std::cos(st.heading));
        }

        // Agitation: how far the velocity strays from its own average. Speed
        // alone cannot tell a purposeful walk from a jitter.
        const float mvx = st.f_vx(vx, alpha), mvy = st.f_vy(vy, alpha),
                    mvz = st.f_vz(vz, alpha);
        const float ddx = vx - mvx, ddy = vy - mvy, ddz = vz - mvz;
        const float dev = std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz);
        d.agitation = std::clamp(st.f_agitation(dev, alpha) / sref, 0.f, 1.f);

        st.vel = r.velocity;
        st.accel = {ax, ay, az};
      }

      // --- relations to the other entities ---
      {
        const float dx = r.position.x - gx, dy = r.position.y - gy,
                    dz = r.position.z - gz;
        d.centroid_dist
            = std::clamp(std::sqrt(dx * dx + dy * dy + dz * dz) / half, 0.f, 1.f);
      }

      if(pairwise && m_recs.size() > 1)
      {
        float best = std::numeric_limits<float>::max();
        int within = 0;
        for(std::size_t j = 0; j < m_recs.size(); j++)
        {
          if(j == i)
            continue;
          const auto& o = m_recs[j];
          const float dx = r.position.x - o.position.x;
          const float dy = r.position.y - o.position.y;
          const float dz = r.position.z - o.position.z;
          const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
          best = std::min(best, dist);
          if(dist <= nrange)
            within++;
        }
        d.nearest = std::clamp(best / nrange, 0.f, 1.f);
        // Normalised against the frame: 1 means everyone else is inside the
        // radius, which is what "crowded" means here.
        d.density = std::clamp(float(within) / float(m_recs.size() - 1), 0.f, 1.f);
      }

      m_desc[r.id] = d;
    }
  }

  //! Fold a signed, reference-relative quantity into 0..1 with 0.5 at rest.
  //! MIDI is unipolar, so a signed source has to pick a convention; centring
  //! it is the one that keeps "not moving" at a fixed, meaningful value.
  static float bipolar(float v) noexcept
  {
    return std::clamp(0.5f + 0.5f * v, 0.f, 1.f);
  }

  float source_value(const track_record& r, E2MSource s) const noexcept
  {
    switch(s)
    {
      case E2MSource::None:
        return 0.f;
      case E2MSource::X:
        return norm_axis(r, E2MAxis::X, false);
      case E2MSource::Y:
        return norm_axis(r, E2MAxis::Y, false);
      case E2MSource::Z:
        return norm_axis(r, E2MAxis::Z, false);
      case E2MSource::Confidence:
        return std::clamp(r.confidence, 0.f, 1.f);
      default:
        break;
    }

    const auto it = m_desc.find(r.id);
    if(it == m_desc.end())
      return 0.f;
    const entity_descriptors& d = it->second;

    switch(s)
    {
      case E2MSource::Radius:
        return d.radius;
      case E2MSource::Speed:
        return d.speed;
      case E2MSource::VelocityX:
        return d.vx;
      case E2MSource::VelocityY:
        return d.vy;
      case E2MSource::VelocityZ:
        return d.vz;
      case E2MSource::SpeedX:
        return d.sx;
      case E2MSource::SpeedY:
        return d.sy;
      case E2MSource::SpeedZ:
        return d.sz;
      case E2MSource::Acceleration:
        return d.accel;
      case E2MSource::AccelX:
        return d.ax;
      case E2MSource::AccelY:
        return d.ay;
      case E2MSource::AccelZ:
        return d.az;
      case E2MSource::Jerk:
        return d.jerk;
      case E2MSource::TurnRate:
        return d.turn_rate;
      case E2MSource::Curvature:
        return d.curvature;
      case E2MSource::HeadingSin:
        return d.heading_sin;
      case E2MSource::HeadingCos:
        return d.heading_cos;
      case E2MSource::Agitation:
        return d.agitation;
      case E2MSource::NearestNeighbour:
        return d.nearest;
      case E2MSource::CentroidDistance:
        return d.centroid_dist;
      case E2MSource::Density:
        return d.density;
      case E2MSource::Age:
        return d.age;
      default:
        return 0.f;
    }
  }

  float entity_priority(const track_record& r) const noexcept
  {
    switch(inputs.priority.value)
    {
      case E2MPriority::ConfidenceAge:
        return std::clamp(r.confidence, 0.f, 1.f)
               * std::clamp(r.age / 1.f, 0.f, 1.f);
      case E2MPriority::StealOldest:
        return -r.age;
      case E2MPriority::StealNewest:
        return r.age;
      case E2MPriority::StealSlowest:
        return speed_of(r);
      case E2MPriority::StealLeastConfident:
        return r.confidence;
    }
    return 0.f;
  }

  // ------------------------------------------------------------- ingest
  voice* find_voice(int32_t id) noexcept
  {
    for(auto& v : m_voices)
      if(v.st != vstate::off && v.id == id)
        return &v;
    return nullptr;
  }

  voice* free_voice() noexcept
  {
    int used = 0;
    voice* free = nullptr;
    for(auto& v : m_voices)
    {
      if(v.st == vstate::off)
      {
        if(!free)
          free = &v;
      }
      else
        used++;
    }
    return used < std::min(inputs.max_voices.value, k_max_voices) ? free : nullptr;
  }

  static bool number_like(const ossia::value& v) noexcept
  {
    const auto t = v.get_type();
    return t == ossia::val_type::FLOAT || t == ossia::val_type::INT
           || t == ossia::val_type::BOOL;
  }
  static float to_float(const ossia::value& v) noexcept
  {
    return ossia::convert<float>(v);
  }

  //! Whether a vec3's third component is confidence rather than z, as declared
  //! by the Coordinates control.
  bool third_is_confidence() const noexcept
  {
    return inputs.coords.value == E2MCoords::TwoD;
  }

  //! Fill `out` from one element. Returns false if it carries no position.
  bool parse_entity(const ossia::value& v, bool third_conf, track_record& out) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::VEC2F: {
        const auto& a = *v.target<ossia::vec2f>();
        out.position = {a[0], a[1], 0.f};
        return true;
      }
      case ossia::val_type::VEC3F: {
        const auto& a = *v.target<ossia::vec3f>();
        if(third_conf)
        {
          out.position = {a[0], a[1], 0.f};
          out.confidence = a[2];
        }
        else
          out.position = {a[0], a[1], a[2]};
        return true;
      }
      case ossia::val_type::VEC4F: {
        const auto& a = *v.target<ossia::vec4f>();
        out.position = {a[0], a[1], a[2]};
        out.confidence = a[3];
        return true;
      }
      case ossia::val_type::LIST: {
        const auto& l = *v.target<std::vector<ossia::value>>();
        if(l.size() < 2 || !number_like(l[0]) || !number_like(l[1]))
          return false;
        out.position = {to_float(l[0]), to_float(l[1]), 0.f};
        if(l.size() >= 3 && number_like(l[2]))
        {
          if(third_conf)
            out.confidence = to_float(l[2]);
          else
            out.position.z = to_float(l[2]);
        }
        if(l.size() >= 4 && number_like(l[3]))
          out.confidence = to_float(l[3]);
        return true;
      }
      case ossia::val_type::MAP: {
        const auto& m = *v.target<ossia::value_map_type>();
        bool has_pos = false;
        for(const auto& [k, val] : m)
        {
          if(k == "position" || k == "pos" || k == "centroid" || k == "point")
          {
            track_record sub;
            if(parse_entity(val, third_conf, sub))
            {
              out.position = sub.position;
              has_pos = true;
            }
          }
          else if(k == "position_raw")
          {
            track_record sub;
            if(parse_entity(val, third_conf, sub))
              out.position_raw = sub.position;
          }
          else if(k == "velocity" || k == "vel")
          {
            track_record sub;
            if(parse_entity(val, false, sub))
              out.velocity = sub.position;
          }
          else if(k == "id")
          {
            if(number_like(val))
              out.id = int(to_float(val));
          }
          else if(k == "slot")
          {
            if(number_like(val))
              out.slot = int(to_float(val));
          }
          else if(k == "confidence" || k == "conf" || k == "score")
          {
            if(number_like(val))
              out.confidence = to_float(val);
          }
          else if(k == "age")
          {
            if(number_like(val))
              out.age = to_float(val);
          }
          else if(k == "time_since_seen")
          {
            if(number_like(val))
              out.time_since_seen = to_float(val);
          }
          else if(k == "creation_time")
          {
            if(number_like(val))
              out.creation_time = to_float(val);
          }
          else if(k == "state")
          {
            if(auto st = val.target<std::string>())
              out.state = *st;
          }
          else if(k == "provisional")
          {
            if(number_like(val))
              out.provisional = to_float(val) != 0.f;
          }
          else if(k == "reacquired")
          {
            if(number_like(val))
              out.reacquired = to_float(val) != 0.f;
          }
        }
        return has_pos;
      }
      default:
        return false;
    }
  }

  //! Turn the Entities inlet into track records. Anything the source did not
  //! supply is synthesised here - identity from list order, velocity and age
  //! by differencing against the previous frame - so that a bare list of
  //! positions drives expression exactly as a full record does.
  void parse_entities(double now)
  {
    m_recs.clear();
    const auto& in = inputs.tracks.value;
    if(in.empty())
    {
      m_synth.clear();
      return;
    }

    const bool third_conf = third_is_confidence();

    // One flat frame of plain numbers: [x, y(, z), x, y(, z), ...], strided by
    // the declared dimension.
    if(number_like(in[0]))
    {
      const std::size_t stride = inputs.coords.value == E2MCoords::ThreeD ? 3 : 2;
      for(std::size_t i = 0; i + stride <= in.size(); i += stride)
      {
        track_record r;
        r.position
            = {to_float(in[i]), to_float(in[i + 1]),
               stride == 3 ? to_float(in[i + 2]) : 0.f};
        r.confidence = 1.f;
        m_recs.push_back(std::move(r));
        if(m_recs.size() >= 512)
          break;
      }
    }
    else
    {
      for(const auto& v : in)
      {
        track_record r;
        r.confidence = 1.f;
        if(parse_entity(v, third_conf, r))
          m_recs.push_back(std::move(r));
        if(m_recs.size() >= 512)
          break;
      }
    }

    // Identity and the derived quantities. A source that supplies ids keeps
    // them; otherwise the index is the identity, which is why the inlet
    // documents that the order must be stable.
    ossia::flat_map<int, synth_state> next;
    for(std::size_t i = 0; i < m_recs.size(); i++)
    {
      auto& r = m_recs[i];
      if(r.id < 0)
        r.id = int(i);
      if(r.slot < 0)
        r.slot = r.id;
      if(r.state.empty())
        r.state = r.provisional ? "provisional" : "confirmed";
      if(!std::isfinite(r.position_raw.x))
        r.position_raw = r.position;

      auto& st = next[r.id];
      if(auto it = m_synth.find(r.id); it != m_synth.end())
      {
        const auto& prev = it->second;
        const double d = now - prev.time;
        // Only synthesise what the source did not provide: a full record
        // already carries a filtered velocity, and differencing it again
        // would fight the tracker's own smoothing.
        if(d > 1e-6 && r.velocity.x == 0.f && r.velocity.y == 0.f
           && r.velocity.z == 0.f)
        {
          r.velocity
              = {float((r.position.x - prev.pos.x) / d),
                 float((r.position.y - prev.pos.y) / d),
                 float((r.position.z - prev.pos.z) / d)};
        }
        // Carry the whole previous state, not just first_seen: the analysis
        // history and the filter memories the descriptors are built from live
        // here too, and rebuilding them every frame would reset every
        // derivative to its first-sample value.
        const double since = now - prev.time;
        st = prev;
        st.last_dt = since;
      }
      else
      {
        st.first_seen = now;
        st.last_dt = 0.;
        // A newly seen entity has no history to difference against, so it
        // enters with zero velocity rather than an arbitrary jump.
      }
      st.time = now;
      st.pos = r.position;
      if(r.age == 0.f)
        r.age = float(now - st.first_seen);
    }
    m_synth = std::move(next);
  }

  void ingest(double now)
  {
    const auto& recs = m_recs;
    const bool triggered = inputs.note_model.value == E2MNoteModel::Triggered;

    m_present.clear();
    for(const auto& r : recs)
    {
      if(r.id < 0)
        continue;
      m_present.push_back(r.id);

      // Pre-roll speed history, kept from the very first (provisional)
      // sighting so entry velocity measures the approach, not the arrival.
      auto& h = m_hist[r.id];
      h.push(now, speed_of(r));

      voice* v = find_voice(r.id);
      const bool eligible
          = inputs.trigger_on.value == E2MTriggerOn::FirstDetection || !r.provisional;

      if(v)
      {
        refresh_voice(*v, r, now);
      }
      else if(eligible)
      {
        if(triggered)
          try_start_triggered(r, now);
        else
          try_start_sustained(r, now);
      }
    }

    // Mark voices whose entity vanished from this update.
    for(auto& v : m_voices)
    {
      if(v.st == vstate::off)
        continue;
      bool present = false;
      for(auto id : m_present)
        if(id == v.id)
        {
          present = true;
          break;
        }
      if(!present && !v.missing)
      {
        v.missing = true;
        v.missing_since = now;
      }
    }

    // Prune stale speed history.
    if(m_hist.size() > 128)
      for(auto it = m_hist.begin(); it != m_hist.end();)
        it = (now - it->second.last_t > 2.) ? m_hist.erase(it) : std::next(it);
  }

  void refresh_voice(voice& v, const track_record& r, double now)
  {
    v.last_seen = now;
    v.missing = false;
    v.coasting = r.state == "coasting";
    v.entity_confirmed = v.entity_confirmed || !r.provisional;
    v.priority = entity_priority(r);

    const bool freeze = v.coasting && inputs.coast.value == E2MCoast::Freeze;
    if(!freeze)
    {
      const float rp = raw_pitch(r);
      v.pitch_target = quantize_pitch(rp, v);
      v.pressure_t = source_value(r, inputs.pressure_src.value);
      v.timbre_t = source_value(r, inputs.timbre_src.value);
    }
    if(v.coasting && inputs.coast.value == E2MCoast::Fade)
      v.pressure_t = 0.f;

    // Triggered + Retrigger tracking: a scale-step change fires a new note.
    //
    // This cannot test off_scheduled: try_start_triggered sets it on every
    // note, up front, because a triggered note has a fixed lifetime. Testing
    // it made the whole branch unreachable, so Retrigger behaved exactly like
    // Latched. What has to be excluded instead is a note that is already
    // waiting for its follow-up.
    if(inputs.note_model.value == E2MNoteModel::Triggered
       && inputs.pitch_tracking.value == E2MPitchTracking::Retrigger
       && v.st == vstate::sounding && !v.retrig_pending)
    {
      const int new_note = pitch_to_note(v.pitch_target);
      if(new_note != v.note && can_retrigger(v.id, now)
         && now - v.on_time >= inputs.min_note.value * 1e-3)
      {
        // Bring the fixed-lifetime note-off forward rather than calling
        // schedule_off, which returns early once off_scheduled is set and so
        // would leave the note sounding under the follow-up.
        v.off_raw_t = std::min(v.off_raw_t, now);
        v.off_target_q = -1.;
        v.retrig_pending = true;
        // The follow-up note starts once the off is emitted; remember intent.
        m_retrig_ids.push_back(v.id);
      }
    }
  }

  static int pitch_to_note(float target) noexcept
  {
    return std::clamp(int(std::lround(target)), 0, 127);
  }

  bool can_retrigger(int32_t id, double now) const noexcept
  {
    auto it = m_last_on.find(id);
    return it == m_last_on.end()
           || now - it->second >= inputs.retrig_lockout.value * 1e-3;
  }

  // Steal or deny. Returns a voice ready to be initialised, or nullptr.
  voice* obtain_voice(const track_record& r, double now)
  {
    if(auto* v = free_voice())
      return v;

    if(!inputs.allow_steal)
    {
      m_denied++;
      return nullptr;
    }

    // Never steal a confirmed entity's voice for a provisional newcomer.
    voice* victim = nullptr;
    for(auto& v : m_voices)
    {
      if(v.st == vstate::off)
        continue;
      if(r.provisional && v.entity_confirmed)
        continue;
      if(!victim || v.priority < victim->priority)
        victim = &v;
    }
    const float p_new = entity_priority(r);
    if(!victim || p_new <= victim->priority + inputs.steal_margin.value)
    {
      m_denied++;
      return nullptr;
    }

    // Emit the victim's off immediately (prio 0, same-timestamp offs sort
    // before ons) and recycle the slot.
    kill_voice(*victim, now);
    return victim;
  }

  void init_voice(voice& v, const track_record& r, double now)
  {
    v = voice{};
    v.id = r.id;
    v.st = vstate::pending_on;
    v.entity_confirmed = !r.provisional;
    v.last_seen = now;
    v.priority = entity_priority(r);
    v.on_raw_t = now;
    v.pitch_target = quantize_pitch(raw_pitch(r), v);
    v.pressure_t = source_value(r, inputs.pressure_src.value);
    v.timbre_t = source_value(r, inputs.timbre_src.value);
    v.pressure_v = v.pressure_t;
    v.timbre_v = v.timbre_t;
    v.velocity = compute_velocity(r, now);

    if(inputs.quant_mode.value != E2MQuantTargets::Off)
      v.on_target_q = -2.; // to be filled from the tick's musical position
    else
      v.on_target_q = -1.; // immediate
  }

  void try_start_sustained(const track_record& r, double now)
  {
    // While panic or MPE-configuration messages are still queued, starting a
    // note would let its note-on race a pending CC 123 and be silently
    // killed. Wait; the entity is retried on the next track update.
    if(!m_timed.empty())
      return;
    if(!can_retrigger(r.id, now))
      return;
    voice* v = obtain_voice(r, now);
    if(!v)
      return;
    init_voice(*v, r, now);
    const int c = alloc_channel(now);
    if(c < 0)
    {
      // No channel free (all reserved / in use): deny rather than share.
      v->st = vstate::off;
      m_denied++;
      return;
    }
    v->channel = int8_t(c);
  }

  void try_start_triggered(const track_record& r, double now)
  {
    if(!m_timed.empty())
      return;
    if(!can_retrigger(r.id, now))
      return;
    voice* v = obtain_voice(r, now);
    if(!v)
      return;
    init_voice(*v, r, now);
    const int c = alloc_channel(now);
    if(c < 0)
    {
      v->st = vstate::off;
      m_denied++;
      return;
    }
    v->channel = int8_t(c);
    // Triggered notes get a fixed lifetime, scheduled up front.
    v->off_scheduled = true;
    v->off_raw_t = now + inputs.trigger_duration.value * 1e-3;
    v->off_target_q = -1.;
  }

  uint8_t compute_velocity(const track_record& r, double now) const
  {
    const int lo = std::min(inputs.vel_lo.value, inputs.vel_hi.value);
    const int hi = std::max(inputs.vel_lo.value, inputs.vel_hi.value);
    switch(inputs.vel_source.value)
    {
      case E2MVelSource::Fixed:
        return uint8_t(std::clamp(inputs.vel_fixed.value, 1, 127));
      case E2MVelSource::Confidence: {
        const float n = std::clamp(r.confidence, 0.f, 1.f);
        return uint8_t(std::clamp(int(std::lround(lo + n * (hi - lo))), 1, 127));
      }
      case E2MVelSource::EntrySpeed: {
        // Peak speed over the pre-roll window before the note starts:
        // measuring at the instant of confirmation catches the tracker's
        // smoother still converging and yields the same middling value for
        // every entrance.
        float peak = 0.f;
        auto it = m_hist.find(r.id);
        if(it != m_hist.end())
          peak = it->second.peak_since(now - inputs.preroll.value * 1e-3);
        else
          peak = speed_of(r);
        const float ref = std::max(inputs.speed_ref.value, 1e-3f);
        const float n = std::clamp(peak / ref, 0.f, 1.f);
        return uint8_t(std::clamp(int(std::lround(lo + n * (hi - lo))), 1, 127));
      }
    }
    return 100;
  }

  // ------------------------------------------------------------- lifecycle
  void lifecycle(double now, const halp::tick_musical& tk)
  {
    const bool sustained = inputs.note_model.value == E2MNoteModel::Sustained;
    const double grace = inputs.lost_grace.value * 1e-3;
    const double wd = inputs.watchdog.value * 1e-3;
    const double maxn = inputs.max_note.value * 1e-3;

    for(auto& v : m_voices)
    {
      if(v.st == vstate::off)
        continue;

      // Entity gone past the grace window -> release.
      if(sustained && v.missing && !v.off_scheduled && now - v.missing_since >= grace)
        schedule_off(v, now, true);

      // A pending note whose entity vanished never sounds.
      if(v.st == vstate::pending_on && v.missing && now - v.missing_since >= grace)
      {
        free_channel(v.channel, now);
        v.st = vstate::off;
        continue;
      }

      // Watchdog: no data at all for too long, independent of the tracker's
      // own lifecycle. Catches a dead tracker / unplugged camera.
      if(wd > 0. && v.st == vstate::sounding && !v.off_scheduled
         && now - v.last_seen >= wd)
        schedule_off(v, now, false);

      // Hard note-length ceiling.
      if(maxn > 0. && v.st == vstate::sounding && !v.off_scheduled
         && now - v.on_time >= maxn)
        schedule_off(v, now, true);
    }
  }

  void schedule_off(voice& v, double now, bool allow_quantise)
  {
    if(v.st == vstate::pending_on)
    {
      // Never sounded: cancel silently.
      free_channel(v.channel, now);
      v.st = vstate::off;
      return;
    }
    if(v.off_scheduled)
      return;
    v.off_scheduled = true;
    v.off_raw_t = now;
    v.off_target_q
        = (allow_quantise
           && inputs.quant_mode.value == E2MQuantTargets::OnsetsAndOffsets)
              ? -2.
              : -1.;
  }

  // Immediate, unquantised kill (stealing, panic): off at current tick start.
  void kill_voice(voice& v, double now)
  {
    if(v.on_wire)
      push_note_off(v, 0);
    free_channel(v.channel, now);
    v.st = vstate::off;
  }

  // ------------------------------------------------------------- rhythm
  double grid_quarters() const noexcept
  {
    switch(inputs.grid.value)
    {
      case E2MGrid::Whole:
        return 4.;
      case E2MGrid::Half:
        return 2.;
      case E2MGrid::Quarter:
        return 1.;
      case E2MGrid::Eighth:
        return 0.5;
      case E2MGrid::Sixteenth:
        return 0.25;
      case E2MGrid::EighthTriplet:
        return 1. / 3.;
      case E2MGrid::SixteenthTriplet:
        return 1. / 6.;
    }
    return 0.5;
  }

  // Compute the quantised emission point in quarters for an event raised at
  // musical position raw_q. Nearest grid point, lerped by strength.
  double quantise_target(double raw_q) const noexcept
  {
    const double g = grid_quarters();
    const double grid_q = std::round(raw_q / g) * g;
    return raw_q + inputs.strength.value * (grid_q - raw_q);
  }

  // If target_q falls inside this tick, return the sample offset; -1 if it
  // is in the future; 0 (emit now) if it is already past.
  static int64_t
  quarters_to_frame(double target_q, const halp::tick_musical& tk) noexcept
  {
    const double a = tk.start_position_in_quarters;
    const double b = tk.end_position_in_quarters;
    if(target_q <= a)
      return 0;
    if(b <= a)
      return -1; // transport not advancing
    if(target_q >= b)
      return -1;
    return int64_t((target_q - a) / (b - a) * std::max(tk.frames, 1));
  }

  void emit_pending_notes(double now, double dt, int frames, const halp::tick_musical& tk)
  {
    const double max_hold = inputs.max_hold.value * 1e-3;
    const double min_note = inputs.min_note.value * 1e-3;

    for(auto& v : m_voices)
    {
      if(v.st == vstate::pending_on)
      {
        // First tick after the trigger: resolve the musical target.
        if(v.on_target_q == -2.)
          v.on_target_q = quantise_target(tk.start_position_in_quarters);

        int64_t off = -1;
        bool fire = false;
        if(v.on_target_q < 0.)
        {
          fire = true;
          off = 0;
        }
        else
        {
          const int64_t f = quarters_to_frame(v.on_target_q, tk);
          if(f >= 0)
          {
            fire = true;
            off = f;
          }
          else if(now - v.on_raw_t >= max_hold)
          {
            // Beat tracker dropout / transport stall: play anyway.
            fire = true;
            off = 0;
          }
        }
        if(fire)
          emit_note_on(v, now, off, frames);
      }

      if(v.st == vstate::sounding && v.off_scheduled)
      {
        // Resolve the off's musical target on the tick after scheduling.
        if(v.off_target_q == -2.)
          v.off_target_q = quantise_target(tk.start_position_in_quarters);

        // Earliest legal off: on_time + min_note (also guards quantised
        // off landing before its on).
        const double earliest = v.on_time + min_note;

        int64_t off = -1;
        bool fire = false;
        if(v.off_target_q >= 0.)
        {
          const int64_t f = quarters_to_frame(v.off_target_q, tk);
          if(f >= 0)
          {
            fire = true;
            off = f;
          }
          else if(now - v.off_raw_t >= max_hold)
          {
            fire = true;
            off = 0;
          }
        }
        else if(v.off_raw_t <= now + dt)
        {
          fire = true;
          off = std::clamp<int64_t>(
              int64_t((v.off_raw_t - now) / dt * frames), 0, frames - 1);
        }

        if(fire)
        {
          const double t_emit = now + double(off) / m_rate;
          if(t_emit < earliest)
          {
            // Too early: push the off to the earliest legal time, unquantised.
            if(earliest <= now + dt)
            {
              fire = true;
              off = std::clamp<int64_t>(
                  int64_t((earliest - now) / dt * frames), 0, frames - 1);
            }
            else
              fire = false;
          }
          if(fire)
            emit_note_off(v, off, now + double(off) / m_rate);
        }
      }
    }

    // Retrigger follow-ups (triggered mode): start the new note for ids
    // whose old note was just released.
    if(!m_retrig_ids.empty())
    {
      for(auto id : m_retrig_ids)
      {
        for(const auto& r : m_recs)
          if(r.id == id)
          {
            try_start_triggered(r, now);
            break;
          }
      }
      m_retrig_ids.clear();
    }
  }

  void emit_note_on(voice& v, double tick_start, int64_t frame_off, int frames)
  {
    const int note = pitch_to_note(v.pitch_target);

    // Invariant 2: never a second note-on for a held (channel, pitch).
    // Structural in MPE / channel-per-entity (one note per channel); it
    // bites in single-channel mode when two entities converge on the same
    // quantised pitch - which happens constantly, because bodies cluster.
    for(const auto& o : m_voices)
      if(&o != &v && o.on_wire && o.channel == v.channel && o.note == note)
      {
        // Hold the voice pending; it will fire when the pitch diverges or
        // the other note releases.
        if(!v.note_suppressed)
        {
          v.note_suppressed = true;
          m_denied++;
        }
        return;
      }

    // Invariant 4, intra-tick form: the (channel, pitch) may still be
    // occupied ON THE WIRE even though no voice holds it - a note-off
    // already pushed this tick can carry a later mid-buffer timestamp
    // (triggered-mode duration, min-note delay, quantised off). Emitting
    // the new on earlier in the buffer would invert their order for the
    // receiver and hang the note. Start the new note at the off's
    // timestamp instead: at equal timestamps offs sort before ons.
    for(const auto& m : m_msgs)
      if(m.n == 3 && (m.bytes[0] & 0xF0) == 0x80
         && (m.bytes[0] & 0x0F) == (uint8_t(v.channel) & 0x0F)
         && m.bytes[1] == uint8_t(note) && m.ts > frame_off)
        frame_off = m.ts;
    frame_off = std::clamp<int64_t>(frame_off, 0, frames - 1);
    const double t = tick_start + double(frame_off) / m_rate;

    v.note = uint8_t(note);
    v.note_suppressed = false;
    v.bend_semis = v.pitch_target - float(v.note);

    const uint8_t ch = uint8_t(v.channel);
    const bool member_dims = !is_single();

    if(member_dims)
    {
      // Set the expressive state BEFORE the note-on so it starts in tune.
      if(inputs.pitch_tracking.value == E2MPitchTracking::ContinuousBend)
      {
        push_bend(v, frame_off, /*prio*/ 1);
      }
      else
      {
        v.bend_semis = 0.f;
        push_bend_value(ch, 8192, frame_off, 1);
        v.last_bend = 8192;
      }
      if(inputs.pressure_src.value != E2MSource::None)
        push_pressure(v, frame_off, 1);
      if(inputs.timbre_src.value != E2MSource::None)
        push_timbre(v, frame_off, 1);
    }

    push(frame_off, 1, uint8_t(0x90 | ch), v.note, v.velocity);
    m_outstanding++;
    v.on_wire = true;
    v.st = vstate::sounding;
    v.on_time = t;
    v.expr_acc = 0.;
    m_touched[ch] = true;
    m_last_on[v.id] = t;
    prune_last_on(t);
  }

  void emit_note_off(voice& v, int64_t frame_off, double t)
  {
    if(v.on_wire)
    {
      push(frame_off, 0, uint8_t(0x80 | uint8_t(v.channel)), v.note, 64);
      m_outstanding--;
      v.on_wire = false;
    }
    free_channel(v.channel, t);
    v.st = vstate::off;
  }

  void push_note_off(voice& v, int64_t frame_off)
  {
    if(v.on_wire)
    {
      push(frame_off, 0, uint8_t(0x80 | uint8_t(v.channel)), v.note, 64);
      m_outstanding--;
      v.on_wire = false;
    }
  }

  void prune_last_on(double now)
  {
    if(m_last_on.size() > 256)
      for(auto it = m_last_on.begin(); it != m_last_on.end();)
        it = (now - it->second > 10.) ? m_last_on.erase(it) : std::next(it);
  }

  // ------------------------------------------------------------- expression
  void update_expression(double now, double dt, int frames)
  {
    const bool single = is_single();
    const double period = 1. / std::max(inputs.expr_rate.value, 1.f);
    const float db = inputs.deadband.value;
    const double tau_rise = std::max(inputs.rise.value, 0.f) * 1e-3;
    const double tau_fall = std::max(inputs.fall.value, 0.f) * 1e-3;
    const double tau_glide = std::max(inputs.glide.value, 0.f) * 1e-3;
    const bool cont
        = inputs.pitch_tracking.value == E2MPitchTracking::ContinuousBend && !single;

    // Round-robin so no entity starves when the budget is tight.
    const int n = k_max_voices;
    const int start = m_rr++ % n;
    for(int k = 0; k < n; k++)
    {
      voice& v = m_voices[(start + k) % n];
      if(v.st != vstate::sounding || !v.on_wire)
        continue;

      // Slew the mapping state every tick regardless of emission rate.
      const float bend_target = cont ? (v.pitch_target - float(v.note)) : 0.f;
      if(tau_glide <= 0.)
        v.bend_semis = bend_target;
      else
        v.bend_semis += (bend_target - v.bend_semis)
                        * float(ossia::lag_alpha(tau_glide, dt));
      smooth_asym(v.pressure_v, v.pressure_t, tau_rise, tau_fall, dt);
      smooth_asym(v.timbre_v, v.timbre_t, tau_rise, tau_fall, dt);

      v.expr_acc += dt;
      if(v.expr_acc < period)
        continue;

      v.expr_acc = 0.;

      if(cont)
      {
        const int bend = bend_to_14bit(v);
        if(v.last_bend < 0 || std::abs(bend - v.last_bend) >= int(db * 16384.f))
          if(bend != v.last_bend)
          {
            push_bend_value(uint8_t(v.channel), bend, 0, 2);
            v.last_bend = bend;
          }
      }
      if(inputs.pressure_src.value != E2MSource::None)
      {
        const int p = std::clamp(int(std::lround(v.pressure_v * 127.f)), 0, 127);
        if(v.last_pressure < 0
           || std::abs(p - v.last_pressure) >= std::max(1, int(db * 127.f)))
          if(p != v.last_pressure)
          {
            if(single)
              push(0, 2, uint8_t(0xA0 | uint8_t(v.channel)), v.note, uint8_t(p));
            else
              push2(0, 2, uint8_t(0xD0 | uint8_t(v.channel)), uint8_t(p));
            v.last_pressure = p;
          }
      }
      if(!single && inputs.timbre_src.value != E2MSource::None)
      {
        const int t = std::clamp(int(std::lround(v.timbre_v * 127.f)), 0, 127);
        if(v.last_timbre < 0
           || std::abs(t - v.last_timbre) >= std::max(1, int(db * 127.f)))
          if(t != v.last_timbre)
          {
            push(
                0, 2, uint8_t(0xB0 | uint8_t(v.channel)),
                uint8_t(std::clamp(inputs.timbre_cc.value, 0, 127)), uint8_t(t));
            v.last_timbre = t;
          }
      }
    }
  }

  static void
  smooth_asym(float& state, float target, double tau_rise, double tau_fall, double dt)
  {
    const double tau = target > state ? tau_rise : tau_fall;
    if(tau <= 0.)
      state = target;
    else
      state += (target - state) * float(ossia::lag_alpha(tau, dt));
  }

  int bend_to_14bit(const voice& v) const noexcept
  {
    const float range = float(std::max(inputs.bend_range.value, 1));
    const float n = std::clamp(v.bend_semis / range, -1.f, 1.f);
    return std::clamp(8192 + int(std::lround(n * 8191.f)), 0, 16383);
  }

  // ------------------------------------------------------------- emission
  void push(int64_t ts, uint8_t prio, uint8_t b0, uint8_t b1, uint8_t b2)
  {
    m_msgs.push_back({ts, prio, 3, {b0, b1, b2}});
  }
  void push2(int64_t ts, uint8_t prio, uint8_t b0, uint8_t b1)
  {
    m_msgs.push_back({ts, prio, 2, {b0, b1, 0}});
  }

  void push_bend(voice& v, int64_t ts, uint8_t prio)
  {
    const int bend = bend_to_14bit(v);
    push_bend_value(uint8_t(v.channel), bend, ts, prio);
    v.last_bend = bend;
  }
  void push_bend_value(uint8_t ch, int bend, int64_t ts, uint8_t prio)
  {
    push(ts, prio, uint8_t(0xE0 | ch), uint8_t(bend & 0x7F), uint8_t((bend >> 7) & 0x7F));
  }
  void push_pressure(voice& v, int64_t ts, uint8_t prio)
  {
    const int p = std::clamp(int(std::lround(v.pressure_v * 127.f)), 0, 127);
    push2(ts, prio, uint8_t(0xD0 | uint8_t(v.channel)), uint8_t(p));
    v.last_pressure = p;
  }
  void push_timbre(voice& v, int64_t ts, uint8_t prio)
  {
    const int t = std::clamp(int(std::lround(v.timbre_v * 127.f)), 0, 127);
    push(
        ts, prio, uint8_t(0xB0 | uint8_t(v.channel)),
        uint8_t(std::clamp(inputs.timbre_cc.value, 0, 127)), uint8_t(t));
    v.last_timbre = t;
  }

  // Timed queue: panic + configuration, spaced spacing ms apart, drained
  // across as many ticks as needed.
  void queue_timed(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t n = 3)
  {
    m_timed_tail = std::max(m_timed_tail, m_now);
    m_timed.push_back({m_timed_tail, n, {b0, b1, b2}});
    m_timed_tail += std::max(inputs.spacing.value, 0.f) * 1e-3;
  }

  void flush_timed(double now, double dt, int frames)
  {
    if(m_timed.empty())
      return;
    std::size_t taken = 0;
    for(const auto& m : m_timed)
    {
      if(m.t >= now + dt)
        break;
      const int64_t off = std::clamp<int64_t>(
          int64_t((m.t - now) / dt * frames), 0, frames - 1);
      // Panic/config are class 0/3; use 0 so note traffic cannot precede a
      // pending panic.
      m_msgs.push_back({off, 0, m.n, {m.bytes[0], m.bytes[1], m.bytes[2]}});
      taken++;
    }
    if(taken)
      m_timed.erase(m_timed.begin(), m_timed.begin() + taken);
  }

  void queue_rpn(uint8_t ch, uint8_t rpn_msb, uint8_t rpn_lsb, uint8_t value)
  {
    queue_timed(uint8_t(0xB0 | ch), 101, rpn_msb);
    queue_timed(uint8_t(0xB0 | ch), 100, rpn_lsb);
    queue_timed(uint8_t(0xB0 | ch), 6, value);
    queue_timed(uint8_t(0xB0 | ch), 101, 127);
    queue_timed(uint8_t(0xB0 | ch), 100, 127);
  }

  void queue_mpe_config()
  {
    const uint8_t master = uint8_t(master_channel());
    const uint8_t members = uint8_t(member_count());
    // RPN 6: MPE configuration - number of member channels.
    queue_rpn(master, 0, 6, members);
    // RPN 0: pitch bend sensitivity, per member channel.
    const uint8_t range = uint8_t(std::clamp(inputs.bend_range.value, 1, 96));
    for(int i = 0; i < members; i++)
      queue_rpn(uint8_t(member_channel(i)), 0, 0, range);
  }

  // Panic through the normal (timestamped, spaced) path.
  void queue_panic()
  {
    const double now = m_now;
    // 1. Explicit note-off per held voice - CC 123 alone is widely ignored.
    for(auto& v : m_voices)
    {
      if(v.st == vstate::off)
        continue;
      if(v.on_wire)
      {
        queue_timed(uint8_t(0x80 | uint8_t(v.channel)), v.note, 64);
        m_outstanding--;
        v.on_wire = false;
      }
      free_channel(v.channel, now);
      v.st = vstate::off;
    }
    // 2. All sound off / all notes off / reset controllers per touched
    //    channel; 3. reset the expressive state.
    for(int c = 0; c < 16; c++)
    {
      if(!m_touched[c])
        continue;
      queue_timed(uint8_t(0xB0 | c), 120, 0);
      queue_timed(uint8_t(0xB0 | c), 123, 0);
      queue_timed(uint8_t(0xB0 | c), 121, 0);
      queue_timed(uint8_t(0xE0 | c), 0x00, 0x40);            // bend centre
      queue_timed(uint8_t(0xD0 | c), 0, 0, 2);               // pressure 0
      queue_timed(uint8_t(0xB0 | c), 74, 64);                // timbre centre
    }
  }

  // Stop-time panic: ticks have ended, so push straight to the MIDI device
  // protocol if the output is bound to one. Ordering is preserved; the 1 ms
  // spacing is not achievable here without blocking the caller. Every byte
  // also lands in last_direct_panic so tests (and post-mortems) can see
  // exactly what went out.
  void panic_direct()
  {
    last_direct_panic.clear();
    auto* proto = midi_out.load();
    for(auto& v : m_voices)
    {
      if(v.st == vstate::off)
        continue;
      if(v.on_wire)
      {
        send_direct(proto, uint8_t(0x80 | uint8_t(v.channel)), v.note, 64);
        m_outstanding--;
        m_wire[uint8_t(v.channel) & 0x0F].reset(v.note);
      }
      v.on_wire = false;
      v.st = vstate::off;
    }
    for(int c = 0; c < 16; c++)
    {
      if(!m_touched[c])
        continue;
      send_direct(proto, uint8_t(0xB0 | c), 120, 0);
      send_direct(proto, uint8_t(0xB0 | c), 123, 0);
      send_direct(proto, uint8_t(0xB0 | c), 121, 0);
      send_direct(proto, uint8_t(0xE0 | c), 0x00, 0x40);
    }
  }

  void send_direct(
      ossia::net::midi::midi_protocol* proto, uint8_t b0, uint8_t b1, uint8_t b2)
  {
    if(last_direct_panic.size() < 4096)
      last_direct_panic.push_back({b0, b1, b2});
    if(proto)
      proto->push_value(libremidi::message{libremidi::midi_bytes{b0, b1, b2}, 0});
  }

  void resolve_protocol()
  {
    if(outputs.midi.ossia_node)
    {
      auto& proto = outputs.midi.ossia_node->get_device().get_protocol();
      if(auto mp = dynamic_cast<ossia::net::midi::midi_protocol*>(&proto))
        midi_out.store(mp);
    }
  }

  void hard_reset()
  {
    for(auto& v : m_voices)
    {
      if(v.on_wire)
        m_outstanding--;
      v = voice{};
    }
    for(auto& c : m_chans)
      c = chan_info{};
    m_timed.clear();
    m_timed_tail = 0.;
    m_retrig_ids.clear();
    // Fresh wire model for the next run. m_wire_err deliberately survives:
    // a violation is a bug, and resetting it would let tests miss it.
    for(auto& w : m_wire)
      w.reset();
  }

  uint64_t config_signature() const noexcept
  {
    return uint64_t(inputs.output_mode.value)
           | (uint64_t(inputs.mpe_zone.value) << 4)
           | (uint64_t(std::clamp(inputs.member_channels.value, 1, 16)) << 8)
           | (uint64_t(std::clamp(inputs.bend_range.value, 1, 96)) << 16)
           | (uint64_t(std::clamp(inputs.single_channel.value, 1, 16)) << 24)
           | (uint64_t(1) << 32); // never 0, so the first tick configures
  }

private:
  std::array<voice, k_max_voices> m_voices{};
  std::array<chan_info, 16> m_chans{};
  std::array<bool, 16> m_touched{};
  // Receiver-side wire model for invariants 2/4: which (channel, pitch) is
  // sounding, updated by replaying each tick's sorted output. m_wire_err is
  // sticky: once a violation is seen it stays reported - it is a bug.
  std::array<std::bitset<128>, 16> m_wire{};
  std::string m_wire_err;
  std::vector<out_msg> m_msgs;
  std::vector<timed_msg> m_timed;
  std::vector<int32_t> m_present;
  std::vector<int32_t> m_retrig_ids;
  ossia::flat_map<int32_t, speed_hist> m_hist;
  ossia::flat_map<int32_t, double> m_last_on;

  double m_rate = 48000.;
  double m_now = 0.;
  double m_timed_tail = 0.;
  int m_denied = 0;
  int m_outstanding = 0;
  unsigned m_rr = 0;
  uint64_t m_config_sig = 0;
  bool m_do_config = false;
  bool m_started = false;

public:
  // ------------------------------------------------------------- UI
  struct ui
  {
    halp_meta(name, "Entity To MIDI")
    halp_meta(layout, halp::layouts::tabs)
    halp_meta(background, halp::colors::background_mid)

    struct
    {
      halp_meta(name, "Output")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Mode")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::output_mode> output_mode;
        halp::item<&ins::single_channel> single_channel;
      } mode;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "MPE")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::mpe_zone> mpe_zone;
        halp::item<&ins::member_channels> member_channels;
        halp::item<&ins::bend_range> bend_range;
        halp::item<&ins::send_config> send_config;
      } mpe;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Channels")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::channel_reuse> channel_reuse;
        halp::item<&ins::release_reserve> release_reserve;
      } chans;
    } output_tab;

    struct
    {
      halp_meta(name, "Voice")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Model")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::note_model> note_model;
        halp::item<&ins::trigger_on> trigger_on;
        halp::item<&ins::trigger_duration> trigger_duration;
        halp::item<&ins::coast> coast;
      } model;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Allocation")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::max_voices> max_voices;
        halp::item<&ins::priority> priority;
        halp::item<&ins::allow_steal> allow_steal;
        halp::item<&ins::steal_margin> steal_margin;
      } alloc;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Timing")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::lost_grace> lost_grace;
        halp::item<&ins::min_note> min_note;
        halp::item<&ins::max_note> max_note;
        halp::item<&ins::retrig_lockout> retrig_lockout;
        halp::item<&ins::watchdog> watchdog;
      } timing;
    } voice_tab;

    struct
    {
      halp_meta(name, "Pitch")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Source")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::coords> coords;
        halp::item<&ins::pitch_axis> pitch_axis;
        halp::item<&ins::pitch_invert> pitch_invert;
        halp::item<&ins::in_lo> in_lo;
        halp::item<&ins::in_hi> in_hi;
      } source;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Range")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::pitch_lo> pitch_lo;
        halp::item<&ins::pitch_hi> pitch_hi;
        halp::item<&ins::pitch_tracking> pitch_tracking;
        halp::item<&ins::glide> glide;
      } range;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Scale")
        halp_meta(layout, halp::layouts::vbox)
        // note: the member must not be called `scale` - the layout engine
        // reads a `scale` member as a QGraphicsItem transform property.
        halp::item<&ins::scale> scale_sel;
        halp::item<&ins::root> root;
        halp::item<&ins::quant_hyst> quant_hyst;
      } scl;
    } pitch_tab;

    struct
    {
      halp_meta(name, "Expression")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Velocity")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::vel_source> vel_source;
        halp::item<&ins::vel_fixed> vel_fixed;
        halp::item<&ins::vel_lo> vel_lo;
        halp::item<&ins::vel_hi> vel_hi;
        halp::item<&ins::preroll> preroll;
      } vel;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Dimensions")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::pressure_src> pressure_src;
        halp::item<&ins::timbre_src> timbre_src;
        halp::item<&ins::timbre_cc> timbre_cc;
      } dims;

      halp::spacing sp_an{.width = 12, .height = 1};

      // The full-scale value for each family of descriptor. Grouped together
      // because they are only ever touched when a mapping pins at 0 or 127,
      // and then it is obvious which one is to blame.
      struct
      {
        halp_meta(name, "Analysis range")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::speed_ref> speed_ref;
        halp::item<&ins::accel_ref> accel_ref;
        halp::item<&ins::jerk_ref> jerk_ref;
        halp::item<&ins::turn_ref> turn_ref;
        halp::item<&ins::neighbour_range> neighbour_range;
        halp::item<&ins::age_ref> age_ref;
        halp::item<&ins::desc_smooth> desc_smooth;
      } analysis;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Smoothing")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::rise> rise;
        halp::item<&ins::fall> fall;
        halp::item<&ins::expr_rate> expr_rate;
        halp::item<&ins::deadband> deadband;
      } smooth;
    } expr_tab;

    struct
    {
      halp_meta(name, "Rhythm")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Quantize")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::quant_mode> quant_mode;
        halp::item<&ins::grid> grid;
      } quant;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Feel")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::strength> strength;
        halp::item<&ins::max_hold> max_hold;
      } feel;
    } rhythm_tab;

    struct
    {
      halp_meta(name, "Safety")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Panic")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::panic> panic;
        halp::item<&ins::panic_on_stop> panic_on_stop;
        halp::item<&ins::spacing> spacing;
      } pan;
    } safety_tab;
  };
};

}
