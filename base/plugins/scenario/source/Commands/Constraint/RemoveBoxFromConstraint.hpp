#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class BoxModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveBoxFromConstraint class
         *
         * Removes a box : all the decks and function views will be removed.
         */
        class RemoveBoxFromConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveBoxFromConstraint, "ScenarioControl")
                RemoveBoxFromConstraint(ObjectPath&& boxPath);
                RemoveBoxFromConstraint(ObjectPath&& constraintPath, id_type<BoxModel> boxId);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<BoxModel> m_boxId {};

                QByteArray m_serializedBoxData; // Should be done in the constructor
        };
    }
}
