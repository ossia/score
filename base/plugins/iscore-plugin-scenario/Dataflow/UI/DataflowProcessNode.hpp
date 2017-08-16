#pragma once
#include <Process/Process.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>

namespace Process
{
class DataflowProcess;
class DataflowProcessNode
        : public Process::Node
{
  public:
    DataflowProcessNode(
          Process::DataflowProcess& proc);

    Process::DataflowProcess& process;

    ~DataflowProcessNode();

    QString getText() const override;
    std::size_t audioInlets() const override;
    std::size_t messageInlets() const override;
    std::size_t midiInlets() const override;

    std::size_t audioOutlets() const override;
    std::size_t messageOutlets() const override;
    std::size_t midiOutlets() const override;

    std::vector<Process::Port> inlets() const override;
    std::vector<Process::Port> outlets() const override;

    std::vector<Id<Process::Cable>> cables() const override;
    void addCable(Id<Process::Cable> c) override;
    void removeCable(Id<Process::Cable> c) override;
};

class ISCORE_LIB_PROCESS_EXPORT DataflowProcess : public Process::ProcessModel
{
  Q_OBJECT
  ISCORE_SERIALIZE_FRIENDS
  public:

  DataflowProcess(
      TimeVal duration,
      const Id<ProcessModel>& id,
      const QString& name,
      QObject* parent);

  explicit DataflowProcess(
          const DataflowProcess& source,
          const Id<Process::ProcessModel>& id,
          const QString& name,
          QObject* parent);

  DataflowProcess(DataStream::Deserializer& vis, QObject* parent);
  DataflowProcess(JSONObject::Deserializer& vis, QObject* parent);

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

  DataflowProcessNode m_node{*this};
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
