#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/optional/optional.hpp>
#include <algorithm>
#include <iterator>

#include "AbstractScenarioLayerModel.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/utilsCPP11.hpp>

namespace Scenario
{
ConstraintViewModel& AbstractScenarioLayerModel::constraint(
        const Id<ConstraintViewModel>& id) const
{
    auto it = std::find(
                std::begin(m_constraints),
                std::end(m_constraints),
                id);
    if(it != std::end(m_constraints))
    {
        return **it;
    }

    ISCORE_ABORT;
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
        const Id<ConstraintViewModel>& constraintViewModelId)
{
    // We have to emit before, because on removal,
    // some other stuff might use the now-removed model id
    // to do the comparison in vec_erase_remove_if

    using namespace std;
    auto it = find_if(begin(m_constraints),
                      end(m_constraints),
                      [&] (ConstraintViewModel* vm)
    {
        return vm->id() == constraintViewModelId;
    });
    ISCORE_ASSERT(it != end(m_constraints));

    emit constraintViewModelRemoved(**it);
    removeById(m_constraints, constraintViewModelId);

}

ConstraintViewModel& AbstractScenarioLayerModel::constraint(
        const Id<ConstraintModel>& constraintModelId) const
{
    using namespace std;
    auto it = find_if(begin(m_constraints),
                      end(m_constraints),
                      [&] (ConstraintViewModel* vm)
    {
        return vm->model().id() == constraintModelId;
    });

    ISCORE_ASSERT(it != end(m_constraints));
    return **it;
}

void createConstraintViewModels(const ConstraintViewModelIdMap& idMap,
                                const Id<ConstraintModel>& constraintId,
                                const Scenario::ScenarioModel& scenario)
{
    // Creation of all the constraint view models
    for(auto& viewModel : layers(scenario))
    {
        auto lm_id = iscore::IDocument::path(*viewModel);

        if(idMap.contains(lm_id))
        {
            viewModel->makeConstraintViewModel(constraintId,
                                               idMap[lm_id]);
        }
        else
        {
           ISCORE_ABORT;
        }
    }
}

std::vector<ConstraintViewModel*> getConstraintViewModels(
        const Id<ConstraintModel>& constraintId,
        const Scenario::ScenarioModel& scenario)
{
    const auto& lays = layers(scenario);

    std::vector<ConstraintViewModel*> vec;
    vec.reserve(lays.size());

    // Creation of all the constraint view models
    for(auto viewModel : lays)
    {
        vec.push_back(&viewModel->constraint(constraintId));
    }

    return vec;
}
}
