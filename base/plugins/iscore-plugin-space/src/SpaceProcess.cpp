#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"
#include "Area/AreaParser.hpp"
#include "Space/SpaceModel.hpp"
#include "Area/Circle/CircleAreaModel.hpp"
#include "Area/Pointer/PointerAreaModel.hpp"
#include <src/Executor/ProcessExecutor.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Space
{
ProcessModel::ProcessModel(
        const iscore::DocumentContext& doc,
        const TimeValue &duration,
        const Id<Process::ProcessModel> &id,
        QObject *parent):
    Process::ProcessModel{id, ProcessMetadata::processObjectName(), parent},
    m_space{new SpaceModel{
            Id<SpaceModel>(0),
            this}},
    m_context{doc, *m_space, doc.plugin<DeviceDocumentPlugin>()}
{
    metadata.setName(QString("Space.%1").arg(*this->id().val()));
    using namespace GiNaC;
    using namespace spacelib;

    auto x_dim = new DimensionModel{"x", Id<DimensionModel>{0}, m_space};
    auto y_dim = new DimensionModel{"y", Id<DimensionModel>{1}, m_space};

    m_space->addDimension(x_dim);
    m_space->addDimension(y_dim);

    auto vp = new ViewportModel{Id<ViewportModel>{0}, m_space};
    m_space->addViewport(vp);

    /*
    const auto& space_vars = m_space->space().variables();
    {
        AreaParser circleParser("(xv-x0)^2 + (yv-y0)^2 <= r^2");

        auto ar1 = new AreaModel(circleParser.result(),
                                 *m_space, Id<AreaModel>(0), this);
        const auto& syms = ar1->area().symbols();

        ar1->setSpaceMapping({{syms[0], space_vars[0].symbol()},
                              {syms[2], space_vars[1].symbol()}});

        Device::FullAddressSettings x0, y0, r;
        x0.value = State::Value::fromVariant(200);
        y0.value = State::Value::fromVariant(200);
        r.value = State::Value::fromVariant(100);
        ar1->setParameterMapping({
                        {syms[1].get_name().c_str(), {syms[1], x0}},
                        {syms[3].get_name().c_str(), {syms[3], y0}},
                        {syms[4].get_name().c_str(), {syms[4], r}},
                    });

        addArea(ar1);
    }

    {
        AreaParser parser("xv + yv >= c");

        auto ar2 = new AreaModel(parser.result(), *m_space, Id<AreaModel>(1), this);
        const auto& syms = ar2->area().symbols();

        ar2->setSpaceMapping({{syms[0], space_vars[0].symbol()},
                              {syms[1], space_vars[1].symbol()}});


        Device::FullAddressSettings c;
        c.value = State::Value::fromVariant(300);

        ar2->setParameterMapping({
                        {syms[2].get_name().c_str(), {syms[2], c}}
                    });

        addArea(ar2);
    }

    {
        addArea(new CircleAreaModel(*m_space,Id<AreaModel>(2), this));
    }
    */

    setDuration(duration);
}

ProcessModel *ProcessModel::clone(
        const Id<Process::ProcessModel> &newId,
        QObject *newParent) const
{
    auto& doc = iscore::IDocument::documentContext(*newParent);
    return new ProcessModel{doc, this->duration(), newId, newParent};
}

const ProcessFactoryKey& ProcessModel::key() const
{
    return ProcessMetadata::factoryKey();
}

QString ProcessModel::prettyName() const
{
    return tr("Space process");
}

void ProcessModel::setDurationAndScale(const TimeValue &newDuration)
{
    setDuration(newDuration);
    ISCORE_TODO;
}

void ProcessModel::setDurationAndGrow(const TimeValue &newDuration)
{
    setDuration(newDuration);
    ISCORE_TODO;
}

void ProcessModel::setDurationAndShrink(const TimeValue &newDuration)
{
    setDuration(newDuration);
    ISCORE_TODO;
}

void ProcessModel::reset()
{
    ISCORE_TODO;
}

ProcessStateDataInterface* ProcessModel::startStateData() const
{
    ISCORE_TODO;
    return nullptr;
}

ProcessStateDataInterface *ProcessModel::endStateData() const
{
    ISCORE_TODO;
    return nullptr;
}

Selection ProcessModel::selectableChildren() const
{
    ISCORE_TODO;
    return {};
}

Selection ProcessModel::selectedChildren() const
{
    ISCORE_TODO;
    return {};
}

void ProcessModel::setSelection(const Selection &s) const
{
    ISCORE_TODO;
}

void ProcessModel::serialize(const VisitorVariant &vis) const
{
    ISCORE_TODO;
}


Process::LayerModel *ProcessModel::makeLayer_impl(
        const Id<Process::LayerModel> &viewModelId,
        const QByteArray &constructionData,
        QObject *parent)
{
    return new LayerModel{viewModelId, *this, parent};
}

Process::LayerModel *ProcessModel::loadLayer_impl(
        const VisitorVariant &,
        QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}

Process::LayerModel *ProcessModel::cloneLayer_impl(
        const Id<Process::LayerModel> &newId,
        const Process::LayerModel &source,
        QObject *parent)
{
    ISCORE_TODO;
    return nullptr;
}


void ProcessModel::startExecution()
{
}

void ProcessModel::stopExecution()
{
    // Reset everything to the default values.
}
}
