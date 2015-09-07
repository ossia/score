#include "RackPresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/RackView.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <QGraphicsScene>

static const constexpr int slotSpacing = 0;

RackPresenter::RackPresenter(const RackModel& model,
                           RackView* view,
                           QObject* parent):
    NamedObject {"RackPresenter", parent},
    m_model {model},
    m_view {view}
{
    for(const auto& slotModel : m_model.slotmodels)
    {
        on_slotCreated_impl(slotModel);
    }

    m_duration = m_model.constraint().duration.defaultDuration();

    on_askUpdate();

    con(m_model.slotmodels, &NotifyingMap<SlotModel>::added,
        this, &RackPresenter::on_slotCreated);
    con(m_model.slotmodels, &NotifyingMap<SlotModel>::removed,
        this, &RackPresenter::on_slotRemoved);
    con(m_model, &RackModel::slotPositionsChanged,
        this, &RackPresenter::on_slotPositionsChanged);

    con(m_model, &RackModel::on_durationChanged,
        this, &RackPresenter::on_durationChanged);
}

RackPresenter::~RackPresenter()
{
    deleteGraphicsObject(m_view);
}

const RackView &RackPresenter::view() const
{
    return *m_view;
}

qreal RackPresenter::height() const
{
    qreal totalHeight = 0; // No slot -> not visible ? or just "add a process" button ? Bottom bar ? How to make it visible ?

    for(const auto& slot : slotmodels)
    {
        totalHeight += slot.height() + slotSpacing;
    }

    return totalHeight;
}

qreal RackPresenter::width() const
{
    return m_view->boundingRect().width();
}

void RackPresenter::setWidth(qreal w)
{
    m_view->setWidth(w);

    for(auto& slot : slotmodels)
    {
        slot.setWidth(m_view->boundingRect().width());
    }
}

const Id<RackModel>& RackPresenter::id() const
{
    return m_model.id();
}

void RackPresenter::setDisabledSlotState()
{
    for(auto& slot : slotmodels)
    {
        slot.disable();
    }
}

void RackPresenter::setEnabledSlotState()
{
    for(auto& slot : slotmodels)
    {
        slot.enable();
    }
}

void RackPresenter::on_durationChanged(const TimeValue& duration)
{
    m_duration = duration;
    on_askUpdate();
}

void RackPresenter::on_slotCreated(const SlotModel& slot)
{
    on_slotCreated_impl(slot);
    on_askUpdate();
}

void RackPresenter::on_slotCreated_impl(const SlotModel& slotModel)
{
    auto slotPres = new SlotPresenter {slotModel,
                                       m_view,
                                       this};
    slotmodels.insert(slotPres);
    slotPres->on_zoomRatioChanged(m_zoomRatio);

    connect(slotPres, &SlotPresenter::askUpdate,
            this,     &RackPresenter::on_askUpdate);

    connect(slotPres, &SlotPresenter::pressed, this, &RackPresenter::pressed);
    connect(slotPres, &SlotPresenter::moved, this, &RackPresenter::moved);
    connect(slotPres, &SlotPresenter::released, this, &RackPresenter::released);


    // Set the correct view for the slot graphically if we're in a scenario
    auto scenario = dynamic_cast<TemporalScenarioPresenter*>(this->parent()->parent());
    if(!scenario)
        return;

    if(scenario->stateMachine().tool() == ScenarioToolKind::MoveSlot)
    {
        slotPres->disable();
    }
}

void RackPresenter::on_slotRemoved(const SlotModel& slot_model)
{
    auto slot = &slotmodels.at(slot_model.id());

    slotmodels.remove(slot_model.id());
    delete slot;

    on_askUpdate();
}

void RackPresenter::updateShape()
{
    using namespace std;
    // Vertical shape
    m_view->setHeight(height());

    // Set the slots position graphically in order.
    int currentSlotY = 0;

    for(const auto& slotId : m_model.slotsPositions())
    {
        auto& slotPres = slotmodels.at(slotId);
        slotPres.setWidth(width());
        slotPres.setVerticalPosition(currentSlotY);
        currentSlotY += slotPres.height() + slotSpacing; // Separation between slots
    }

    // Horizontal shape
    setWidth(m_duration.toPixels(m_zoomRatio));

    for(auto& slot : slotmodels)
    {
        slot.on_parentGeometryChanged();
    }
}

void RackPresenter::on_askUpdate()
{
    updateShape();
    emit askUpdate();
}

void RackPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    on_askUpdate();

    // We have to change the width of the slots aftewards
    // because their width depend on the rack width
    // TODO this smells.
    for(auto& slot : slotmodels)
    {
        slot.on_zoomRatioChanged(m_zoomRatio);
    }
}

void RackPresenter::on_slotPositionsChanged()
{
    updateShape();
}
