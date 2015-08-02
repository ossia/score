#include "AbstractScenarioLayerModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

// TODO removeme
template<typename Container, typename id_T>
typename Container::value_type findById(const Container& c, const id_T& id)
{
    auto it = std::find(std::begin(c), std::end(c), id);
    if(it != std::end(c))
    {
        return *it;
    }

    throw std::runtime_error(QString("findById : id %1 not found in vector of %2").arg(*id.val()).arg(typeid(c).name()).toLatin1().constData());
}

ConstraintViewModel& AbstractScenarioLayerModel::constraint(
        const id_type<ConstraintViewModel>& constraintViewModelid) const
{
    return *findById(m_constraints, constraintViewModelid);
}

QVector<ConstraintViewModel*> AbstractScenarioLayerModel::constraints() const
{
    return m_constraints;
}


template <typename Vector, typename id_T>
void removeById(Vector& c, const id_T& id)
{
    vec_erase_remove_if(c,
                        [&id](typename Vector::value_type model)
    {
        bool to_delete = model->id() == id;

        if(to_delete)
        {
            delete model;
        }

        return to_delete;
    });
}

void AbstractScenarioLayerModel::removeConstraintViewModel(
        const id_type<ConstraintViewModel>& constraintViewModelId)
{
    // We have to emit before, because on removal,
    // some other stuff might use the now-removed model id
    // to do the comparison in vec_erase_remove_if
    emit constraintViewModelRemoved(constraintViewModelId);
    removeById(m_constraints, constraintViewModelId);

}

ConstraintViewModel& AbstractScenarioLayerModel::constraint(
        const id_type<ConstraintModel>& constraintModelId) const
{
    using namespace std;
    auto it = find_if(begin(m_constraints),
                      end(m_constraints),
                      [&] (ConstraintViewModel* vm)
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
    for(auto& viewModel : layers(scenario))
    {
        auto lm_id = iscore::IDocument::path(viewModel);

        if(idMap.contains(lm_id))
        {
            viewModel->makeConstraintViewModel(constraintId,
                                               idMap[lm_id]);
        }
        else
        {
           Q_ASSERT_X(false, "createConstraintViewModels", "missing identifier.");
        }
    }
}
