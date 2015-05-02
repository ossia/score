#include "MoveEvent.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include "Commands/Scenario/Deletions/ClearConstraint.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include <ProcessInterface/ProcessViewModelInterface.hpp>
using namespace iscore;
using namespace Scenario::Command;

MoveEvent::MoveEvent(ObjectPath&& scenarioPath,
                     id_type<EventModel> eventId,
                     const TimeValue& date,
                     double height,
                     ExpandMode mode) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath)},
    m_eventId {eventId},
    m_newHeightPosition {height},
    m_newDate {date},
    m_mode{mode}
{
    auto scenar = m_path.find<ScenarioModel>();
    const auto& movedEvent = scenar->event(m_eventId);
    m_oldHeightPosition = movedEvent.heightPercentage();
    m_oldDate = movedEvent.date();

    StandardDisplacementPolicy::getRelatedElements(*scenar,
                                                   scenar->event(m_eventId).timeNode(),
                                                   m_movableTimenodes);

    // 1. Make a list of the constraints that need to be resized
    QSet<id_type<ConstraintModel>> constraints;
    for(const auto& tn_id : m_movableTimenodes)
    {
        const auto& tn = scenar->timeNode(tn_id);
        for(const auto& ev_id : tn.events())
        {
            constraints += scenar->event(ev_id).constraints().toList().toSet();
        }
    }

    // 2. Save them
    for(const auto& cst_id : constraints)
    {
        const auto& constraint = scenar->constraint(cst_id);

        // Save the constraint data
        QByteArray arr;
        Visitor<Reader<DataStream>> jr{&arr};
        jr.readFrom(constraint);

        // Save for each view model of this constraint
        // the identifier of the box that was displayed
        QMap<id_type<AbstractConstraintViewModel>, id_type<BoxModel>> map;
        for(const AbstractConstraintViewModel* vm : constraint.viewModels())
        {
            map[vm->id()] = vm->shownBox();
        }

        m_savedConstraints.push_back({{iscore::IDocument::path(constraint), arr}, map});
    }
}

void MoveEvent::undo()
{
    auto scenar = m_path.find<ScenarioModel>();
    auto& event = scenar->event(m_eventId);

    event.setHeightPercentage(m_oldHeightPosition);
    StandardDisplacementPolicy::updatePositions(
                *scenar,
                m_movableTimenodes,
                m_oldDate - event.date(),
                [&] (ProcessSharedModelInterface* , const TimeValue& ) {  });

    // Now we have to restore the state of each constraint that might have been modified
    // during this command.
    for(auto& obj : m_savedConstraints)
    {
        // 1. Clear the constraint
        auto cmd1 = new ClearConstraint{
                    ObjectPath{obj.first.first}};
        cmd1->redo();

        ConstraintModel* constraint = obj.first.first.find<ConstraintModel>();
        // 2. Restore the boxes & processes.
        // TODO if possible refactor this with CopyConstraintContent
        // Be careful however, the code differs in subtle ways
        {
            ConstraintModel src_constraint{
                    Deserializer<DataStream>{obj.first.second},
                    constraint}; // Temporary parent

            std::map<const ProcessSharedModelInterface*, ProcessSharedModelInterface*> processPairs;

            // Clone the processes
            auto src_procs = src_constraint.processes();
            for(auto i = src_procs.size(); i --> 0; )
            {
                auto sourceproc = src_procs[i];
                auto newproc = sourceproc->clone(sourceproc->id(), constraint);

                processPairs.insert(std::make_pair(sourceproc, newproc));
                constraint->addProcess(newproc);
            }

            // Clone the boxes
            auto src_boxes = src_constraint.boxes();
            for(auto i = src_boxes.size(); i --> 0; )
            {
                // A note about what happens here :
                // Since we want to duplicate our process view models using
                // the target constraint's cloned shared processes (they might setup some specific data),
                // we maintain a pair mapping each original process to their cloned counterpart.
                // We can then use the correct cloned process to clone the process view model.
                auto newbox = new BoxModel{
                        *src_boxes[i],
                        src_boxes[i]->id(),
                        [&] (DeckModel& source, DeckModel& target)
                        {
                            for(const auto& pvm : source.processViewModels())
                            {
                                // We can safely reuse the same id since it's in a different deck.
                                ProcessSharedModelInterface* proc = processPairs[&pvm->sharedProcessModel()];
                                // TODO harmonize the order of parameters (source first, then new id)
                                target.addProcessViewModel(proc->cloneViewModel(pvm->id(), *pvm, &target));
                            }
                        },
                        constraint};
                constraint->addBox(newbox);
            }
        }

        // 3. Restore the correct boxes in the constraint view models
        for(auto& viewmodel : constraint->viewModels())
        {
            viewmodel->showBox(obj.second[viewmodel->id()]);
        }
    }
}

void MoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    auto& event = scenar->event(m_eventId);

    event.setHeightPercentage(m_newHeightPosition);
    StandardDisplacementPolicy::updatePositions(
                *scenar,
                m_movableTimenodes,
                m_newDate - event.date(),
                [&] (ProcessSharedModelInterface* p, const TimeValue& t)
    { p->expandProcess(m_mode, t); });
}

void MoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_eventId
      << m_oldHeightPosition
      << m_newHeightPosition
      << m_oldDate
      << m_newDate
      << m_movableTimenodes
      << (int)m_mode
      << m_savedConstraints;
}

void MoveEvent::deserializeImpl(QDataStream& s)
{
    int mode;
    s >> m_path
      >> m_eventId
      >> m_oldHeightPosition
      >> m_newHeightPosition
      >> m_oldDate
      >> m_newDate
      >> m_movableTimenodes
      >> mode
      >> m_savedConstraints;

    m_mode = static_cast<ExpandMode>(mode);
}
