#include "ResizeBaseConstraint.hpp"

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <Scenario/Commands/Scenario/Deletions/ClearConstraint.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Process/LayerModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
using namespace iscore;
using namespace Scenario::Command;

MoveBaseEvent::MoveBaseEvent(
        Path<BaseScenario>&& baseScenarioPath,
        const TimeValue& date,
        ExpandMode mode) :
    m_path {std::move(baseScenarioPath)},
    m_newDate {date},
    m_mode{mode}
{
    auto& scenar = m_path.find();
    const auto& constraint = scenar.baseConstraint();
    m_oldDate = constraint.duration.defaultDuration();

    // Save the constraint data
    QByteArray arr;
    Visitor<Reader<DataStream>> jr{&arr};
    jr.readFrom(constraint);

    // Save for each view model of this constraint
    // the identifier of the rack that was displayed
    QMap<Id<ConstraintViewModel>, Id<RackModel>> map;
    for(const ConstraintViewModel* vm : constraint.viewModels())
    {
        map[vm->id()] = vm->shownRack();
    }

    m_savedConstraint = {arr, map};
}

template<typename ScaleFun>
static void updateDuration(
        BaseScenario& scenar,
        const TimeValue& newDuration,
        ScaleFun&& scaleMethod)
{
    auto& timeNode = scenar.endTimeNode();
    timeNode.setDate(newDuration);
    scenar.endEvent().setDate(timeNode.date());

    auto& constraint = scenar.baseConstraint();
    ConstraintDurations::Algorithms::setDurationInBounds(constraint, newDuration);
    for(auto& process : constraint.processes)
    {
        scaleMethod(process, newDuration);
    }
}

void MoveBaseEvent::undo() const
{
    auto& scenar = m_path.find();

    updateDuration(scenar,
              m_oldDate,
              [&] (Process& p , const TimeValue& v) {
        // Nothing is needed since the processes will be replaced anyway.
    });


    // TODO do this only if we shrink.

    // Now we have to restore the state of each constraint that might have been modified
    // during this command.

    // 1. Clear the constraint
    ClearConstraint clearCmd{scenar.baseConstraint()};
    clearCmd.redo();

    auto& constraint = scenar.baseConstraint();
    // 2. Restore the rackes & processes.

    // TODO if possible refactor this with ReplaceConstraintContent and ConstraintModel::clone
    // Be careful however, the code differs in subtle ways
    ConstraintModel src_constraint{
        Deserializer<DataStream>{m_savedConstraint.first},
        &constraint}; // Temporary parent

    std::map<const Process*, Process*> processPairs;

    // Clone the processes
    for(const auto& sourceproc : src_constraint.processes)
    {
        auto newproc = sourceproc.clone(sourceproc.id(), &constraint);

        processPairs.insert(std::make_pair(&sourceproc, newproc));
        constraint.processes.add(newproc);
    }

    // Clone the rackes
    for(const auto& sourcerack : src_constraint.racks)
    {
        // A note about what happens here :
        // Since we want to duplicate our process view models using
        // the target constraint's cloned shared processes (they might setup some specific data),
        // we maintain a pair mapping each original process to their cloned counterpart.
        // We can then use the correct cloned process to clone the process view model.
        auto newrack = new RackModel{
                sourcerack,
                sourcerack.id(),
                [&] (const SlotModel& source, SlotModel& target)
        {
            for(const auto& lm : source.layers)
            {
                // We can safely reuse the same id since it's in a different slot.
                Process* proc = processPairs[&lm.processModel()];
                // TODO harmonize the order of parameters (source first, then new id)
                target.layers.add(proc->cloneLayer(lm.id(), lm, &target));
            }
        },
        &constraint};
        constraint.racks.add(newrack);
    }


    // 3. Restore the correct rackes in the constraint view models
    for(auto& viewmodel : constraint.viewModels())
    {
        viewmodel->showRack(m_savedConstraint.second[viewmodel->id()]);
    }
}

void MoveBaseEvent::redo() const
{
    auto& scenar = m_path.find();

    updateDuration(scenar,
              m_newDate,
              [&] (Process& p , const TimeValue& v) {
        p.expandProcess(m_mode, v);
    });
}

void MoveBaseEvent::serializeImpl(DataStreamInput& s) const
{
    s << m_path
      << m_oldDate
      << m_newDate
      << (int)m_mode
      << m_savedConstraint;
}

void MoveBaseEvent::deserializeImpl(DataStreamOutput& s)
{
    int mode;
    s >> m_path
            >> m_oldDate
            >> m_newDate
            >> mode
            >> m_savedConstraint;
    m_mode = static_cast<ExpandMode>(mode);
}
