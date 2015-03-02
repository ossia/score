#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The SetMaxDuration class
         *
         * Sets the Max duration of a Constraint
         */
        class SetMaxDuration : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>

            public:
                ISCORE_COMMAND_DEFAULT_CTOR(SetMaxDuration, "ScenarioControl")
                SetMaxDuration(ObjectPath&& constraintPath, TimeValue duration);

                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                TimeValue m_oldDuration {};
                TimeValue m_newDuration {};
        };
    }
}
