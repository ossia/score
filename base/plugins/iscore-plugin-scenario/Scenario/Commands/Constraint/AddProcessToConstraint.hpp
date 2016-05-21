#pragma once
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Settings/Model.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <QObject>

#include <QString>
#include <vector>

#include <Process/LayerModel.hpp>

#include <iscore/application/ApplicationContext.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>

namespace Scenario
{
namespace Command
{
class AddProcessToConstraintBase : public iscore::SerializableCommand
{
    public:
        AddProcessToConstraintBase() = default;
        AddProcessToConstraintBase(
                const ConstraintModel& constraint,
                UuidKey<Process::ProcessFactory> process):
            m_addProcessCommand{
                constraint,
                getStrongId(constraint.processes),
                process}
        {
        }

        const Path<ConstraintModel>& constraintPath() const
        { return m_addProcessCommand.constraintPath(); }
        const Id<Process::ProcessModel>& processId() const
        { return m_addProcessCommand.processId(); }
        const UuidKey<Process::ProcessFactory>& processKey() const
        { return m_addProcessCommand.processKey(); }


    protected:
        AddOnlyProcessToConstraint m_addProcessCommand;
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
        { return QObject::tr("Add a process to a constraint"); }
        static const CommandFactoryKey& static_key()
        { return AddProcessDelegate::static_key(); }

        AddProcessToConstraint() = default;

        AddProcessToConstraint(
                const ConstraintModel& constraint,
                const UuidKey<Process::ProcessFactory>& process) :
            AddProcessToConstraintBase{constraint, process}
        {
            auto& fact = context.components.factory<Process::ProcessList>();
            m_delegate.init(fact, constraint);
        }

        void undo() const override
        {
            auto& constraint = m_addProcessCommand.constraintPath().find();

            m_delegate.undo(constraint);
            m_addProcessCommand.undo(constraint);
        }
        void redo() const override
        {
            auto& constraint = m_addProcessCommand.constraintPath().find();

            // Create process model
            auto& proc = m_addProcessCommand.redo(constraint);
            m_delegate.redo(constraint, proc);
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_addProcessCommand.serialize();
            m_delegate.serialize(s);
        }
        void deserializeImpl(DataStreamOutput& s) override
        {
            QByteArray b;
            s >> b;

            m_addProcessCommand.deserialize(b);
            m_delegate.deserialize(s);
        }

    private:
        AddProcessDelegate m_delegate{*this};
};



class NotBaseConstraint {}; class IsBaseConstraint {};
class HasNoRacks {}; class HasRacks {};
class HasNoSlots {}; class HasSlots {};

template<typename... Traits>
class AddProcessDelegate;

template<>
class AddProcessDelegate<HasNoRacks>
{
    private:
        using proc_t = AddProcessToConstraint<AddProcessDelegate<HasNoRacks>>;
        proc_t& m_cmd;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegate_NoRacks"};
            return var;
        }

        AddProcessDelegate<HasNoRacks>(proc_t& cmd):
            m_cmd{cmd}
        {

        }

        void init(const Process::ProcessList& fact, const ConstraintModel& constraint)
        {
            m_createdRackId = getStrongId(constraint.racks);
            m_createdSlotId = Id<SlotModel>(iscore::id_generator::getFirstId());
            m_createdLayerId = Id<Process::LayerModel> (iscore::id_generator::getFirstId());
            m_layerConstructionData = fact.get(m_cmd.processKey())->makeStaticLayerConstructionData();
        }

        void undo(ConstraintModel& constraint) const
        {
            auto& rack = constraint.racks.at(m_createdRackId);

            // Removing the slot will remove the layer
            rack.slotmodels.remove(m_createdSlotId);
            constraint.racks.remove(m_createdRackId);
        }

        void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
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
            auto h = iscore::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
            auto slot = new SlotModel {m_createdSlotId,
                    h,
                    rack};
            rack->addSlot(slot);

            // Process View
            slot->layers.add(proc.makeLayer(m_createdLayerId, m_layerConstructionData, slot));
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
        Id<Process::LayerModel> m_createdLayerId;
        QByteArray m_layerConstructionData;
};


template<>
class AddProcessDelegate<HasNoSlots, HasRacks, NotBaseConstraint>
{
    private:
        using proc_t = AddProcessToConstraint<AddProcessDelegate<HasNoSlots,HasRacks, NotBaseConstraint>>;
        proc_t& m_cmd;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint"};
            return var;
        }

        AddProcessDelegate<HasNoSlots, HasRacks, NotBaseConstraint>(proc_t& cmd):
            m_cmd{cmd}
        {

        }

        void init(const Process::ProcessList& fact, const ConstraintModel& constraint)
        {
            m_createdSlotId = Id<SlotModel>(iscore::id_generator::getFirstId());

            m_layerConstructionData = fact.get(m_cmd.processKey())->makeStaticLayerConstructionData();
            m_createdLayerId = Id<Process::LayerModel> (iscore::id_generator::getFirstId());
        }

        void undo(ConstraintModel& constraint) const
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            auto& firstRack = *constraint.racks.begin();

            // Removing the slot will remove the layer
            firstRack.slotmodels.remove(m_createdSlotId);
        }

        void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            auto& firstRack = *constraint.racks.begin();

            // Slot
            auto h = iscore::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
            auto slot = new SlotModel {
                    m_createdSlotId,
                    h,
                    &firstRack};
            firstRack.addSlot(slot);

            // Layer
            slot->layers.add(proc.makeLayer(m_createdLayerId, m_layerConstructionData, slot));
        }

        void serialize(DataStreamInput& s) const
        {
            s << m_createdSlotId
              << m_createdLayerId
              << m_layerConstructionData;
        }

        void deserialize(DataStreamOutput& s)
        {
            s >> m_createdSlotId
              >> m_createdLayerId
              >> m_layerConstructionData;
        }

    private:
        Id<SlotModel> m_createdSlotId;
        Id<Process::LayerModel> m_createdLayerId;
        QByteArray m_layerConstructionData;
};


template<>
class AddProcessDelegate<HasSlots, HasRacks, NotBaseConstraint>
{
    private:
        using proc_t = AddProcessToConstraint<AddProcessDelegate<HasSlots, HasRacks, NotBaseConstraint>>;
        proc_t& m_cmd;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint"};
            return var;
        }

        AddProcessDelegate<HasSlots, HasRacks, NotBaseConstraint>(proc_t& cmd):
            m_cmd{cmd}
        {

        }

        void init(const Process::ProcessList& fact, const ConstraintModel& constraint)
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            const auto& firstRack = *constraint.racks.begin();

            ISCORE_ASSERT(!firstRack.slotmodels.empty());
            const auto& firstSlot = *firstRack.slotmodels.begin();

            m_layerConstructionData = fact.get(m_cmd.processKey())->makeStaticLayerConstructionData();
            m_createdLayerId = getStrongId(firstSlot.layers);
        }

        void undo(ConstraintModel& constraint) const
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            const auto& firstRack = *constraint.racks.begin();

            ISCORE_ASSERT(!firstRack.slotmodels.empty());
            auto& firstSlot = *firstRack.slotmodels.begin();

            firstSlot.layers.remove(m_createdLayerId);
        }

        void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
        {
            ISCORE_ASSERT(!constraint.racks.empty());
            const auto& firstRack = *constraint.racks.begin();

            ISCORE_ASSERT(!firstRack.slotmodels.empty());
            auto& firstSlot = *firstRack.slotmodels.begin();

            firstSlot.layers.add(proc.makeLayer(m_createdLayerId, m_layerConstructionData, &firstSlot));
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
        Id<Process::LayerModel> m_createdLayerId;
        QByteArray m_layerConstructionData;
};

template<>
class AddProcessDelegate<HasRacks, IsBaseConstraint>
{

        using proc_t = AddProcessToConstraint<AddProcessDelegate<HasRacks, IsBaseConstraint>>;

    public:
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey var{"AddProcessDelegateWhenRacksAndBaseConstraint"};
            return var;
        }

        AddProcessDelegate<HasRacks, IsBaseConstraint>(const proc_t&)
        {

        }

        void init(const Process::ProcessList& fact, const ConstraintModel& constraint)
        {
            // Base constraint : add in new slot?
        }

        void undo(ConstraintModel& constraint) const
        {

        }

        void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
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
        const UuidKey<Process::ProcessFactory>& process)
{
    auto isScenarioModel = dynamic_cast<Scenario::ScenarioModel*>(constraint.parent());
    auto noRackes = constraint.racks.empty();


    iscore::SerializableCommand* cmd{};
    if(noRackes)
    {
        cmd = new AddProcessToConstraint<AddProcessDelegate<HasNoRacks>>{
                constraint, process};
    }
    else
    {
        auto& firstRack = *constraint.racks.begin();
        auto noSlots = firstRack.slotmodels.empty();
        if(isScenarioModel)
        {
            if(noSlots)
            {
                cmd = new AddProcessToConstraint<AddProcessDelegate<HasNoSlots, HasRacks, NotBaseConstraint>>{
                    constraint, process};
            }
            else
            {
                cmd = new AddProcessToConstraint<AddProcessDelegate<HasSlots, HasRacks, NotBaseConstraint>>{
                    constraint, process};
            }
        }
        else
        {
            if(noSlots)
            {
                ISCORE_TODO;
            }
            else
            {
                cmd = new AddProcessToConstraint<AddProcessDelegate<HasRacks, IsBaseConstraint>>{
                    constraint, process};

            }
        }
    }

    return cmd;
}

// To make the preprocessor happy
using AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint = AddProcessDelegate<HasNoSlots,  HasRacks, NotBaseConstraint>;
using AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint = AddProcessDelegate<HasSlots, HasRacks, NotBaseConstraint>;
using AddProcessDelegate_HasRacks_BaseConstraint = AddProcessDelegate<HasRacks, IsBaseConstraint>;
}
}

ISCORE_COMMAND_DECL_T(AddProcessToConstraint<AddProcessDelegate<HasNoRacks>>)
ISCORE_COMMAND_DECL_T(AddProcessToConstraint<AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint>)
ISCORE_COMMAND_DECL_T(AddProcessToConstraint<AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint>)
ISCORE_COMMAND_DECL_T(AddProcessToConstraint<AddProcessDelegate_HasRacks_BaseConstraint>)
