#pragma once
#include <Process/ProcessFactory.hpp>
#include <QByteArray>
#include <QString>

#include <Process/TimeValue.hpp>

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

class ScenarioFactory final : public Process::ProcessModelFactory
{
public:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override;
  QString prettyName() const override;
  QString category() const override;
  Process::ProcessModel* make(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent) override;

  Process::ProcessModel* load(const VisitorVariant&, QObject* parent) override;
};

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
