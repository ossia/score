#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/TimeValue.hpp>
#include <Sequence/SequenceModel.hpp>

#include <QByteArray>
#include <QString>
namespace Process
{
class LayerPresenter;
class LayerView;
class ProcessModel;
}
namespace Scenario
{
class EditionSettings;
}
class QGraphicsItem;
class QObject;
struct VisitorVariant;

namespace Sequence
{
using SequenceFactory = Process::ProcessFactory_T<Sequence::ProcessModel>;

class LayerFactory final : public Process::LayerFactory
{
public:
  LayerFactory(Scenario::EditionSettings&);

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel&, Process::LayerView*,
      const Process::ProcessPresenterContext& context,
      QObject* parent) const override;

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& viewmodel,
      QGraphicsItem* parent) const override;

  Process::MiniLayer* makeMiniLayer(
      const Process::ProcessModel& view, QGraphicsItem* parent) const override;

  bool matches(const UuidKey<Process::ProcessModel>& p) const override;
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override;

private:
  Scenario::EditionSettings& m_editionSettings;
};

}
