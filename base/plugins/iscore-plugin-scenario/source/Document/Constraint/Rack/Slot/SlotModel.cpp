#include "SlotModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"

#include "ProcessInterface/Process.hpp"
#include "ProcessInterface/LayerModel.hpp"

constexpr const char SlotModel::className[];

SlotModel::SlotModel(
        const Id<SlotModel>& id,
        RackModel* parent) :
    IdentifiedObject<SlotModel> {id, "SlotModel", parent}
{
    initConnections();
}

SlotModel::SlotModel(
        std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
        const SlotModel& source,
        const Id<SlotModel>& id,
        RackModel *parent):
    IdentifiedObject<SlotModel> {id, "SlotModel", parent},
    m_frontLayerModelId {source.m_frontLayerModelId}, // Keep the same id.
    m_height {source.height() }
{
    initConnections();
    lmCopyMethod(source, *this);
}

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

const LayerModel& SlotModel::frontLayerModel() const
{
    return layers.at(m_frontLayerModelId);
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
