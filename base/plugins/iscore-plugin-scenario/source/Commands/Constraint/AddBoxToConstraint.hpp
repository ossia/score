#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class BoxModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddBoxToConstraint class
         *
         * Adds an empty box, with no decks, to a constraint.
         */
        class AddBoxToConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>

            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddBoxToConstraint, "ScenarioControl")
                AddBoxToConstraint(ObjectPath&& constraintPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                id_type<BoxModel> m_createdBoxId {};
        };
    }
}
