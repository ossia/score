#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>



#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessModel;
class LayerModel;
class RackModel;
class SlotModel;
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
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddProcessToConstraint, "ScenarioControl")
                AddProcessToConstraint(ObjectPath&& constraintPath, QString process);

                virtual void undo() override;
                virtual void redo() override;

                const ObjectPath& constraintPath() const
                { return m_path; }
                id_type<ProcessModel> processId() const
                {
                    return m_createdProcessId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_processName;

                id_type<ProcessModel> m_createdProcessId {};
                id_type<RackModel> m_createdRackId {};
                id_type<SlotModel> m_createdSlotId {};
                id_type<LayerModel> m_createdLayerId {};
                QByteArray m_layerConstructionData;
                bool m_noRackes = false;
                bool m_notBaseConstraint = false;
        };
    }
}
