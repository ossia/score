#include "RackPresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/RackView.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

#include <iscore/command/SerializableCommand.hpp>
#include <QGraphicsScene>

RackPresenter::RackPresenter(const RackModel& model,
                           RackView* view,
                           QObject* parent):
    NamedObject {"RackPresenter", parent},
    m_model {model},
    m_view {view}
{
    for(const auto& slotModel : m_model.getSlots())
    {
        on_slotCreated_impl(slotModel);
    }

    m_duration = m_model.constraint().duration.defaultDuration();

    on_askUpdate();

    connect(&m_model, &RackModel::slotCreated,
            this, &RackPresenter::on_slotCreated);
    connect(&m_model,&RackModel::slotRemoved,
            this, &RackPresenter::on_slotRemoved);
    connect(&m_model, &RackModel::slotPositionsChanged,
            this, &RackPresenter::on_slotPositionsChanged);

    connect(&m_model, &RackModel::on_durationChanged,
            this, &RackPresenter::on_durationChanged);
}

RackPresenter::~RackPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

const RackView &RackPresenter::view() const
{
    return *m_view;
}

qreal RackPresenter::height() const
{
    qreal totalHeight = 0; // No slot -> not visible ? or just "add a process" button ? Bottom bar ? How to make it visible ?

    for(const auto& slot : m_slots)
    {
        totalHeight += slot.height() + 5.;
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

    for(auto& slot : m_slots)
    {
        slot.setWidth(m_view->boundingRect().width());
    }
}

const id_type<RackModel>& RackPresenter::id() const
{
    return m_model.id();
}

void RackPresenter::setDisabledSlotState()
{
    for(auto& slot : m_slots)
    {
        slot.disable();
    }
}

void RackPresenter::setEnabledSlotState()
{
    for(auto& slot : m_slots)
    {
        slot.enable();
    }
}

void RackPresenter::on_durationChanged(const TimeValue& duration)
{
    m_duration = duration;
    on_askUpdate();
}

void RackPresenter::on_slotCreated(const id_type<SlotModel>& slotId)
{
    on_slotCreated_impl(m_model.slot(slotId));
    on_askUpdate();
}

#include "Process/Temporal/TemporalScenarioPresenter.hpp"
void RackPresenter::on_slotCreated_impl(const SlotModel& slotModel)
{
    auto slotPres = new SlotPresenter {slotModel,
                                       m_view,
                                       this};
    m_slots.insert(slotPres);
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

    if(scenario->stateMachine().tool() == Tool::MoveSlot)
    {
        slotPres->disable();
    }
}

void RackPresenter::on_slotRemoved(const id_type<SlotModel>& slotId)
{
    auto slot = &m_slots.at(slotId);

    delete slot;
    m_slots.remove(slotId);

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
        auto& slotPres = m_slots.at(slotId);
        slotPres.setWidth(width());
        slotPres.setVerticalPosition(currentSlotY);
        currentSlotY += slotPres.height() + 5; // Separation between slots
    }

    // Horizontal shape
    setWidth(m_duration.toPixels(m_zoomRatio));

    for(auto& slot : m_slots)
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
    for(auto& slot : m_slots)
    {
        slot.on_zoomRatioChanged(m_zoomRatio);
    }
}

void RackPresenter::on_slotPositionsChanged()
{
    updateShape();
}
