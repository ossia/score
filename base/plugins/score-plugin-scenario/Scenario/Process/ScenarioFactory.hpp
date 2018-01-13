#pragma once
#include <Process/ProcessFactory.hpp>
#include <QByteArray>
#include <QString>

#include <Process/GenericProcessFactory.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
namespace Process
{
class LayerPresenter;
class LayerView;
class ProcessModel;
}
class QGraphicsItem;
class QObject;
struct VisitorVariant;
#include <score/model/Identifier.hpp>

namespace Scenario
{
class EditionSettings;

using ScenarioFactory = Process::ProcessFactory_T<Scenario::ProcessModel>;

class ScenarioTemporalLayerFactory final : public Process::LayerFactory
{
public:
  ScenarioTemporalLayerFactory(Scenario::EditionSettings&);

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel&,
      Process::LayerView*,
      const Process::ProcessPresenterContext& context,
      QObject* parent) override;

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& viewmodel, QGraphicsItem* parent) override;

  Process::MiniLayer*
  makeMiniLayer(const Process::ProcessModel& view, QGraphicsItem* parent) override;

  bool matches(const UuidKey<Process::ProcessModel>& p) const override;
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override;

private:
  Scenario::EditionSettings& m_editionSettings;
};
}
