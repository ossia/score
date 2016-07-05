#include "ProcessFactory.hpp"
#include "StateProcessFactoryList.hpp"
#include "ProcessList.hpp"
#include <Process/Dummy/DummyLayerModel.hpp>
#include <Process/Dummy/DummyLayerPresenter.hpp>
#include <Process/Dummy/DummyLayerView.hpp>
#include <Process/Process.hpp>

namespace Process
{
ProcessFactory::~ProcessFactory() = default;
ProcessModelFactory::~ProcessModelFactory() = default;
LayerModelFactory::~LayerModelFactory() = default;
ProcessList::~ProcessList() = default;
StateProcessList::~StateProcessList() = default;


LayerModel* LayerModelFactory::makeLayer(
        Process::ProcessModel& proc,
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lm = makeLayer_impl(proc, viewModelId, constructionData, parent);
    proc.addLayer(lm);

    return lm;
}


LayerModel*LayerModelFactory::loadLayer(
        Process::ProcessModel& proc,
        const VisitorVariant& v,
        QObject* parent)
{
    auto lm = loadLayer_impl(proc, v, parent);
    proc.addLayer(lm);

    return lm;
}

LayerModel*LayerModelFactory::cloneLayer(
        Process::ProcessModel& proc,
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto lm = cloneLayer_impl(proc, newId, source, parent);
    proc.addLayer(lm);

    return lm;
}



QByteArray LayerModelFactory::makeLayerConstructionData(
        const ProcessModel&) const
{
    return { };
}

QByteArray LayerModelFactory::makeStaticLayerConstructionData() const
{
    return { };
}

LayerPresenter*LayerModelFactory::makeLayerPresenter(
        const LayerModel& lm,
        LayerView* v,
        const ProcessPresenterContext& context,
        QObject* parent)
{
    return new Dummy::DummyLayerPresenter{lm, static_cast<Dummy::DummyLayerView*>(v), context, parent};
}

LayerView*LayerModelFactory::makeLayerView(
        const LayerModel& view,
        QGraphicsItem* parent)
{
    return new Dummy::DummyLayerView{parent};
}

LayerModel*LayerModelFactory::makeLayer_impl(
        ProcessModel& p,
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new Dummy::DummyLayerModel{p, viewModelId, parent};
}

LayerModel*LayerModelFactory::loadLayer_impl(
        ProcessModel& p ,
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new Dummy::DummyLayerModel{
                        deserializer, p, parent};

        return autom;
    });
}

LayerModel*LayerModelFactory::cloneLayer_impl(
        ProcessModel& p,
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return new Dummy::DummyLayerModel{
        safe_cast<const Dummy::DummyLayerModel&>(source),
                p,
                newId,
                parent};
}

}
