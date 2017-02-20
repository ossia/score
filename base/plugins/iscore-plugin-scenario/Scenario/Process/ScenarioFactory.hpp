#pragma once
#include <Process/ProcessFactory.hpp>
#include <QByteArray>
#include <QString>

#include <Process/TimeValue.hpp>

namespace Process
{
class LayerModel;
class LayerPresenter;
class LayerView;
class ProcessModel;
}
class QQuickPaintedItem;
class QObject;
struct VisitorVariant;
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class EditionSettings;

class ScenarioFactory final : public Process::ProcessModelFactory
{
public:
  UuidKey<Process::ProcessModelFactory> concreteKey() const noexcept override;
  QString prettyName() const override;

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
  QByteArray makeStaticLayerConstructionData() const override;

  QByteArray
  makeLayerConstructionData(const Process::ProcessModel& proc) const override;

  Process::LayerModel* makeLayer_impl(
      Process::ProcessModel& proc,
      const Id<Process::LayerModel>& viewModelId,
      const QByteArray& constructionData,
      QObject* parent) override;

  Process::LayerModel* cloneLayer_impl(
      Process::ProcessModel& proc,
      const Id<Process::LayerModel>& newId,
      const Process::LayerModel& source,
      QObject* parent) override;

  Process::LayerModel* loadLayer_impl(
      Process::ProcessModel& proc,
      const VisitorVariant& vis,
      QObject* parent) override;

  Process::LayerPresenter* makeLayerPresenter(
      const Process::LayerModel&,
      Process::LayerView*,
      const Process::ProcessPresenterContext& context,
      QObject* parent) override;

  Process::LayerView* makeLayerView(
      const Process::LayerModel& viewmodel, QQuickPaintedItem* parent) override;

  bool matches(const UuidKey<Process::ProcessModelFactory>& p) const override;
  UuidKey<Process::LayerFactory> concreteKey() const noexcept override;

private:
  Scenario::EditionSettings& m_editionSettings;
};
}
