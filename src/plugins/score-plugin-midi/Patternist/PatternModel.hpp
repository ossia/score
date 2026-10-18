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
  //! 0 - 1: each time the step plays, its velocity moves by up to this much
  //! either way.
  float velocityRandom{0.f};
  //! 0 - 1, scaled by the Density input.
  float chance{1.f};
  //! -0.5 - 0.5 of a step: early or late.
  float offset{0.f};
  //! 0 - 0.5 of a step: each time the step plays, its timing moves by up to
  //! this much either way.
  float offsetRandom{0.f};
  //! 0 - 1 of a step; a tie, or 1, holds the note until the next step.
  float gate{1.f};
  //! 0 - 1: each time the step plays, its gate moves by up to this much
  //! either way.
  float gateRandom{0.f};
  //! Notes played within the step, 1 - 8.
  uint8_t ratchet{1};
  //! -1 - 1: positive starts the ratchet's hits wide apart and tightens them,
  //! negative the other way round.
  float pace{0.f};
  //! Semitones added to the lane's note.
  int8_t pitch{0};
  //! 0 - 24: each time the step plays, its pitch moves by up to this many
  //! semitones either way.
  uint8_t pitchRandom{0};
  Condition condition{Condition::Always};
  uint8_t cycleA{1};
  uint8_t cycleB{2};

  bool operator==(const StepParams&) const noexcept = default;
  bool isDefault() const noexcept { return *this == StepParams{}; }
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
  //! A MIDI note, or accentLane or slideLane.
  uint8_t note{};
  bool mute{};
  bool solo{};
  //! Steps before the lane loops; 0 follows the pattern.
  int length{};
  //! Steps per whole note; 0 follows the pattern.
  int division{};
  //! 0 - 1.
  float velocity{100.f / 127.f};
  //! Same size as `pattern`.
  std::vector<StepParams> params;
  //! Which random spreads each hit of a ratchet draws for itself, rather than
  //! the whole note sharing one.
  enum RandomPerHit : uint8_t
  {
    VelocityPerHit = 1,
    PitchPerHit = 2,
    TimingPerHit = 4,
    GatePerHit = 8
  };
  uint8_t randomPerHit{};

  bool operator==(const Lane& other) const noexcept = default;
};

//! Lanes that play no note of their own and act on the notes of the others on
//! the same step. An accent raises their velocity towards the top, by the
//! accent step's velocity times the accent lane's; a slide holds them into the
//! next step, which strikes before the held note is released, so that a mono
//! synth glides between the two.
inline constexpr uint8_t accentLane = 255;
inline constexpr uint8_t slideLane = 254;
inline bool isControlLane(const Lane& l) noexcept
{
  return l.note == accentLane || l.note == slideLane;
}

struct Pattern
{
  static const constexpr int maxLength = 128;
  static const constexpr int maxLanes = 128;
  //! Steps per whole note, at most.
  static const constexpr int maxDivision = 64;

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

//! What a lane's step holds, before mute, solo, chance and conditions.
inline Note writtenNote(const Pattern& p, const Lane& l, int step) noexcept
{
  return (step >= 0 && step < int(l.pattern.size())) ? l.pattern[step] : Note::Rest;
}

inline const StepParams& stepParams(const Lane& l, int step) noexcept
{
  static const StepParams defaults;
  return (step >= 0 && step < int(l.params.size())) ? l.params[step] : defaults;
}

//! Whether a tied step strikes a note: when the step before plays nothing,
//! when it ratchets, or when its pitch differs. Otherwise it only holds the
//! note struck before it, and its velocity does nothing.
inline bool tieStrikes(const Pattern& p, const Lane& l, int step) noexcept
{
  const int len = laneLength(p, l);
  if(len <= 0)
    return true;
  const int prev = ((step - 1) % len + len) % len;
  if(writtenNote(p, l, prev) == Note::Rest)
    return true;
  const StepParams& sp = stepParams(l, step);
  const StepParams& pp = stepParams(l, prev);
  const bool spread
      = !(l.randomPerHit & Lane::PitchPerHit) && (sp.pitchRandom > 0 || pp.pitchRandom > 0);
  return sp.ratchet > 1 || sp.pitch != pp.pitch || spread;
}

//! The step that struck the note a step holds: itself unless it is a tie that
//! does not strike, in which case the one before, and so on.
inline int struckStep(const Pattern& p, const Lane& l, int step) noexcept
{
  const int len = laneLength(p, l);
  for(int k = 0; k < len; k++)
  {
    if(writtenNote(p, l, step) != Note::Legato || tieStrikes(p, l, step))
      return step;
    step = ((step - 1) % len + len) % len;
  }
  return step;
}

//! What a lane plays at a step, once mute and solo are applied.
inline Note audibleNote(const Pattern& p, const Lane& l, int step, bool anySolo) noexcept
{
  if(l.mute || (anySolo && !l.solo))
    return Note::Rest;
  return writtenNote(p, l, step);
}

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
  //! Brings what was loaded into the ranges everything else relies on.
  void sanitize();

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
  //! Multiplies every step's ratchet.
  std::unique_ptr<Process::ControlInlet> ratchet;
  //! Added to every step's pace.
  std::unique_ptr<Process::ControlInlet> pace;
  //! When set, the interval the steps that ratchet repeat at.
  std::unique_ptr<Process::ControlInlet> repeatTime;
  std::unique_ptr<Process::MidiOutlet> outlet;
  std::unique_ptr<Process::Outlet> accent;
  std::unique_ptr<Process::Outlet> slide;

public:
  void channelChanged(int arg_1) W_SIGNAL(channelChanged, arg_1);
  void currentPatternChanged(int arg_1) W_SIGNAL(currentPatternChanged, arg_1);
  //! execSteps() changed.
  void execStepsChanged() W_SIGNAL(execStepsChanged);
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
  //! Patterns made by selecting past the end and not edited since.
  std::vector<bool> m_grown;
  std::vector<Pattern> m_patterns;
};
}
