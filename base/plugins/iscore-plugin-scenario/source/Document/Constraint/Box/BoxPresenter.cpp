#include "BoxPresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/Slot/SlotPresenter.hpp"
#include "Document/Constraint/Box/Slot/SlotView.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

#include <iscore/command/SerializableCommand.hpp>
#include <QGraphicsScene>

BoxPresenter::BoxPresenter(const BoxModel& model,
                           BoxView* view,
                           QObject* parent):
    NamedObject {"BoxPresenter", parent},
    m_model {model},
    m_view {view}
{
    for(const auto& slotModel : m_model.getSlots())
    {
        on_slotCreated_impl(*slotModel);
    }

    m_duration = m_model.constraint().defaultDuration();
    m_view->setText(m_model.constraint().metadata.name());

    on_askUpdate();

    connect(&m_model, &BoxModel::slotCreated,
            this, &BoxPresenter::on_slotCreated);
    connect(&m_model,&BoxModel::slotRemoved,
            this, &BoxPresenter::on_slotRemoved);
    connect(&m_model, &BoxModel::slotPositionsChanged,
            this, &BoxPresenter::on_slotPositionsChanged);

    connect(&m_model, &BoxModel::on_durationChanged,
            this, &BoxPresenter::on_durationChanged);

    connect(&m_model.constraint().metadata, &ModelMetadata::nameChanged,
            this, [&] (const QString& name) { m_view->setText(name); });
}

BoxPresenter::~BoxPresenter()
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

const BoxView &BoxPresenter::view() const
{
    return *m_view;
}

int BoxPresenter::height() const
{
    int totalHeight = 25; // No slot -> not visible ? or just "add a process" button ? Bottom bar ? How to make it visible ?

    for(auto& slot : m_slots)
    {
        totalHeight += slot->height() + 5;
    }

    return totalHeight;
}

int BoxPresenter::width() const
{
    return m_view->boundingRect().width();
}

void BoxPresenter::setWidth(int w)
{
    m_view->setWidth(w);

    for(const auto& slot : m_slots)
    {
        slot->setWidth(m_view->boundingRect().width());
    }
}

const id_type<BoxModel>& BoxPresenter::id() const
{
    return m_model.id();
}

void BoxPresenter::setDisabledSlotState()
{
    for(const auto& slot : m_slots)
    {
        slot->disable();
    }
}

void BoxPresenter::setEnabledSlotState()
{
    for(const auto& slot : m_slots)
    {
        slot->enable();
    }
}

void BoxPresenter::on_durationChanged(const TimeValue& duration)
{
    m_duration = duration;
    on_askUpdate();
}

void BoxPresenter::on_slotCreated(const id_type<SlotModel>& slotId)
{
    on_slotCreated_impl(*m_model.slot(slotId));
    on_askUpdate();
}

#include "Process/Temporal/TemporalScenarioPresenter.hpp"
void BoxPresenter::on_slotCreated_impl(const SlotModel& slotModel)
{
    auto slotPres = new SlotPresenter {slotModel,
                                       m_view,
                                       this};
    m_slots.insert(slotPres);
    slotPres->on_zoomRatioChanged(m_zoomRatio);

    connect(slotPres, &SlotPresenter::askUpdate,
            this,     &BoxPresenter::on_askUpdate);

    connect(slotPres, &SlotPresenter::pressed, this, &BoxPresenter::pressed);
    connect(slotPres, &SlotPresenter::moved, this, &BoxPresenter::moved);
    connect(slotPres, &SlotPresenter::released, this, &BoxPresenter::released);


    // Set the correct view for the slot graphically if we're in a scenario
    auto scenario = dynamic_cast<TemporalScenarioPresenter*>(this->parent()->parent());
    if(!scenario)
        return;

    if(scenario->stateMachine().tool() == Tool::MoveSlot)
    {
        slotPres->disable();
    }
}

void BoxPresenter::on_slotRemoved(const id_type<SlotModel>& slotId)
{
    auto slot = m_slots.at(slotId);

    delete slot;
    m_slots.remove(slotId);

    on_askUpdate();
}

void BoxPresenter::updateShape()
{
    using namespace std;
    // Vertical shape
    m_view->setHeight(height());

    // Set the slots position graphically in order.
    int currentSlotY = 20;

    for(auto& slotId : m_model.slotsPositions())
    {
        auto slotPres = m_slots.at(slotId);
        slotPres->setWidth(width());
        slotPres->setVerticalPosition(currentSlotY);
        currentSlotY += slotPres->height() + 5; // Separation between slots
    }

    // Horizontal shape
    setWidth(m_duration.toPixels(m_zoomRatio));

    for(SlotPresenter* slot : m_slots)
    {
        slot->on_parentGeometryChanged();
    }
}

void BoxPresenter::on_askUpdate()
{
    updateShape();
    emit askUpdate();
}

void BoxPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    on_askUpdate();

    // We have to change the width of the slots aftewards
    // because their width depend on the box width
    // TODO this smells.
    for(SlotPresenter* slot : m_slots)
    {
        slot->on_zoomRatioChanged(m_zoomRatio);
    }
}

void BoxPresenter::on_slotPositionsChanged()
{
    updateShape();
}
