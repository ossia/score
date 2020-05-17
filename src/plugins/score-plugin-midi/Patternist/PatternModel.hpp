#pragma once
#include <Process/Process.hpp>

#include <Patternist/PatternMetadata.hpp>
#include <score_plugin_midi_export.h>

#include <verdigris>

namespace Patternist
{
struct Lane
{
  std::vector<bool> pattern;
  uint8_t note{};

  bool operator==(const Lane& other) const noexcept
  {
    return note == other.note && pattern == other.pattern;
  }
};

struct Pattern
{
  int length{16};
  int division{8};
  std::vector<Lane> lanes;

  bool operator==(const Pattern& other) const noexcept
  {
    return length == other.length && division == other.division && lanes == other.lanes;
  }
};

class SCORE_PLUGIN_MIDI_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  W_OBJECT(ProcessModel)

public:
  PROCESS_METADATA_IMPL(Patternist::ProcessModel)
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
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

  void setPattern(int n, Pattern p);
  void setPatterns(const std::vector<Pattern>& n);
  const std::vector<Pattern>& patterns() const noexcept;

  std::unique_ptr<Process::MidiOutlet> outlet;

public:
  void channelChanged(int arg_1) W_SIGNAL(channelChanged, arg_1);
  void currentPatternChanged(int arg_1) W_SIGNAL(currentPatternChanged, arg_1);
  void patternsChanged() W_SIGNAL(patternsChanged);

  PROPERTY(int, channel READ channel WRITE setChannel NOTIFY channelChanged, W_Final)
  PROPERTY(
      int,
      currentPattern READ currentPattern WRITE setCurrentPattern NOTIFY currentPatternChanged,
      W_Final)
private:
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  int m_channel{1};
  int m_currentPattern{};
  std::vector<Pattern> m_patterns;
};
}
