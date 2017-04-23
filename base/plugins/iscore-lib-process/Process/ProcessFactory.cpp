#include "ProcessFactory.hpp"
#include "ProcessList.hpp"
#include "StateProcessFactoryList.hpp"
#include <Process/Dummy/DummyLayerPresenter.hpp>
#include <Process/Dummy/DummyLayerView.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/Process.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/path/RelativePath.hpp>

namespace Process
{
ProcessModelFactory::~ProcessModelFactory() = default;
LayerFactory::~LayerFactory() = default;
ProcessFactoryList::~ProcessFactoryList() = default;
LayerFactoryList::~LayerFactoryList() = default;
StateProcessList::~StateProcessList() = default;


LayerPresenter* LayerFactory::makeLayerPresenter(
    const ProcessModel& lm,
    LayerView* v,
    const ProcessPresenterContext& context,
    QObject* parent)
{
  return new Dummy::DummyLayerPresenter{
      lm, static_cast<Dummy::DummyLayerView*>(v), context, parent};
}

LayerView*
LayerFactory::makeLayerView(const ProcessModel& view, QGraphicsItem* parent)
{
  return new Dummy::DummyLayerView{parent};
}

LayerPanelProxy*
LayerFactory::makePanel(const ProcessModel& layer, QObject* parent)
{
  return new Process::GraphicsViewLayerPanelProxy{layer, parent};
}

bool LayerFactory::matches(const ProcessModel& p) const
{
  return matches(p.concreteKey());
}

bool LayerFactory::matches(
    const UuidKey<Process::ProcessModel>& p) const
{
  return false;
}

ProcessFactoryList::object_type* ProcessFactoryList::loadMissing(
    const VisitorVariant& vis, QObject* parent) const
{
  ISCORE_TODO;
  return nullptr;
}

StateProcessList::object_type*
StateProcessList::loadMissing(const VisitorVariant& vis, QObject* parent) const
{
  ISCORE_TODO;
  return nullptr;
}

LayerFactory*
LayerFactoryList::findDefaultFactory(const ProcessModel& proc) const
{
  return findDefaultFactory(proc.concreteKey());
}

LayerFactory* LayerFactoryList::findDefaultFactory(
    const UuidKey<ProcessModel>& proc) const
{
  for (auto& fac : *this)
  {
    if (fac.matches(proc))
      return &fac;
  }
  return nullptr;
}
}
