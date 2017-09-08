#pragma once
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>

#include <QPointer>
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
class FullViewIntervalViewModel;
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentModel final
    : public score::DocumentDelegateModel
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

private:
  void init();
  void initializeNewDocument(const IntervalModel& viewmodel);

  BaseScenario* m_baseScenario{};
};
}
