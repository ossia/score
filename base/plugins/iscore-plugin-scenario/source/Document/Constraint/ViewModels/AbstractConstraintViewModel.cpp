#include "AbstractConstraintViewModel.hpp"

bool AbstractConstraintViewModel::isRackShown() const
{
    return bool (m_shownRack.val());
}

const id_type<RackModel>& AbstractConstraintViewModel::shownRack() const
{
    return m_shownRack;
}

void AbstractConstraintViewModel::hideRack()
{
    m_shownRack.unset();
    emit rackHidden();
}

void AbstractConstraintViewModel::showRack(const id_type<RackModel>& rackId)
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

void AbstractConstraintViewModel::on_rackRemoved(const id_type<RackModel>& rackId)
{
    if(shownRack() == rackId)
    {
        hideRack();
        emit rackRemoved();
    }
}


AbstractConstraintViewModel::AbstractConstraintViewModel(
        const id_type<AbstractConstraintViewModel>& id,
        const QString& name,
        const ConstraintModel& model,
        QObject* parent) :
    IdentifiedObject<AbstractConstraintViewModel> {id, name, parent},
    m_model {model}
{
}

const ConstraintModel& AbstractConstraintViewModel::model() const
{
    return m_model;
}
