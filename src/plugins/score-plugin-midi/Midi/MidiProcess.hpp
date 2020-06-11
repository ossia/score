#pragma once
#include <Midi/MidiNote.hpp>
#include <Midi/MidiProcessMetadata.hpp>
#include <Process/Process.hpp>

#include <score/tools/Clamp.hpp>

#include <verdigris>

namespace Midi
{

class SCORE_PLUGIN_MIDI_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  W_OBJECT(ProcessModel)

public:
  PROCESS_METADATA_IMPL(Midi::ProcessModel)
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

  score::EntityMap<Note> notes;

  void setChannel(int n);
  int channel() const;

  std::pair<int, int> range() const { return m_range; }

  void setRange(int min, int max);

  std::unique_ptr<Process::MidiOutlet> outlet;

  using Process::ProcessModel::setDuration;

public:
  void notesNeedUpdate() W_SIGNAL(notesNeedUpdate);
  void notesChanged() W_SIGNAL(notesChanged);
  void channelChanged(int arg_1) W_SIGNAL(channelChanged, arg_1);

  void rangeChanged(int arg_1, int arg_2) W_SIGNAL(rangeChanged, arg_1, arg_2);

private:
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  int m_channel{1};
  std::pair<int, int> m_range{0, 127};
};
}
