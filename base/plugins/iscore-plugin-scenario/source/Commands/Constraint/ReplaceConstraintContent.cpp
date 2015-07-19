#include "ReplaceConstraintContent.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QJsonDocument>

using namespace iscore;
using namespace Scenario::Command;

ReplaceConstraintContent::ReplaceConstraintContent(QJsonObject&& sourceConstraint,
                                             ObjectPath&& targetConstraint,
                                             ExpandMode mode) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_source{sourceConstraint},
    m_target{targetConstraint},
    m_mode{mode}
{
    auto& trg_constraint = m_target.find<ConstraintModel>();
    ConstraintModel src_constraint{
            Deserializer<JSONObject>{m_source},
            &trg_constraint}; // Temporary parent

    // For all rackes in source, generate new id's
    auto target_rackes = trg_constraint.racks();
    QVector<id_type<RackModel>> target_rackes_ids;
    std::transform(target_rackes.begin(), target_rackes.end(),
                   std::back_inserter(target_rackes_ids),
                   [] (const auto& rack) { return rack.id(); });

    for(const auto& rack : src_constraint.racks())
    {
        auto newId = getStrongId(target_rackes_ids);
        m_rackIds.insert(rack.id(), newId);
        target_rackes_ids.append(newId);
    }

    // Same for processes
    auto target_processes = trg_constraint.processes();
    QVector<id_type<ProcessModel>> target_processes_ids;
    std::transform(target_processes.begin(), target_processes.end(),
                   std::back_inserter(target_processes_ids),
                   [] (const auto& proc) { return proc.id(); });

    for(const auto& proc : src_constraint.processes())
    {
        auto newId = getStrongId(target_processes_ids);
        m_processIds.insert(proc.id(), newId);
        target_processes_ids.append(newId);
    }
}

void ReplaceConstraintContent::undo()
{
    // We just have to remove what we added
    auto& trg_constraint = m_target.find<ConstraintModel>();

    for(const auto& proc_id : m_processIds)
    {
        trg_constraint.removeProcess(proc_id);
    }

    for(const auto& rack_id : m_rackIds)
    {
        trg_constraint.removeRack(rack_id);
    }
}


void ReplaceConstraintContent::redo()
{
    auto& trg_constraint = m_target.find<ConstraintModel>();
    ConstraintModel src_constraint{
            Deserializer<JSONObject>{m_source},
            &trg_constraint}; // Temporary parent

    std::map<const ProcessModel*, ProcessModel*> processPairs;

    // Clone the processes
    auto src_procs = src_constraint.processes();
    for(const auto& sourceproc : src_procs)
    {
        auto newproc = sourceproc.clone(m_processIds[sourceproc.id()], &trg_constraint);

        processPairs.insert(std::make_pair(&sourceproc, newproc));
        trg_constraint.addProcess(newproc);

        // Resize the processes according to the new constraint.
        if(m_mode == ExpandMode::Scale)
        {
            newproc->setDurationAndScale(trg_constraint.defaultDuration());
        }
        else if (m_mode == ExpandMode::Grow)
        {
            newproc->setDurationAndGrow(trg_constraint.defaultDuration());
        }
    }

    // Clone the rackes
    auto src_rackes = src_constraint.racks();
    for(const auto& sourcerack: src_rackes)
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
                    for(const auto& lm : source.layerModels())
                    {
                        // We can safely reuse the same id since it's in a different slot.
                        auto proc = processPairs[&lm.sharedProcessModel()];
                        // TODO harmonize the order of parameters (source first, then new id)
                        target.addLayerModel(proc->cloneLayer(lm.id(), lm, &target));
                    }
                },
                &trg_constraint};
        trg_constraint.addRack(newrack);
    }
}

void ReplaceConstraintContent::serializeImpl(QDataStream& s) const
{
    s << m_source << m_target << m_rackIds << m_processIds << (int) m_mode;
}

void ReplaceConstraintContent::deserializeImpl(QDataStream& s)
{
    int mode;
    s >> m_source >> m_target >> m_rackIds >> m_processIds >> mode;
    m_mode = static_cast<ExpandMode>(mode);
}
