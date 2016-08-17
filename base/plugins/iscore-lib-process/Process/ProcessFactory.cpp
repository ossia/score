#include "ProcessFactory.hpp"
#include "StateProcessFactoryList.hpp"
#include "ProcessList.hpp"
#include <Process/Dummy/DummyLayerModel.hpp>
#include <Process/Dummy/DummyLayerPresenter.hpp>
#include <Process/Dummy/DummyLayerView.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/Process.hpp>

namespace Process
{
ProcessFactory::~ProcessFactory() = default;
ProcessModelFactory::~ProcessModelFactory() = default;
LayerFactory::~LayerFactory() = default;
ProcessList::~ProcessList() = default;
StateProcessList::~StateProcessList() = default;


LayerModel* LayerFactory::makeLayer(
        Process::ProcessModel& proc,
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lm = makeLayer_impl(proc, viewModelId, constructionData, parent);
    proc.addLayer(lm);

    return lm;
}


LayerModel*LayerFactory::loadLayer(
        Process::ProcessModel& proc,
        const VisitorVariant& v,
        QObject* parent)
{
    auto lm = loadLayer_impl(proc, v, parent);
    proc.addLayer(lm);

    return lm;
}

LayerModel*LayerFactory::cloneLayer(
        Process::ProcessModel& proc,
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto lm = cloneLayer_impl(proc, newId, source, parent);
    proc.addLayer(lm);

    return lm;
}



QByteArray LayerFactory::makeLayerConstructionData(
        const ProcessModel&) const
{
    return { };
}

QByteArray LayerFactory::makeStaticLayerConstructionData() const
{
    return { };
}

LayerPresenter*LayerFactory::makeLayerPresenter(
        const LayerModel& lm,
        LayerView* v,
        const ProcessPresenterContext& context,
        QObject* parent)
{
    return new Dummy::DummyLayerPresenter{lm, static_cast<Dummy::DummyLayerView*>(v), context, parent};
}

LayerView*LayerFactory::makeLayerView(
        const LayerModel& view,
        QGraphicsItem* parent)
{
    return new Dummy::DummyLayerView{parent};
}

LayerModelPanelProxy* LayerFactory::makePanel(const LayerModel& layer, QObject* parent)
{
    return new Process::GraphicsViewLayerModelPanelProxy{layer, parent};
}

LayerModel*LayerFactory::makeLayer_impl(
        ProcessModel& p,
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new Dummy::Layer{p, viewModelId, parent};
}

LayerModel*LayerFactory::loadLayer_impl(
        ProcessModel& p ,
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new Dummy::Layer{
                        deserializer, p, parent};

        return autom;
    });
}

LayerModel*LayerFactory::cloneLayer_impl(
        ProcessModel& p,
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return new Dummy::Layer{
        safe_cast<const Dummy::Layer&>(source),
                p,
                newId,
                parent};
}

}
