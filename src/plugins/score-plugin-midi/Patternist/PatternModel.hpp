#pragma once
#include <Process/Process.hpp>

#include <Patternist/PatternMetadata.hpp>

#include <score_plugin_midi_export.h>

#include <verdigris>

namespace Process
{
class ControlInlet;
class ValueOutlet;
}
namespace Patternist
{

enum class Note : uint8_t
{
  Rest,
  Note,
  Legato
};
}

Q_DECLARE_METATYPE(Patternist::Note)
W_REGISTER_ARGTYPE(Patternist::Note)

namespace Patternist
{

//! When a step plays, beyond its chance.
enum class Condition : uint8_t
{
  Always,
  //! Only while the Fill input is on, or only while it is off.
  Fill,
  NotFill,
  //! Only on the lane's first pass since the process started, or never on it.
  First,
  NotFirst,
  //! On pass `a` of every `b` passes of the lane.
  Cycle
};

//! What a step does besides being a note, a rest or a tie. The defaults play the
//! step as written.
struct StepParams
{
  //! 0 - 1, scaled by the lane's velocity and the Velocity input.
  float velocity{1.f};
  //! 0 - 1, scaled by the Density input.
  float chance{1.f};
  //! -0.5 - 0.5 of a step: early or late.
  float offset{0.f};
  //! 0 - 1 of a step; a tie, or 1, holds the note until the next step.
  float gate{1.f};
  //! Notes played within the step, 1 - 8.
  uint8_t ratchet{1};
  //! Semitones added to the lane's note.
  int8_t pitch{0};
  Condition condition{Condition::Always};
  uint8_t cycleA{1};
  uint8_t cycleB{2};

  bool operator==(const StepParams&) const noexcept = default;
  bool isDefault() const noexcept { return *this == StepParams{}; }
};

//! How the Euclidean rhythm of a lane combines with its written steps.
enum class EuclidMode : uint8_t
{
  //! The rhythm replaces the written notes; the step parameters still apply.
  Replace,
  //! Notes where either the rhythm or the written steps have one.
  Add,
  //! Written notes only where the rhythm has a hit.
  Mask
};

struct Euclid
{
  bool enabled{};
  int pulses{4};
  int rotation{0};
  EuclidMode mode{EuclidMode::Replace};

  bool operator==(const Euclid&) const noexcept = default;
};

//! Whether step `i` of `steps` is a hit when `pulses` of them are spread as
//! evenly as possible, the rhythm turned `rotation` steps later.
inline bool euclidHit(int i, int steps, int pulses, int rotation) noexcept
{
  if(steps <= 0 || pulses <= 0)
    return false;
  if(pulses >= steps)
    return true;
  const int j = (((i - rotation) % steps) + steps) % steps;
  return (j * pulses) % steps < pulses;
}

struct Lane
{
  std::vector<Patternist::Note> pattern;
  //! A MIDI note, or 255 for the accent lane and 254 for the slide lane.
  uint8_t note{};
  bool mute{};
  bool solo{};
  //! Steps before the lane loops; 0 follows the pattern.
  int length{};
  //! Steps per whole note; 0 follows the pattern.
  int division{};
  //! 0 - 1.
  float velocity{100.f / 127.f};
  Euclid euclid;
  //! Same size as `pattern`.
  std::vector<StepParams> params;

  bool operator==(const Lane& other) const noexcept = default;
};

struct Pattern
{
  static const constexpr int maxLength = 128;

  int length{16};
  int division{16};
  std::vector<Lane> lanes;

  bool operator==(const Pattern& other) const noexcept
  {
    return length == other.length && division == other.division && lanes == other.lanes;
  }
};

//! Brings a pattern into the range the editor and the engine rely on: every lane
//! has at least `length` steps. Steps past `length` are kept, so that shortening
//! a pattern then lengthening it again gives the same steps back.
SCORE_PLUGIN_MIDI_EXPORT void normalize(Pattern& p) noexcept;

inline bool anySolo(const Pattern& p) noexcept
{
  for(const Lane& l : p.lanes)
    if(l.solo)
      return true;
  return false;
}

inline int laneLength(const Pattern& p, const Lane& l) noexcept
{
  return l.length > 0 ? l.length : p.length;
}

inline int laneDivision(const Pattern& p, const Lane& l) noexcept
{
  return l.division > 0 ? l.division : p.division;
}

//! What a lane's step is once its Euclidean rhythm is applied, before mute,
//! solo, chance and conditions.
inline Note writtenNote(const Pattern& p, const Lane& l, int step) noexcept
{
  const Note written
      = (step >= 0 && step < int(l.pattern.size())) ? l.pattern[step] : Note::Rest;
  if(!l.euclid.enabled)
    return written;

  const bool hit = euclidHit(step, laneLength(p, l), l.euclid.pulses, l.euclid.rotation);
  switch(l.euclid.mode)
  {
    case EuclidMode::Replace:
      return hit ? Note::Note : Note::Rest;
    case EuclidMode::Add:
      return written != Note::Rest ? written : (hit ? Note::Note : Note::Rest);
    case EuclidMode::Mask:
      return hit ? written : Note::Rest;
  }
  return written;
}

inline const StepParams& stepParams(const Lane& l, int step) noexcept
{
  static const StepParams defaults;
  return (step >= 0 && step < int(l.params.size())) ? l.params[step] : defaults;
}

//! What a lane plays at a step, once its rhythm, mute and solo are applied.
inline Note audibleNote(const Pattern& p, const Lane& l, int step, bool anySolo) noexcept
{
  if(l.mute || (anySolo && !l.solo))
    return Note::Rest;
  return writtenNote(p, l, step);
}

//! Writes the lane's Euclidean rhythm into its steps and turns it off, so that
//! it can be edited by hand.
SCORE_PLUGIN_MIDI_EXPORT void printEuclid(const Pattern& p, Lane& l) noexcept;

//! How the lane headers name their note.
enum class LaneLabels : uint8_t
{
  NameAndNumber,
  Name,
  Number,
  DrumName
};
}

Q_DECLARE_METATYPE(Patternist::LaneLabels)
W_REGISTER_ARGTYPE(Patternist::LaneLabels)

namespace Patternist
{

class SCORE_PLUGIN_MIDI_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  W_OBJECT(ProcessModel)

public:
  //! Upper bound on the pattern list, and the pattern port's range.
  static const constexpr int maxPatterns = 128;

  PROCESS_METADATA_IMPL(Patternist::ProcessModel)
  explicit ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);
  explicit ProcessModel(
      const TimeVal& duration, const QString& custom,
      const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  ~ProcessModel() override;

  void setChannel(int n);
  int channel() const noexcept;

  void setCurrentPattern(int n);
  int currentPattern() const noexcept;

  void setLaneLabels(LaneLabels l);
  LaneLabels laneLabels() const noexcept;

  //! For each lane of the playing pattern, how many steps it played before the
  //! one it is on, -1 for none.
  const std::vector<int64_t>& execSteps() const noexcept { return m_execSteps; }
  void setExecSteps(std::vector<int64_t> s) { m_execSteps = std::move(s); }

  void setPattern(int n, Pattern p);
  void setPatterns(const std::vector<Pattern>& n);
  const std::vector<Pattern>& patterns() const noexcept;

  std::unique_ptr<Process::ControlInlet> patternSelect;
  std::unique_ptr<Process::ControlInlet> switchQuantification;
  //! Steps with the Fill / Not fill condition play while it is on / off.
  std::unique_ptr<Process::ControlInlet> fill;
  //! Scales the chance of every step.
  std::unique_ptr<Process::ControlInlet> density;
  //! Scales the velocity of every step.
  std::unique_ptr<Process::ControlInlet> velocity;
  //! Delays every other step, by up to half a step.
  std::unique_ptr<Process::ControlInlet> swing;
  std::unique_ptr<Process::MidiOutlet> outlet;
  std::unique_ptr<Process::Outlet> accent;
  std::unique_ptr<Process::Outlet> slide;

public:
  void channelChanged(int arg_1) W_SIGNAL(channelChanged, arg_1);
  void currentPatternChanged(int arg_1) W_SIGNAL(currentPatternChanged, arg_1);
  void execPosition(int arg_1) W_SIGNAL(execPosition, arg_1);
  //! The pattern playing and the one waiting for the next quantification
  //! point (-1 when none is), or -1, -1 when stopped.
  void execPattern(int current, int pending) W_SIGNAL(execPattern, current, pending);
  void patternsChanged() W_SIGNAL(patternsChanged);
  void laneLabelsChanged(LaneLabels arg_1) W_SIGNAL(laneLabelsChanged, arg_1);
  //! The bar grid the steps fall on moved: an ancestor moved or changed tempo.
  void timeGridChanged() W_SIGNAL(timeGridChanged);

  PROPERTY(int, channel READ channel WRITE setChannel NOTIFY channelChanged, W_Final)
  PROPERTY(
      int,
      currentPattern READ currentPattern WRITE setCurrentPattern NOTIFY
          currentPatternChanged,
      W_Final)
  PROPERTY(
      LaneLabels,
      laneLabels READ laneLabels WRITE setLaneLabels NOTIFY laneLabelsChanged, W_Final)
private:
  void ancestorStartDateChanged() override;
  void ancestorTempoChanged() override;
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  int m_channel{1};
  int m_currentPattern{};
  LaneLabels m_laneLabels{LaneLabels::NameAndNumber};
  std::vector<int64_t> m_execSteps;
  std::vector<Pattern> m_patterns;
};
}
