#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
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
        /**
        * @brief The AddProcessToConstraint class
        */
        class AddProcessToConstraint : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), AddProcessToConstraint, "AddProcessToConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                AddProcessToConstraint(
                    Path<ConstraintModel>&& constraintPath,
                    const QString& process);

                void undo() const override;
                void redo() const override;

                const Path<ConstraintModel>& constraintPath() const
                { return m_path; }
                const Id<Process>& processId() const
                { return m_createdProcessId; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<ConstraintModel> m_path;
                QString m_processName;

                Id<Process> m_createdProcessId {};
                Id<RackModel> m_createdRackId {};
                Id<SlotModel> m_createdSlotId {};
                Id<LayerModel> m_createdLayerId {};
                QByteArray m_layerConstructionData;
                bool m_noRackes = false;
                bool m_notBaseConstraint = false;
        };
    }
}



class AddOnlyProcessToConstraint : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), AddOnlyProcessToConstraint, "AddOnlyProcessToConstraint")
    public:
        AddOnlyProcessToConstraint(
            Path<ConstraintModel>&& constraint,
                const QString& process);
        AddOnlyProcessToConstraint(
                Path<ConstraintModel>&& constraint,
                const Id<Process>& idToUse,
                const QString& process);

        void undo() const override;
        void redo() const override;

        const Path<ConstraintModel>& constraintPath() const
        { return m_path; }

        const Id<Process>& processId() const
        { return m_createdProcessId; }

    protected:
        void serializeImpl(QDataStream& s) const override;
        void deserializeImpl(QDataStream& s) override;

    private:
        Path<ConstraintModel> m_path;
        QString m_processName;

        Id<Process> m_createdProcessId {};
};


#include <iscore/command/AggregateCommand.hpp>
// MOVEME
class CreateProcessInExistingSlot : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      CreateProcessInExistingSlot,
                                      "CreateProcessInExistingSlot")
};
// MOVEME
class CreateProcessInNewSlot : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      CreateProcessInNewSlot,
                                      "CreateProcessInNewSlot")
};
