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
  PROCESS_METADATA_IMPL(Midi::ProcessModel)
  Q_OBJECT

public:
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  explicit ProcessModel(
      const ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  virtual ~ProcessModel();

  score::EntityMap<Note> notes;

  void setDevice(const QString& dev)
  {
    m_device = dev;
    emit deviceChanged(m_device);
  }

  const QString& device() const
  {
    return m_device;
  }

  void setChannel(int n)
  {
    m_channel = clamp(n, 1, 16);
    emit channelChanged(n);
  }
  int channel() const
  {
    return m_channel;
  }

signals:
  void notesChanged();
  void deviceChanged(const QString&);
  void channelChanged(int);

private:
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  QString m_device;
  int m_channel{1};
};
}
