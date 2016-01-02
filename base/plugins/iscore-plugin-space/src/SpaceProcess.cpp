#include "SpaceProcess.hpp"
#include "SpaceLayerModel.hpp"
#include "Area/AreaParser.hpp"
#include "Space/SpaceModel.hpp"
#include "Area/Circle/CircleAreaModel.hpp"
#include "Area/Pointer/PointerAreaModel.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Editor/Message.h>

namespace Space
{
template<typename K>
struct KeyPair : std::pair<K, K>
{
    public:
        using std::pair<K, K>::pair;

        friend bool operator==(
                const KeyPair& lhs,
                const KeyPair& rhs)
        {
            return (lhs.first == rhs.first && lhs.second == rhs.second)
                    ||
                   (lhs.first == rhs.second && lhs.second == rhs.first);
        }

        friend bool operator!=(
                const KeyPair& lhs,
                const KeyPair& rhs)
        {
            return !(lhs == rhs);
        }
};
template<typename K>
auto make_keys(const K& k1, const K& k2)
{
    return KeyPair<K>{k1, k2};
}

using CollisionFun = std::function<bool(const AreaModel& a1, const AreaModel& a2)>;

class CollisionHandler
{
        std::map<KeyPair<AreaFactoryKey>, CollisionFun> m_handlers;
    public:
        CollisionHandler()
        {
            m_handlers.insert(
                        std::make_pair(
                        make_keys(
                                CircleAreaModel::static_factoryKey(),
                                CircleAreaModel::static_factoryKey()),
                              [] (const AreaModel& a1, const AreaModel& a2)
            {
                auto& c1 = static_cast<const CircleAreaModel&>(a1);
                auto& c2 = static_cast<const CircleAreaModel&>(a2);

                auto c1_val = c1.mapToData(c1.currentMapping(), c1.parameterMapping());
                auto c2_val = c2.mapToData(c2.currentMapping(), c2.parameterMapping());

                // Check if the distance of both centers is < to the sum of radiuses
                auto dist = [] (auto v1, auto v2) {
                    return std::sqrt(std::pow(v2.x - v1.x, 2) + std::pow(v2.y - v1.y, 2));
                };
                return dist(c1_val, c2_val) < (c1_val.r + c2_val.r);
            }));

            m_handlers.insert(
                        std::make_pair(
                        make_keys(
                                CircleAreaModel::static_factoryKey(),
                                PointerAreaModel::static_factoryKey()),
                              [] (const AreaModel& a1, const AreaModel& a2)
            {
                auto& c = static_cast<const CircleAreaModel&>(a1);
                auto& p = static_cast<const PointerAreaModel&>(a2);

                auto c_val = c.mapToData(c.currentMapping(), c.parameterMapping());
                auto p_val = p.mapToData(p.currentMapping(), p.parameterMapping());

                // Check if the distance of both centers is < to the sum of radiuses
                auto dist = [] (auto v1, auto v2) {
                    return std::sqrt(std::pow(v2.x - v1.x, 2) + std::pow(v2.y - v1.y, 2));
                };
                return dist(c_val, p_val) < c_val.r;
            }));
        }

        void inscribe(std::pair<KeyPair<AreaFactoryKey>, CollisionFun> val)
        {
            m_handlers.insert(val);
        }

        bool check(const AreaModel& a1, const AreaModel& a2)
        {
            auto it = m_handlers.find(make_keys(a1.factoryKey(), a2.factoryKey()));
            if(it != m_handlers.end())
            {
                return it->second(a1, a2);
            }

            // TODO return a generic computation

            // TODO for solving collisions, put the equations in a system ?
            return false;
        }
};

void makeState(
        OSSIA::State& state,
        const AreaModel& ar)
{
    // Display all variables of the equation

    // Then display custom variables.

}

void makeState(
        OSSIA::State& state,
        const ComputationModel& comp)
{
    // Name and result
    // auto mess = OSSIA::Message::create()

}


ProcessExecutor::ProcessExecutor(ProcessModel& process):
m_process{process}
{

}


std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue&,
        const OSSIA::TimeValue&)
{
    auto& devlist = m_process.context().devices.list();


    // For each area whose parameters depend on an address,
    // get the current area value and update it.
    for(AreaModel& area : m_process.areas)
    {
        const auto& parameter_map = area.parameterMapping();
        GiNaC::exmap mapping;

        for(const auto& elt : parameter_map)
        {
            // We always set the default value just in case.
            auto it_pair = mapping.insert(
                               std::make_pair(
                                   elt.first,
                                   State::convert::value<double>(elt.second.value)
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
                       it_pair.first->second = State::convert::value<double>(*val);
                    }
                }
            }
        }

        area.setCurrentMapping(mapping);
    }


    auto state = OSSIA::State::create();
    // State of each area
    // Shall be done in the "tree" component.
    /*
    for(const AreaModel& area : m_process.areas())
    {
        makeState(*state, area);
    }
    */


    // Shall be done either here, or in the tree component : choose between reactive, and state mode.
    // Same problem for "mapping" plug-in : react to changes or return state ?
    // "Reactive" execution component (has to be enabled / disabled on start / end)
    // vs "state" execution component
    // Handle computations / collisions
    for(const auto& comp : m_process.computations)
    {
        makeState(*state, comp);
        /*
        CollisionHandler h;
        for(const AreaModel& area_lhs : m_process.areas())
        {
            for(const AreaModel& area_rhs : m_process.areas())
            {
                if(&area_rhs != &area_lhs)
                {
                    h.check(area_lhs, area_rhs);
                }
            }
        }
        */

    }

    // Send the parameters of each area
    // (variables's value ? default computations (like diameter, etc. ?))
    // For each computation, send the new state.

    return state;
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
}
}
