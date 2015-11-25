#include "SlotModel.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>

constexpr const char SlotModel::className[];

SlotModel::SlotModel(
        const Id<SlotModel>& id,
        RackModel* parent) :
    IdentifiedObject<SlotModel> {id, "SlotModel", parent}
{
    initConnections();
    metadata.setName(QString{"Slot.%1"}.arg(*id.val()));
}

SlotModel::SlotModel(
        std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
        const SlotModel& source,
        const Id<SlotModel>& id,
        RackModel *parent):
    IdentifiedObject<SlotModel> {id, "SlotModel", parent},
    m_frontLayerModelId{Id<LayerModel>{source.m_frontLayerModelId.val()}},
    m_height {source.height() }
{
    initConnections();
    lmCopyMethod(source, *this);

    // Note: we have a small trick for the layer model id.
    // Since we're cloning, we want the pointer cached in the layer model to be the
    // one we have cloned, hence instead of just copying the id, we ask the corresponding
    // layer model to give us its id.
    // TODO this is fucking ugly - mostly because two objects exist with the same id...
    metadata.setName(QString{"Slot.%1"} .arg(*id.val()));
}

RackModel&SlotModel::rack() const
{ return *static_cast<RackModel*>(parent()); }

void SlotModel::copyViewModelsInSameConstraint(
        const SlotModel &source,
        SlotModel &target)
{
    for(const auto& lm : source.layers)
    {
        // We can safely reuse the same id since it's in a different slot.
        auto& proc = lm.processModel();
        target.layers.add(
                    proc.cloneLayer(lm.id(),
                                    lm,
                                    &target));
    }
}

void SlotModel::on_addLayer(const LayerModel& viewmodel)
{
    putToFront(viewmodel.id());
}

void SlotModel::on_removeLayer(
        const LayerModel&)
{
    if(!layers.empty())
    {
        putToFront((*layers.begin()).id());
    }
    else
    {
        m_frontLayerModelId.setVal({});
    }
}

void SlotModel::putToFront(
        const Id<LayerModel>& id)
{
    if(!id.val())
        return;

    if(id != m_frontLayerModelId)
    {
        m_frontLayerModelId = id;
        emit layerModelPutToFront(layers.at(id));
    }
}

const LayerModel* SlotModel::frontLayerModel() const
{
    if(!m_frontLayerModelId)
        return nullptr;
    return &layers.at(m_frontLayerModelId);
}

void SlotModel::on_deleteSharedProcessModel(
        const Process& proc)
{
    using namespace std;
    auto it = find_if(begin(layers),
                      end(layers),
                      [id = proc.id()](const LayerModel& lm)
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
        emit heightChanged(arg);
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
    con(layers, &NotifyingMap<LayerModel>::added,
        this, &SlotModel::on_addLayer);
    con(layers, &NotifyingMap<LayerModel>::removed,
        this, &SlotModel::on_removeLayer);
}

ConstraintModel& SlotModel::parentConstraint() const
{
    return static_cast<ConstraintModel&>(*parent()->parent());
}

qreal SlotModel::height() const
{
    return m_height;
}

bool SlotModel::focus() const
{
    return m_focus;
}
