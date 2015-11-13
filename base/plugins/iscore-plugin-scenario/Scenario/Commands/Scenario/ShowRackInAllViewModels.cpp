#include "ShowRackInAllViewModels.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

ShowRackInAllViewModels::ShowRackInAllViewModels(
        Path<ConstraintModel>&& constraint_path,
        const Id<RackModel>& rackId) :
    m_constraintPath {std::move(constraint_path)},
    m_rackId{rackId}
{
    auto cst = m_constraintPath.try_find();
    if(cst)
    {
        for(const auto& vm : cst->viewModels())
        {
            m_previousRacks.insert(vm->id(), vm->shownRack());
        }
    }
}

void ShowRackInAllViewModels::undo() const
{
    auto& cst = m_constraintPath.find();

    const auto& vms = cst.viewModels();
    for(const auto& vm: vms)
    {
        if(m_previousRacks.contains(vm->id()))
        {
            vm->showRack(m_previousRacks[vm->id()]);
        }
    }
}

void ShowRackInAllViewModels::redo() const
{
    auto& cst = m_constraintPath.find();

    const auto& vms = cst.viewModels();
    for(const auto& vm: vms)
    {
        vm->showRack(m_rackId);
    }
}

void ShowRackInAllViewModels::serializeImpl(QDataStream& s) const
{
    s << m_constraintPath
      << m_rackId
      << m_previousRacks;
}

void ShowRackInAllViewModels::deserializeImpl(QDataStream& s)
{
    s >> m_constraintPath
            >> m_rackId
            >> m_previousRacks;
}
