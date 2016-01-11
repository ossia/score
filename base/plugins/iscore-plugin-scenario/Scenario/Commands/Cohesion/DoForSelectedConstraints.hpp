#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
template<typename Fun>
void DoForSelectedConstraints(
        const iscore::DocumentContext& doc,
        Fun f)
{
    using namespace std;

    // Fetch the selected constraints
    auto selected_constraints = filterSelectionByType<ConstraintModel>(doc.selectionStack.currentSelection());

    if(selected_constraints.empty())
        return;

    f(selected_constraints, doc.commandStack);
}
}
