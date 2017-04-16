#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateModel.hpp>

#include <QPointer>
#include <core/document/Document.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore_plugin_scenario_export.h>

class JSONObject;
class DataStream;
namespace Process
{
class LayerModel;
}
namespace Process
{
class LayerPresenter;
}
class QObject;

namespace Scenario
{
class BaseScenario;
class ConstraintModel;
class FullViewConstraintViewModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentModel final
    : public iscore::DocumentDelegateModel
{
  Q_OBJECT
  ISCORE_SERIALIZE_FRIENDS
public:
  ScenarioDocumentModel(const iscore::DocumentContext& ctx, QObject* parent);

  template <typename Impl>
  ScenarioDocumentModel(
      Impl& vis,
      const iscore::DocumentContext& ctx,
      QObject* parent)
      : iscore::DocumentDelegateModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  virtual ~ScenarioDocumentModel() = default;

  BaseScenario& baseScenario() const
  {
    return *m_baseScenario;
  }

  ConstraintModel& baseConstraint() const;

  void serialize(const VisitorVariant&) const override;

private:
  void init();
  void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);

  BaseScenario* m_baseScenario{};
};
}
