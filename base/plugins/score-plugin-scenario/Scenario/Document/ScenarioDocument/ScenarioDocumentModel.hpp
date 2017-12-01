#pragma once
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <core/document/Document.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score_plugin_scenario_export.h>
#include <QPointer>
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
      , m_context{ctx}
  {
    vis.writeTo(*this);
  }

  void finishLoading();
  virtual ~ScenarioDocumentModel() = default;

  BaseScenario& baseScenario() const
  {
    return *m_baseScenario;
  }

  IntervalModel& baseInterval() const;

  void serialize(const VisitorVariant&) const override;

  score::EntityMap<Process::Cable> cables;
private:
  void initializeNewDocument(const IntervalModel& viewmodel);
  const score::DocumentContext& m_context;
  BaseScenario* m_baseScenario{};
  QJsonArray m_savedCables;
};
}
