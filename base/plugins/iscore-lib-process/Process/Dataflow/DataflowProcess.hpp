#pragma once
#include <Process/Process.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT DataflowProcess : public Process::ProcessModel
{
  Q_OBJECT
  ISCORE_SERIALIZE_FRIENDS
public:
    using Process::ProcessModel::ProcessModel;

  explicit DataflowProcess(
          const DataflowProcess& source,
          const Id<Process::ProcessModel>& id,
          const QString& name,
          QObject* parent);

  template<typename Impl>
  explicit DataflowProcess(
          Impl& vis,
          QObject* parent) :
      Process::ProcessModel{vis, parent}
  {
      vis.writeTo(*this);
      updateCounts();
  }

  ~DataflowProcess();

  void setInlets(const std::vector<Port>& inlets);
  void setOutlets(const std::vector<Port>& outlets);

  std::size_t audioInlets() const { return m_portCount.audioIn; }
  std::size_t messageInlets() const { return m_portCount.messageIn; }
  std::size_t midiInlets() const { return m_portCount.midiIn; }

  std::size_t audioOutlets() const { return m_portCount.audioOut; }
  std::size_t messageOutlets() const { return m_portCount.messageOut; }
  std::size_t midiOutlets() const { return m_portCount.midiOut; }

  const std::vector<Port>& inlets() const;
  const std::vector<Port>& outlets() const;

  std::vector<Id<Cable>> cables;

signals:
  void inletsChanged();
  void outletsChanged();

private:
  void updateCounts();
  std::vector<Port> m_inlets;
  std::vector<Port> m_outlets;

  struct {
  std::size_t audioIn{}, messageIn{}, midiIn{};
  std::size_t audioOut{}, messageOut{}, midiOut{};
  } m_portCount;
};


}
