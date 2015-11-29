#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <boost/core/explicit_operator_bool.hpp>

#include "ConstraintViewModel.hpp"
#include "iscore/tools/IdentifiedObject.hpp"

class QObject;

bool ConstraintViewModel::isRackShown() const
{
    return bool (m_shownRack.val());
}

const Id<RackModel>& ConstraintViewModel::shownRack() const
{
    return m_shownRack;
}

void ConstraintViewModel::hideRack()
{
    m_shownRack.unset();
    emit rackHidden();
}

void ConstraintViewModel::showRack(const Id<RackModel>& rackId)
{
    if(rackId.val().is_initialized())
    {
        m_shownRack = rackId;

        emit rackShown(m_shownRack);
    }
    else
    {
        hideRack();
    }
}

void ConstraintViewModel::on_rackRemoved(const RackModel& rack)
{
    if(shownRack() == rack.id())
    {
        hideRack();
        emit rackRemoved();
    }
}


ConstraintViewModel::ConstraintViewModel(
        const Id<ConstraintViewModel>& id,
        const QString& name,
        const ConstraintModel& model,
        QObject* parent) :
    IdentifiedObject<ConstraintViewModel> {id, name, parent},
    m_model {model}
{
}

const ConstraintModel& ConstraintViewModel::model() const
{
    return m_model;
}
