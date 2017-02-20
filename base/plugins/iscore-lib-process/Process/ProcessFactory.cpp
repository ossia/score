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

LayerModel* LayerFactory::make(
    Process::ProcessModel& proc,
    const Id<LayerModel>& viewModelId,
    const QByteArray& constructionData,
    QObject* parent)
{
  auto lm = makeLayer_impl(proc, viewModelId, constructionData, parent);
  proc.addLayer(lm);

  return lm;
}

LayerModel* LayerFactory::load(
    const VisitorVariant& v,
    const iscore::RelativePath& process,
    QObject* parent)
{
  switch (v.identifier)
  {
    case DataStream::type():
    {
      auto& proc = process.find<Process::ProcessModel>(parent);

      // Note : we pass a reference to the stream,
      // so it is already increased past the reading of the path.
      auto lm = loadLayer_impl(proc, v, parent);
      proc.addLayer(lm);

      return lm;
    }
    case JSONObject::type():
    {
      if (auto p = process.try_find<Process::ProcessModel>(parent))
      {
        auto& proc = *p;
        auto lm = loadLayer_impl(proc, v, parent);
        proc.addLayer(lm);

        return lm;
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

LayerModel* LayerFactory::cloneLayer(
    Process::ProcessModel& proc,
    const Id<LayerModel>& newId,
    const LayerModel& source,
    QObject* parent)
{
  auto lm = cloneLayer_impl(proc, newId, source, parent);
  proc.addLayer(lm);

  return lm;
}

QByteArray LayerFactory::makeLayerConstructionData(const ProcessModel&) const
{
  return {};
}

QByteArray LayerFactory::makeStaticLayerConstructionData() const
{
  return {};
}

LayerPresenter* LayerFactory::makeLayerPresenter(
    const LayerModel& lm,
    LayerView* v,
    const ProcessPresenterContext& context,
    QObject* parent)
{
  return new Dummy::DummyLayerPresenter{
      lm, static_cast<Dummy::DummyLayerView*>(v), context, parent};
}

LayerView*
LayerFactory::makeLayerView(const LayerModel& view, QQuickPaintedItem* parent)
{
  return new Dummy::DummyLayerView{parent};
}

LayerModelPanelProxy*
LayerFactory::makePanel(const LayerModel& layer, QObject* parent)
{
  return new Process::GraphicsViewLayerModelPanelProxy{layer, parent};
}

bool LayerFactory::matches(const ProcessModel& p) const
{
  return matches(p.concreteKey());
}

bool LayerFactory::matches(
    const UuidKey<Process::ProcessModelFactory>& p) const
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

LayerFactoryList::object_type*
LayerFactoryList::loadMissing(const VisitorVariant& vis, const iscore::RelativePath&, QObject* parent) const
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
    const UuidKey<ProcessModelFactory>& proc) const
{
  for (auto& fac : *this)
  {
    if (fac.matches(proc))
      return &fac;
  }
  return nullptr;
}
}
