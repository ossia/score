#include "AbstractScenarioViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

AbstractConstraintViewModel* AbstractScenarioViewModel::constraint (id_type<AbstractConstraintViewModel> constraintViewModelid) const
{
    return findById (m_constraints, constraintViewModelid);
}

QVector<AbstractConstraintViewModel*> AbstractScenarioViewModel::constraints() const
{
    return m_constraints;
}


void AbstractScenarioViewModel::removeConstraintViewModel (id_type<AbstractConstraintViewModel> constraintViewModelId)
{
    // We have to emit before, because on removal, some other stuff might use the now-removed model id to do the comparison in vec_erase_remove_if
    emit constraintViewModelRemoved (constraintViewModelId);
    removeById (m_constraints, constraintViewModelId);

}
