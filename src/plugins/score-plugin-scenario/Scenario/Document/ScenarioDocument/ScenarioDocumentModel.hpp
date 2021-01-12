#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Scenario/Instantiations.hpp>

#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <core/document/Document.hpp>

#include <score_plugin_scenario_export.h>

#include <verdigris>

#include <unordered_set>

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
    : public score::DocumentDelegateModel,
      public Nano::Observer
{
  W_OBJECT(ScenarioDocumentModel)
  SCORE_SERIALIZE_FRIENDS
public:
  ScenarioDocumentModel(const score::DocumentContext& ctx, QObject* parent);

  template <typename Impl>
  ScenarioDocumentModel(Impl& vis, const score::DocumentContext& ctx, QObject* parent)
      : score::DocumentDelegateModel{vis, parent}, m_context{ctx}
  {
    vis.writeTo(*this);
    init();
  }

  void finishLoading();
  ~ScenarioDocumentModel() override;

  BaseScenario& baseScenario() const { return *m_baseScenario; }

  IntervalModel& baseInterval() const;

  void serialize(const VisitorVariant&) const override;

  score::EntityMap<Process::Cable> cables;

  void addBus(const IntervalModel*);
  void removeBus(const IntervalModel*);
  std::vector<const IntervalModel*> busIntervals;
  std::vector<const StateModel*> statesWithControls;

  void busesChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, busesChanged)

  const score::DocumentContext& context() const noexcept { return m_context; }
private:
  void init();
  const score::DocumentContext& m_context;
  BaseScenario* m_baseScenario{};
  std::vector<QByteArray> m_savedCables;
  rapidjson::Document m_savedCablesJson;
};
}
