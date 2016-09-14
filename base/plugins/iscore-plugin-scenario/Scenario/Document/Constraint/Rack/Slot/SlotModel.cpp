#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <algorithm>
#include <iterator>

#include <iscore/model/ModelMetadata.hpp>
#include "SlotModel.hpp"
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/Todo.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>

namespace Scenario
{
SlotModel::SlotModel(const Id<SlotModel>& id,
        const qreal slotHeight,
        RackModel* parent) :
    Entity{id, Metadata<ObjectKey_k, SlotModel>::get(), parent}
{
    m_height = slotHeight;
    initConnections();
    metadata().setName(QString{"Slot.%1"}.arg(id.val()));
}

SlotModel::SlotModel(
        std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
        const SlotModel& source,
        const Id<SlotModel>& id,
        RackModel *parent):
    Entity{source, id, Metadata<ObjectKey_k, SlotModel>::get(), parent},
    m_frontLayerModelId{source.m_frontLayerModelId},
    m_height {source.getHeight() }
{
    initConnections();
    lmCopyMethod(source, *this);

    // Note: we have a small trick for the layer model id.
    // Since we're cloning, we want the pointer cached in the layer model to be the
    // one we have cloned, hence instead of just copying the id, we ask the corresponding
    // layer model to give us its id.
    // TODO this is fucking ugly - mostly because two objects exist with the same id...
    metadata().setName(QString{"Slot.%1"} .arg(id.val()));
}

RackModel&SlotModel::rack() const
{ return *static_cast<RackModel*>(parent()); }

void SlotModel::copyViewModelsInSameConstraint(
        const SlotModel &source,
        SlotModel &target)
{
    auto& procs = iscore::AppContext().components.factory<Process::ProcessList>();

    for(const auto& lm : source.layers)
    {
        // We can safely reuse the same id since it's in a different slot.
        auto& proc = lm.processModel();
        auto fact = procs.get(proc.concreteFactoryKey());

        target.layers.add(
                    fact->cloneLayer(
                        proc, lm.id(), lm, &target));
    }
}

void SlotModel::on_addLayer(const Process::LayerModel& viewmodel)
{
    putToFront(viewmodel.id());
}

void SlotModel::on_removeLayer(
        const Process::LayerModel&)
{
    if(!layers.empty())
    {
        putToFront((*layers.begin()).id());
    }
    else
    {
        m_frontLayerModelId = iscore::none;
    }
}

void SlotModel::putToFront(
        const Id<Process::LayerModel>& id)
{
    if(!id.val())
        return;

    if(id != m_frontLayerModelId)
    {
        auto lay = layers.find(id);
        if(lay != layers.end())
        {
            m_frontLayerModelId = id;
            emit layerModelPutToFront(*lay);
        }
    }
}

const Process::LayerModel* SlotModel::frontLayerModel() const
{
    if(!m_frontLayerModelId)
        return nullptr;
    return &layers.at(*m_frontLayerModelId);
}

void SlotModel::on_deleteSharedProcessModel(
        const Process::ProcessModel& proc)
{
    using namespace std;
    auto it = find_if(begin(layers),
                      end(layers),
                      [id = proc.id()](const Process::LayerModel& lm)
    {
        return lm.processModel().id() == id;
    });

    if(it != end(layers))
    {
        layers.remove((*it).id());
    }
}

void SlotModel::setHeight(qreal arg)
{
    if(m_height != arg)
    {
        m_height = arg;
        emit HeightChanged(arg);
    }
}

void SlotModel::setFocus(bool arg)
{
    if (m_focus == arg)
        return;

    m_focus = arg;
    emit focusChanged(arg);
}

void SlotModel::initConnections()
{
    layers.added.connect<SlotModel,&SlotModel::on_addLayer>(this);
    layers.removed.connect<SlotModel,&SlotModel::on_removeLayer>(this);
}

ConstraintModel& SlotModel::parentConstraint() const
{
    return static_cast<ConstraintModel&>(*parent()->parent());
}

qreal SlotModel::getHeight() const
{
    return m_height;
}

bool SlotModel::focus() const
{
    return m_focus;
}
}
