#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "InsertContentInConstraint.hpp"
#include <Process/ExpandMode.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

using namespace iscore;
using namespace Scenario::Command;

InsertContentInConstraint::InsertContentInConstraint(
        QJsonObject&& sourceConstraint,
        Path<ConstraintModel>&& targetConstraint,
        ExpandMode mode) :
    m_source{sourceConstraint},
    m_target{targetConstraint},
    m_mode{mode}
{
    auto& trg_constraint = m_target.find();
    ConstraintModel src_constraint{
            Deserializer<JSONObject>{m_source},
            &trg_constraint}; // Temporary parent

    // For all rackes in source, generate new id's
    // TODO we can use SettableIdentifierGeneration here.
    const auto& target_rackes = trg_constraint.racks;
    std::vector<Id<RackModel>> target_rackes_ids;
    target_rackes_ids.reserve(target_rackes.size());
    std::transform(target_rackes.begin(), target_rackes.end(),
                   std::back_inserter(target_rackes_ids),
                   [] (const auto& rack) { return rack.id(); });

    for(const auto& rack : src_constraint.racks)
    {
        auto newId = getStrongId(target_rackes_ids);
        m_rackIds.insert(rack.id(), newId);
        target_rackes_ids.push_back(newId);
    }

    // Same for processes
    const auto& target_processes = trg_constraint.processes;
    std::vector<Id<Process>> target_processes_ids;
    target_processes_ids.reserve(target_processes.size());
    std::transform(target_processes.begin(), target_processes.end(),
                   std::back_inserter(target_processes_ids),
                   [] (const auto& proc) { return proc.id(); });

    for(const auto& proc : src_constraint.processes)
    {
        auto newId = getStrongId(target_processes_ids);
        m_processIds.insert(proc.id(), newId);
        target_processes_ids.push_back(newId);
    }
}

void InsertContentInConstraint::undo() const
{
    // We just have to remove what we added
    auto& trg_constraint = m_target.find();

    for(const auto& proc_id : m_processIds)
    {
        RemoveProcess(trg_constraint, proc_id);
    }

    for(const auto& rack_id : m_rackIds)
    {
        trg_constraint.racks.remove(rack_id);
    }
}


void InsertContentInConstraint::redo() const
{
    auto& trg_constraint = m_target.find();
    ConstraintModel src_constraint{
            Deserializer<JSONObject>{m_source},
            &trg_constraint}; // Temporary parent

    std::map<const Process*, Process*> processPairs;

    // Clone the processes
    const auto& src_procs = src_constraint.processes;
    for(const auto& sourceproc : src_procs)
    {
        auto newproc = sourceproc.clone(m_processIds[sourceproc.id()], &trg_constraint);

        processPairs.insert(std::make_pair(&sourceproc, newproc));
        AddProcess(trg_constraint, newproc);

        // Resize the processes according to the new constraint.
        if(m_mode == ExpandMode::Scale)
        {
            newproc->setParentDuration(ExpandMode::Scale, trg_constraint.duration.defaultDuration());
        }
        else if (m_mode == ExpandMode::GrowShrink)
        {
            newproc->setParentDuration(ExpandMode::ForceGrow, trg_constraint.duration.defaultDuration());
        }
    }

    // Clone the rackes
    const auto& src_racks = src_constraint.racks;
    for(const auto& sourcerack: src_racks)
    {
        // A note about what happens here :
        // Since we want to duplicate our process view models using
        // the target constraint's cloned shared processes (they might setup some specific data),
        // we maintain a pair mapping each original process to their cloned counterpart.
        // We can then use the correct cloned process to clone the process view model.
        auto newrack = new RackModel{
                sourcerack,
                m_rackIds[sourcerack.id()],
                [&] (const SlotModel& source, SlotModel& target)
                {
                    for(const auto& lm : source.layers)
                    {
                        // We can safely reuse the same id since it's in a different slot.
                        auto proc = processPairs[&lm.processModel()];
                        // TODO harmonize the order of parameters (source first, then new id)
                        target.layers.add(proc->cloneLayer(lm.id(), lm, &target));
                    }
                },
                &trg_constraint};
        trg_constraint.racks.add(newrack);
    }
}

void InsertContentInConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_source << m_target << m_rackIds << m_processIds << (int) m_mode;
}

void InsertContentInConstraint::deserializeImpl(DataStreamOutput& s)
{
    int mode;
    s >> m_source >> m_target >> m_rackIds >> m_processIds >> mode;
    m_mode = static_cast<ExpandMode>(mode);
}
