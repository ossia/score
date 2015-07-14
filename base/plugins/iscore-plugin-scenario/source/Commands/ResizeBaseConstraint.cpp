#include "ResizeBaseConstraint.hpp"

#include "Document/BaseElement/BaseScenario/BaseScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include "Commands/Scenario/Deletions/ClearConstraint.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/VerticalMovePolicy.hpp"
#include <ProcessInterface/LayerModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
using namespace iscore;
using namespace Scenario::Command;

MoveBaseEvent::MoveBaseEvent(ObjectPath&& baseScenarioPath,
                             const TimeValue& date,
                             ExpandMode mode) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(baseScenarioPath)},
    m_newDate {date},
    m_mode{mode}
{
    auto& scenar = m_path.find<BaseScenario>();
    const auto& constraint = scenar.baseConstraint();
    m_oldDate = constraint.defaultDuration();

    // Save the constraint data
    QByteArray arr;
    Visitor<Reader<DataStream>> jr{&arr};
    jr.readFrom(constraint);

    // Save for each view model of this constraint
    // the identifier of the rack that was displayed
    QMap<id_type<ConstraintViewModel>, id_type<RackModel>> map;
    for(const ConstraintViewModel* vm : constraint.viewModels())
    {
        map[vm->id()] = vm->shownRack();
    }

    m_savedConstraint = {arr, map};
}

template<typename ScaleFun>
static void updateDuration(BaseScenario& scenar, const TimeValue& newDuration, ScaleFun&& scaleMethod)
{
    auto& timeNode = scenar.endTimeNode();
    timeNode.setDate(newDuration);
    scenar.endEvent().setDate(timeNode.date());

    auto& constraint = scenar.baseConstraint();
    ConstraintModel::Algorithms::setDurationInBounds(constraint, newDuration);
    for(const auto& process : constraint.processes())
    {
        scaleMethod(process, newDuration);
    }
}

void MoveBaseEvent::undo()
{
    auto& scenar = m_path.find<BaseScenario>();

    updateDuration(scenar,
              m_oldDate,
              [&] (ProcessModel* p , const TimeValue& v) {
        // Nothing is needed since the processes will be replaced anyway.
    });


    // TODO do this only if we shrink.

    // Now we have to restore the state of each constraint that might have been modified
    // during this command.

    // 1. Clear the constraint
    // TODO check if there is some new ClearConstraint leaking somewhere else
    ClearConstraint clearCmd{iscore::IDocument::path(scenar.baseConstraint())};
    clearCmd.redo();

    auto& constraint = scenar.baseConstraint();
    // 2. Restore the rackes & processes.

    // TODO if possible refactor this with CopyConstraintContent and ConstraintModel::clone
    // Be careful however, the code differs in subtle ways
    ConstraintModel src_constraint{
        Deserializer<DataStream>{m_savedConstraint.first},
        &constraint}; // Temporary parent

    std::map<const ProcessModel*, ProcessModel*> processPairs;

    // Clone the processes
    for(const auto& sourceproc : src_constraint.processes())
    {
        auto newproc = sourceproc->clone(sourceproc->id(), &constraint);

        processPairs.insert(std::make_pair(sourceproc, newproc));
        constraint.addProcess(newproc);
    }

    // Clone the rackes
    for(const auto& sourcerack : src_constraint.racks())
    {
        // A note about what happens here :
        // Since we want to duplicate our process view models using
        // the target constraint's cloned shared processes (they might setup some specific data),
        // we maintain a pair mapping each original process to their cloned counterpart.
        // We can then use the correct cloned process to clone the process view model.
        auto newrack = new RackModel{
                *sourcerack,
                sourcerack->id(),
                [&] (const SlotModel& source, SlotModel& target)
        {
            for(const auto& lm : source.layerModels())
            {
                // We can safely reuse the same id since it's in a different slot.
                ProcessModel* proc = processPairs[&lm->sharedProcessModel()];
                // TODO harmonize the order of parameters (source first, then new id)
                target.addLayerModel(proc->cloneLayer(lm->id(), *lm, &target));
            }
        },
        &constraint};
        constraint.addRack(newrack);
    }


    // 3. Restore the correct rackes in the constraint view models
    for(auto& viewmodel : constraint.viewModels())
    {
        viewmodel->showRack(m_savedConstraint.second[viewmodel->id()]);
    }
}

void MoveBaseEvent::redo()
{
    auto& scenar = m_path.find<BaseScenario>();

    updateDuration(scenar,
              m_newDate,
              [&] (ProcessModel* p , const TimeValue& v) {
        p->expandProcess(m_mode, v);
    });
}

void MoveBaseEvent::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_oldDate
      << m_newDate
      << (int)m_mode
      << m_savedConstraint;
}

void MoveBaseEvent::deserializeImpl(QDataStream& s)
{
    int mode;
    s >> m_path
            >> m_oldDate
            >> m_newDate
            >> mode
            >> m_savedConstraint;
    m_mode = static_cast<ExpandMode>(mode);
}
