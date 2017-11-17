#pragma once
#include <MidiUtil/MidiUtilProcessMetadata.hpp>
#include <Process/Process.hpp>

#include <score/tools/Clamp.hpp>

namespace MidiUtil
{

class SCORE_PLUGIN_MIDI_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(MidiUtil::ProcessModel)
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

  ~ProcessModel() override;


  std::vector<Process::Port*> inlets() const override;
  std::vector<Process::Port*> outlets() const override;

  std::unique_ptr<Process::Port> outlet;

};

}
