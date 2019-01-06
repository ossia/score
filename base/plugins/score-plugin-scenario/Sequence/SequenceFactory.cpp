#include <Sequence/SequenceFactory.hpp>
#include <Sequence/SequencePresenter.hpp>
#include <Sequence/SequenceView.hpp>

namespace Sequence
{

LayerFactory::LayerFactory(
    Scenario::EditionSettings& e)
    : m_editionSettings{e}
{
}

Process::LayerView* LayerFactory::makeLayerView(
    const Process::ProcessModel& p, QGraphicsItem* parent) const
{
  if (auto s = dynamic_cast<const Sequence::ProcessModel*>(&p))
    return new SequenceView{*s, parent};

  return nullptr;
}

Process::MiniLayer* LayerFactory::makeMiniLayer(
    const Process::ProcessModel& p, QGraphicsItem* parent) const
{
  return nullptr;
}

bool LayerFactory::matches(
    const UuidKey<Process::ProcessModel>& p) const
{
  return p == Metadata<ConcreteKey_k, Sequence::ProcessModel>::get();
}

UuidKey<Process::ProcessModel>
LayerFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, Sequence::ProcessModel>::get();
}

Process::LayerPresenter* LayerFactory::makeLayerPresenter(
    const Process::ProcessModel& lm, Process::LayerView* view,
    const Process::ProcessPresenterContext& context, QObject* parent) const
{
  if (auto vm = dynamic_cast<const Sequence::ProcessModel*>(&lm))
  {
    auto pres = new SequencePresenter{m_editionSettings, *vm, view,
                                              context, parent};
    return pres;
  }
  return nullptr;
}
}
