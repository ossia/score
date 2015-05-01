#include "AbstractConstraintViewModel.hpp"

bool AbstractConstraintViewModel::isBoxShown() const
{
    return bool (m_shownBox.val());
}

const id_type<BoxModel>& AbstractConstraintViewModel::shownBox() const
{
    return m_shownBox;
}

void AbstractConstraintViewModel::hideBox()
{
    m_shownBox.unset();
    emit boxHidden();
}

void AbstractConstraintViewModel::showBox(const id_type<BoxModel>& boxId)
{
    if(boxId.val().is_initialized())
    {
        m_shownBox = boxId;

        emit boxShown(m_shownBox);
    }
    else
    {
        hideBox();
    }
}

void AbstractConstraintViewModel::on_boxRemoved(const id_type<BoxModel>& boxId)
{
    if(shownBox() == boxId)
    {
        hideBox();
        emit boxRemoved();
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
