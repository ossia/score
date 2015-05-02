#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The SetRigidity class
         *
         * Sets the rigidity of a constraint
         */
        class SetRigidity : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>

            public:
                ISCORE_COMMAND_DEFAULT_CTOR(SetRigidity, "ScenarioControl")
                SetRigidity(ObjectPath&& constraintPath, bool rigid);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                bool m_rigidity {};

                // Unused if the constraint was rigid
                TimeValue m_oldMinDuration;
                TimeValue m_oldMaxDuration;
        };
    }
}
