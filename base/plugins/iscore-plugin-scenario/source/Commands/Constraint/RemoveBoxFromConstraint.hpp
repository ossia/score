#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class AbstractConstraintViewModel;
class BoxModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveBoxFromConstraint class
         *
         * Removes a box : all the slots and function views will be removed.
         */
        class RemoveBoxFromConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveBoxFromConstraint, "ScenarioControl")
                RemoveBoxFromConstraint(ObjectPath&& boxPath);
                RemoveBoxFromConstraint(ObjectPath&& constraintPath, id_type<BoxModel> boxId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<BoxModel> m_boxId {};

                QByteArray m_serializedBoxData; // Should be done in the constructor

                QMap<id_type<AbstractConstraintViewModel>, bool> m_boxMappings;
        };
    }
}
