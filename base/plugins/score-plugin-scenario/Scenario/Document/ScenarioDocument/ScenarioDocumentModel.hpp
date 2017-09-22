#pragma once
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>

#include <Dataflow/DataflowWindow.hpp>
#include <Dataflow/UI/NodeItem.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <QPointer>
#include <unordered_set>
#include <core/document/Document.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score_plugin_scenario_export.h>

class JSONObject;
class DataStream;
namespace Process
{
class LayerPresenter;
}
class QObject;

namespace Scenario
{
class BaseScenario;
class IntervalModel;
class FullViewConstraintViewModel;
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentModel final
    : public score::DocumentDelegateModel
    , public Nano::Observer
{
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS
public:
  ScenarioDocumentModel(const score::DocumentContext& ctx, QObject* parent);

  template <typename Impl>
  ScenarioDocumentModel(
      Impl& vis,
      const score::DocumentContext& ctx,
      QObject* parent)
      : score::DocumentDelegateModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  virtual ~ScenarioDocumentModel() = default;

  BaseScenario& baseScenario() const
  {
    return *m_baseScenario;
  }

  IntervalModel& baseInterval() const;

  void serialize(const VisitorVariant&) const override;

  score::EntityMap<Process::Cable> cables;
  Dataflow::DataflowWindow window;

  void registerNode(Dataflow::NodeItem* n);
private:
  void init();
  void initializeNewDocument(const IntervalModel& viewmodel);

  void on_cableAdded(Process::Cable& c);
  void on_cableRemoving(const Process::Cable& c);

  BaseScenario* m_baseScenario{};
  IdContainer<Dataflow::CableItem, Process::Cable> cableItems;
  std::unordered_set<Dataflow::NodeItem*> nodeItems;

};
}
