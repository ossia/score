#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include "ConstraintViewModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;

namespace Scenario
{
bool ConstraintViewModel::isRackShown() const
{
    return bool (m_shownRack);
}

const OptionalId<RackModel>& ConstraintViewModel::shownRack() const
{
    return m_shownRack;
}

void ConstraintViewModel::hideRack()
{
    if(m_shownRack)
    {
        m_shownRack = iscore::none;
        emit rackHidden();
    }
}

void ConstraintViewModel::showRack(const OptionalId<RackModel>& rackId)
{
    if(rackId)
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
        ConstraintModel& model,
        QObject* parent) :
    IdentifiedObject<ConstraintViewModel> {id, name, parent},
    m_model {model}
{
}

ConstraintViewModel::~ConstraintViewModel()
{
    emit aboutToBeDeleted(this);
}

ConstraintModel& ConstraintViewModel::model() const
{
    return m_model;
}
}
