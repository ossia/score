#pragma once
#include <Media/Sound/SoundModel.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Process/Process.hpp>

#include <Pd/Executor/PdExecutor.hpp>
#include <ossia/editor/value/value.hpp>
#include <Media/Sound/SoundModel.hpp>

#include <iscore_addon_pd_export.h>

namespace Dataflow
{
class SoundNode : public Process::Node
{
public:
  SoundNode(
        Dataflow::DocumentPlugin& doc,
        Media::Sound::ProcessModel& proc,
        Id<Node> c,
        QObject* parent);

  Media::Sound::ProcessModel& process;
  QString getText() const override;
  std::size_t audioInlets() const override;
  std::size_t messageInlets() const override;
  std::size_t midiInlets() const override;

  std::size_t audioOutlets() const override;
  std::size_t messageOutlets() const override;
  std::size_t midiOutlets() const override;

  std::vector<Process::Port> inlets() const override;
  std::vector<Process::Port> outlets() const override;

  ~SoundNode();

  std::vector<Id<Process::Cable>> cables() const override;
  void addCable(Id<Process::Cable> c) override;
  void removeCable(Id<Process::Cable> c) override;

private:
  std::vector<Id<Process::Cable>> m_cables;

};

class SoundComponent :
    public ProcessComponent_T<Media::Sound::ProcessModel>
{
  COMPONENT_METADATA("92540a97-741c-4e36-a532-9edafc45c768")
public:
  SoundComponent(
      Media::Sound::ProcessModel& proc,
      DocumentPlugin& doc,
      const Id<iscore::Component>& id,
      QObject* parent_obj):
    ProcessComponent_T<Media::Sound::ProcessModel>{proc, doc, id, "SoundComponent", parent_obj}
  , m_node{doc, proc, Id<Process::Node>{}, this}
  {
  }

    SoundNode& mainNode() override { return m_node; }
private:
    SoundNode m_node;
};

using SoundComponentFactory = ProcessComponentFactory_T<SoundComponent>;

class ISCORE_ADDON_PD_EXPORT SoundGraphNode final :
    public ossia::graph_node
{
public:
  SoundGraphNode();
  ~SoundGraphNode();

  void setSound(AudioArray vec);
private:
  void run(ossia::execution_state& e) override;

  AudioArray m_data;
};

class SoundExecComponent final
    : public ::Engine::Execution::
    ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("a25d0de0-74e2-4011-aeb6-4188673015f2")
public:
  SoundExecComponent(
      Engine::Execution::ConstraintComponent& parentConstraint,
      Media::Sound::ProcessModel& element,
      const Dataflow::DocumentPlugin& df,
      const ::Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  void recompute();

  ~SoundExecComponent();

private:
  ossia::node_ptr node;
  const Dataflow::DocumentPlugin& m_df;
};
using SoundExecComponentFactory
= Pd::ProcessComponentFactory_T<SoundExecComponent>;

}
