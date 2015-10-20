#include "ShowRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

using namespace iscore;
using namespace Scenario::Command;

ShowRackInViewModel::ShowRackInViewModel(
        Path<ConstraintViewModel>&& constraint_path,
        const Id<RackModel>& rackId) :
    SerializableCommand{factoryName(),
                        commandName(),
                        description()},
    m_constraintViewPath {std::move(constraint_path) },
    m_rackId {rackId}
{
    auto& vm = m_constraintViewPath.find();
    m_previousRackId = vm.shownRack();
}

ShowRackInViewModel::ShowRackInViewModel(
        const ConstraintViewModel& vm,
        const Id<RackModel>& rackId) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_constraintViewPath {vm},
    m_rackId {rackId}
{
    m_previousRackId = vm.shownRack();
}

void ShowRackInViewModel::undo() const
{
    auto& vm = m_constraintViewPath.find();

    // TODO unnecessary
    if(m_previousRackId.val())
    {
        vm.showRack(m_previousRackId);
    }
    else
    {
        vm.hideRack();
    }
}

void ShowRackInViewModel::redo() const
{
    auto& vm = m_constraintViewPath.find();
    vm.showRack(m_rackId);
}

void ShowRackInViewModel::serializeImpl(QDataStream& s) const
{
    s << m_constraintViewPath
      << m_rackId
      << m_previousRackId;
}

void ShowRackInViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_constraintViewPath
            >> m_rackId
            >> m_previousRackId;
}



#include <Document/Constraint/ConstraintModel.hpp>
ShowRackInAllViewModels::ShowRackInAllViewModels(
        Path<ConstraintModel>&& constraint_path,
        const Id<RackModel>& rackId) :
    SerializableCommand{factoryName(),
                        commandName(),
                        description()},
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
