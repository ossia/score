#include "AbstractScenarioViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

AbstractConstraintViewModel& AbstractScenarioViewModel::constraint(
        const id_type<AbstractConstraintViewModel>& constraintViewModelid) const
{
    return *findById(m_constraints, constraintViewModelid);
}

QVector<AbstractConstraintViewModel*> AbstractScenarioViewModel::constraints() const
{
    return m_constraints;
}


void AbstractScenarioViewModel::removeConstraintViewModel(
        const id_type<AbstractConstraintViewModel>& constraintViewModelId)
{
    // We have to emit before, because on removal,
    // some other stuff might use the now-removed model id
    // to do the comparison in vec_erase_remove_if
    emit constraintViewModelRemoved(constraintViewModelId);
    removeById(m_constraints, constraintViewModelId);

}

AbstractConstraintViewModel& AbstractScenarioViewModel::constraint(
        const id_type<ConstraintModel>& constraintModelId) const
{
    using namespace std;
    auto it = find_if(begin(m_constraints),
                      end(m_constraints),
                      [&] (AbstractConstraintViewModel* vm)
    {
        return vm->model().id() == constraintModelId;
    });

    Q_ASSERT(it != end(m_constraints));
    return **it;
}

#include "Process/ScenarioModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
void createConstraintViewModels(const ConstraintViewModelIdMap& idMap,
                                const id_type<ConstraintModel>& constraintId,
                                const ScenarioModel& scenario)
{
    // Creation of all the constraint view models
    for(auto& viewModel : viewModels(scenario))
    {
        auto pvm_id = iscore::IDocument::path(viewModel);

        if(idMap.contains(pvm_id))
        {
            viewModel->makeConstraintViewModel(constraintId,
                                               idMap[pvm_id]);
        }
        else
        {
            throw std::runtime_error("createConstraintViewModels : missing identifier.");
        }
    }
}
