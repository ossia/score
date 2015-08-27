#pragma once
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
                ISCORE_COMMAND_DECL("AddProcessToConstraint", "AddProcessToConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddProcessToConstraint, "ScenarioControl")
                AddProcessToConstraint(
                    ModelPath<ConstraintModel>&& constraintPath,
                    QString process);

                virtual void undo() override;
                virtual void redo() override;

                const ModelPath<ConstraintModel>& constraintPath() const
                { return m_path; }
                id_type<Process> processId() const
                {
                    return m_createdProcessId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ConstraintModel> m_path;
                QString m_processName;

                id_type<Process> m_createdProcessId {};
                id_type<RackModel> m_createdRackId {};
                id_type<SlotModel> m_createdSlotId {};
                id_type<LayerModel> m_createdLayerId {};
                QByteArray m_layerConstructionData;
                bool m_noRackes = false;
                bool m_notBaseConstraint = false;
        };
    }
}
