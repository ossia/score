#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"
#include "Area/AreaParser.hpp"
#include "Space/SpaceModel.hpp"
#include "Area/Circle/CircleAreaModel.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
namespace Space
{


ProcessExecutor::ProcessExecutor(ProcessModel& process):
m_process{process}
{

}


std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue&,
        const OSSIA::TimeValue&)
{
    auto& devlist = m_process.context().devices.list();

    // TODO for solving collisions, do a "==" on the two equation ?
    // For each area whose parameters depend on an address,
    // get the current area value and update it.
    for(AreaModel& area : m_process.areas())
    {
        const auto& parameter_map = area.parameterMapping();
        GiNaC::exmap mapping;

        for(const auto& elt : parameter_map)
        {
            // We always set the default value just in case.
            auto it_pair = mapping.insert(
                               std::make_pair(
                                   elt.first,
                                   iscore::convert::value<double>(elt.second.value)
                                   )
                               );

            auto& addr = elt.second.address;

            if(!addr.device.isEmpty())
            {
                // We fetch it from the device tree
                auto dev_it = devlist.find(addr.device);
                if(dev_it != devlist.devices().end())
                {
                    auto val = (*dev_it)->refresh(addr);
                    if(val)
                    {
                       it_pair.first->second = iscore::convert::value<double>(*val);
                    }
                }
            }
        }

        area.setCurrentMapping(mapping);
    }

    // Send the parameters of each area
    // (variables's value ? default computations (like diameter, etc. ?))
    // For each computation, send the new state.

    return OSSIA::State::create();
}

ProcessModel::ProcessModel(
        const iscore::DocumentContext& doc,
        const TimeValue &duration,
        const Id<Process::ProcessModel> &id,
        QObject *parent):
    RecreateOnPlay::OSSIAProcessModel{id, ProcessMetadata::processObjectName(), parent},
    m_space{new SpaceModel{
            Id<SpaceModel>(0),
            this}},
    m_context{doc, *m_space, doc.plugin<DeviceDocumentPlugin>()},
    m_process{std::make_shared<Space::ProcessExecutor>(*this)}
{
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

        iscore::FullAddressSettings x0, y0, r;
        x0.value = iscore::Value::fromVariant(200);
        y0.value = iscore::Value::fromVariant(200);
        r.value = iscore::Value::fromVariant(100);
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


        iscore::FullAddressSettings c;
        c.value = iscore::Value::fromVariant(300);

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


std::shared_ptr<TimeProcessWithConstraint> ProcessModel::process() const
{
    return m_process;
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

void ProcessModel::addArea(AreaModel* a)
{
    m_areas.insert(a);

    emit areaAdded(*a);
}

void ProcessModel::removeArea(const Id<AreaModel> &id)
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


void ProcessModel::addComputation(ComputationModel * c)
{
    m_computations.insert(c);

    emit computationAdded(*c);
}


void ProcessModel::startExecution()
{
}

void ProcessModel::stopExecution()
{
}
}
