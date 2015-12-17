#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include "ConstraintViewModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

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
    if(m_shownRack)
    {
        m_shownRack.unset();
        emit rackHidden();
    }
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

void ConstraintViewModel::on_rackRemoval(const RackModel& rack)
{
    if(shownRack() == rack.id())
    {
        // There is only one rack left and it is
        // being removed
        if(m_model.racks.size() == 1)
        {
            emit lastRackRemoved();
        }
        else
        {
            hideRack();
        }
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

ConstraintViewModel::~ConstraintViewModel()
{
    emit aboutToBeDeleted(this);
}

const ConstraintModel& ConstraintViewModel::model() const
{
    return m_model;
}
