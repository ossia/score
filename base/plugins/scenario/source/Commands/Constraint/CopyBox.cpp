#include "CopyBox.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyConstraintContent::CopyConstraintContent(QJsonObject&& sourceConstraint,
                                             ObjectPath&& targetConstraint) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_source{sourceConstraint},
    m_target{targetConstraint}
{
    auto trg_constraint = m_target.find<ConstraintModel>();
    auto src_constraint = new ConstraintModel{
            Deserializer<JSONObject>{m_source},
            trg_constraint}; // Temporary parent

    // For all boxes in source, generate new id's
    auto target_boxes = trg_constraint->boxes();
    for(auto i = src_constraint->boxes().size(); i --> 0; )
    {
        m_boxIds.push_back(getStrongId(target_boxes));
    }

    // Same for processes
    auto target_processes = trg_constraint->processes();
    for(auto i = src_constraint->processes().size(); i --> 0; )
    {
        m_processIds.push_back(getStrongId(target_processes));
    }

    delete src_constraint;
}

void CopyConstraintContent::undo()
{
    // We just have to remove what we added
    auto trg_constraint = m_target.find<ConstraintModel>();

    for(auto& proc_id : m_processIds)
    {
        trg_constraint->removeProcess(proc_id);
    }

    for(auto& box_id : m_boxIds)
    {
        trg_constraint->removeBox(box_id);
    }
}


void CopyConstraintContent::redo()
{
    auto trg_constraint = m_target.find<ConstraintModel>();
    auto src_constraint = new ConstraintModel{
            Deserializer<JSONObject>{m_source},
            trg_constraint}; // Temporary parent

    std::map<ProcessSharedModelInterface*, ProcessSharedModelInterface*> processPairs;

    // Clone the processes
    auto src_procs = src_constraint->processes();
    for(auto i = src_procs.size(); i --> 0; )
    {
        auto sourceproc = src_procs[i];
        auto newproc = sourceproc->clone(m_processIds[i], trg_constraint);

        processPairs.insert(std::make_pair(sourceproc, newproc));
        trg_constraint->addProcess(newproc);
    }

    // Clone the boxes
    auto src_boxes = src_constraint->boxes();
    for(auto i = src_boxes.size(); i --> 0; )
    {
        // A note about what happens here :
        // Since we want to duplicate our process view models using
        // the target constraint's cloned shared processes (they might setup some specific data),
        // we maintain a pair mapping each original process to their cloned counterpart.
        // We can then use the correct cloned process to clone the process view model.
        auto newbox = new BoxModel{
                src_boxes[i],
                m_boxIds[i],
                [&] (DeckModel& source, DeckModel& target)
                {
                    for(auto& pvm : source.processViewModels())
                    {
                        // We can safely reuse the same id since it's in a different deck.
                        auto proc = processPairs[pvm->sharedProcessModel()];
                        // TODO harmonize the order of parameters (source first, then new id)
                        target.addProcessViewModel(proc->makeViewModel(pvm->id(), pvm, &target));
                    }
                },
                trg_constraint};
        trg_constraint->addBox(newbox);

    }
}

bool CopyConstraintContent::mergeWith(const Command* other)
{
    return false;
}

#include <QJsonDocument>
void CopyConstraintContent::serializeImpl(QDataStream& s) const
{
    QJsonDocument doc{m_source};

    s << doc.toBinaryData() << m_target << m_boxIds << m_processIds;
}

void CopyConstraintContent::deserializeImpl(QDataStream& s)
{
    QByteArray arr;
    s >> arr >> m_target >> m_boxIds >> m_processIds;

    m_source = QJsonDocument::fromBinaryData(arr).object();
}
