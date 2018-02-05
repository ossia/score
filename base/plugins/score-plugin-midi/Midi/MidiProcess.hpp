#pragma once
#include <Midi/MidiProcessMetadata.hpp>
#include <Process/Process.hpp>

#include <Midi/MidiNote.hpp>
#include <score/tools/Clamp.hpp>

namespace Midi
{

class SCORE_PLUGIN_MIDI_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  Q_OBJECT

public:
    PROCESS_METADATA_IMPL(Midi::ProcessModel)
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_outlets.push_back(outlet.get());
  }

  ~ProcessModel() override;

  score::EntityMap<Note> notes;

  void setDevice(const QString& dev);
  const QString& device() const;

  void setChannel(int n);
  int channel() const;

  std::pair<int, int> range() const
  {
    return m_range;
  }

  void setRange(int min, int max)
  {
    if(min == max)
    {
      min = 127;
      max = 0;

      for(auto& note : notes)
      {
        if(note.pitch() < min)
          min = note.pitch();
        if(note.pitch() > max)
          max = note.pitch();
      }
    }
    else
    {
      min = std::max(0, min);
      max = std::min(127, max);
      if(min >= max)
      {
        min = std::max(0, max - 7);
        max = std::min(127, min + 14);
      }
    }

    std::pair<int,int> range{min, max};
    if(range != m_range)
    {
      m_range = range;
      rangeChanged(min, max);
    }
  }

  std::unique_ptr<Process::Outlet> outlet;

Q_SIGNALS:
  void notesChanged();
  void deviceChanged(const QString&);
  void channelChanged(int);

  void rangeChanged(int, int);

private:
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  QString m_device;
  int m_channel{1};
  std::pair<int, int> m_range{0, 127};
};

}
