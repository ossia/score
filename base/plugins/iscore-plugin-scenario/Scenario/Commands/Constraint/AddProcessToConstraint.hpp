#pragma once
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "AddLayerInNewSlot.hpp"
#include "Rack/Slot/AddLayerModelToSlot.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Process/Process.hpp>

#include <Process/ProcessList.hpp>
#include <Process/ProcessFactory.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>


class Process;
class LayerModel;
class RackModel;
class SlotModel;
class ConstraintModel;
namespace Scenario
{
namespace Command
{
class AddLayerInNewSlot;
class AddLayerModelToSlot;

class AddProcessToConstraintBase : public iscore::SerializableCommand
{
    public:
        AddProcessToConstraintBase() = default;
        AddProcessToConstraintBase(
                const ConstraintModel& constraint,
                const ProcessFactoryKey& process) :
            m_path{constraint},
            m_processName{process},
            m_createdProcessId{getStrongId(constraint.processes)}
        {
        }

        const Path<ConstraintModel>& constraintPath() const
        { return m_path; }
        const Id<Process>& processId() const
        { return m_createdProcessId; }


    protected:
        Path<ConstraintModel> m_path;
        ProcessFactoryKey m_processName;
        Id<Process> m_createdProcessId;
};

template<typename AddProcessDelegate>
class AddProcessToConstraint final : public AddProcessToConstraintBase
{
        friend AddProcessDelegate;

    public:
        const CommandParentFactoryKey& parentKey() const override
        { return ScenarioCommandFactoryName(); }
        const CommandFactoryKey& key() const override
        { return static_key(); }
        QString description() const override
        { return QObject::tr("AddProcessToConstraint"); }
        static const CommandFactoryKey& static_key()
        { return AddProcessDelegate::static_key(); }

        AddProcessToConstraint() = default;

        AddProcessToConstraint(
                const ConstraintModel& constraint,
                const ProcessFactoryKey& process) :
            AddProcessToConstraintBase{constraint, process}
        {
            auto& fact = context.components.factory<DynamicProcessList>();
            m_delegate.init(fact, constraint);
        }

        void undo() const override
        {
            auto& constraint = m_path.find();

            m_delegate.undo(constraint);

            constraint.processes.remove(m_createdProcessId);
        }
        void redo() const override
        {
            auto& fact = context.components.factory<DynamicProcessList>();
            auto& constraint = m_path.find();

            // Create process model
            auto proc =
                    fact.list().get(m_processName)
                    ->makeModel(
                        constraint.duration.defaultDuration(),
                        m_createdProcessId,
                        &constraint);

            constraint.processes.add(proc);

            m_delegate.redo(constraint, *proc);
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_path
              << m_processName
              << m_createdProcessId;
            m_delegate.serialize(s);
        }
        void deserializeImpl(DataStreamOutput& s) override
        {
            s >> m_path
              >> m_processName
              >> m_createdProcessId;
            m_delegate.deserialize(s);
        }

    private:
        AddProcessDelegate m_delegate{*this};
};


class AddProcessDelegateWhenNoRacks
{
    private:
        using proc_t = AddProcessToConstraint<AddProcessDelegateWhenNoRacks>;
        proc_t& m_cmd;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegateWhenNoRacks"};
            return var;
        }

        AddProcessDelegateWhenNoRacks(proc_t& cmd):
            m_cmd{cmd}
        {

        }

        void init(const DynamicProcessList& fact, const ConstraintModel& constraint)
        {
            m_createdRackId = getStrongId(constraint.racks);
            m_createdSlotId = Id<SlotModel>(iscore::id_generator::getFirstId());
            m_createdLayerId = Id<LayerModel> (iscore::id_generator::getFirstId());
            m_layerConstructionData = fact.list().get(m_cmd.m_processName)->makeStaticLayerConstructionData();
        }

        void undo(ConstraintModel& constraint) const
        {
            auto& rack = constraint.racks.at(m_createdRackId);

            // Removing the slot will remove the layer
            rack.slotmodels.remove(m_createdSlotId);
            constraint.racks.remove(m_createdRackId);
        }

        void redo(ConstraintModel& constraint, Process& proc) const
        {
            // TODO refactor with AddRackToConstraint
            auto rack = new RackModel{m_createdRackId, &constraint};
            constraint.racks.add(rack);

            // If it is the first rack created,
            // it is also assigned to all the views of the constraint.
            if(constraint.racks.size() == 1)
            {
                for(const auto& vm : constraint.viewModels())
                {
                    vm->showRack(m_createdRackId);
                }
            }

            // Slot
            rack->addSlot(new SlotModel {m_createdSlotId,
                                         rack});

            // Process View
            auto& slot = rack->slotmodels.at(m_createdSlotId);

            slot.layers.add(proc.makeLayer(m_createdLayerId, m_layerConstructionData, &slot));
        }

        void serialize(DataStreamInput& s) const
        {
            s << m_createdRackId
              << m_createdSlotId
              << m_createdLayerId
              << m_layerConstructionData;
        }

        void deserialize(DataStreamOutput& s)
        {
            s >> m_createdRackId
              >> m_createdSlotId
              >> m_createdLayerId
              >> m_layerConstructionData;
        }

    private:
        Id<RackModel> m_createdRackId;
        Id<SlotModel> m_createdSlotId;
        Id<LayerModel> m_createdLayerId;
        QByteArray m_layerConstructionData;
};

class AddProcessDelegateWhenRacksAndNoBaseConstraint
{
    private:
        using proc_t = AddProcessToConstraint<AddProcessDelegateWhenRacksAndNoBaseConstraint>;
        proc_t& m_cmd;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegateWhenRacksAndNoBaseConstraint"};
            return var;
        }

        AddProcessDelegateWhenRacksAndNoBaseConstraint(proc_t& cmd):
            m_cmd{cmd}
        {

        }
        void init(const DynamicProcessList& fact, const ConstraintModel& constraint)
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            const auto& firstRack = *constraint.racks.begin();
            if(!firstRack.slotmodels.empty())
            {
                const auto& firstSlotModel = *firstRack.slotmodels.begin();

                m_layerConstructionData = fact.list().get(m_cmd.m_processName)->makeStaticLayerConstructionData();
                m_createdLayerId = getStrongId(firstSlotModel.layers);
            }
        }

        void undo(ConstraintModel& constraint) const
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            ISCORE_ASSERT(!(*constraint.racks.begin()).slotmodels.empty());

            auto& slot = *(*constraint.racks.begin()).slotmodels.begin();
            slot.layers.remove(m_createdLayerId);
        }

        void redo(ConstraintModel& constraint, Process& proc) const
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            ISCORE_ASSERT(!(*constraint.racks.begin()).slotmodels.empty());

            auto& slot = *(*constraint.racks.begin()).slotmodels.begin();
            slot.layers.add(proc.makeLayer(m_createdLayerId, m_layerConstructionData, &slot));
        }

        void serialize(DataStreamInput& s) const
        {
            s << m_createdLayerId
              << m_layerConstructionData;
        }

        void deserialize(DataStreamOutput& s)
        {
            s >> m_createdLayerId
              >> m_layerConstructionData;
        }

    private:
        Id<LayerModel> m_createdLayerId;
        QByteArray m_layerConstructionData;
};

class AddProcessDelegateWhenRacksAndBaseConstraint
{

        using proc_t = AddProcessToConstraint<AddProcessDelegateWhenRacksAndBaseConstraint>;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegateWhenRacksAndBaseConstraint"};
            return var;
        }

        AddProcessDelegateWhenRacksAndBaseConstraint(const proc_t&)
        {

        }

        void init(const DynamicProcessList& fact, const ConstraintModel& constraint)
        {
            // Base constraint : add in new slot?
        }

        void undo(ConstraintModel& constraint) const
        {

        }

        void redo(ConstraintModel& constraint, Process& proc) const
        {

        }

        void serialize(DataStreamInput& s) const
        {

        }

        void deserialize(DataStreamOutput& s)
        {

        }
};


inline iscore::SerializableCommand* make_AddProcessToConstraint(
        const ConstraintModel& constraint,
        const ProcessFactoryKey& process)
{
    auto notBaseConstraint = dynamic_cast<ScenarioModel*>(constraint.parent());
    auto noRackes = constraint.racks.empty() && notBaseConstraint;

    iscore::SerializableCommand* cmd{};
    if(noRackes)
    {
        cmd = new AddProcessToConstraint<AddProcessDelegateWhenNoRacks>{
                constraint, process};
    }
    else if(notBaseConstraint)
    {
        cmd = new AddProcessToConstraint<AddProcessDelegateWhenRacksAndNoBaseConstraint>{
                constraint, process};
    }
    else
    {
        cmd = new AddProcessToConstraint<AddProcessDelegateWhenRacksAndBaseConstraint>{
                constraint, process};
    }

    return cmd;
}
}
}
