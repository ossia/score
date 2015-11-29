#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackView.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <QObject>
#include <QRect>

#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include "RackPresenter.hpp"
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/Todo.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

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

    for(const auto& slot : m_slots)
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

    for(auto& slot : m_slots)
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

void RackPresenter::on_slotCreated(const SlotModel& slot)
{
    on_slotCreated_impl(slot);
    on_askUpdate();
}

void RackPresenter::on_slotCreated_impl(const SlotModel& slotModel)
{
    auto& context = iscore::IDocument::documentContext(slotModel);
    auto slotPres = new SlotPresenter {context, slotModel, m_view, this};

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

    if(scenario->editionSettings().tool() == Scenario::Tool::MoveSlot)
    {
        slotPres->disable();
    }
}

void RackPresenter::on_slotRemoved(const SlotModel& slot_model)
{
    auto& slot = m_slots.at(slot_model.id());

    m_slots.remove(slot_model.id());
    delete &slot;

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
        currentSlotY += slotPres.height() + slotSpacing; // Separation between slots
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
