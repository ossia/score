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
         * @brief The CopyBox class
         *
         * Copy a Box inside the same constraint
         */
        class DuplicateBox : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(DuplicateBox, "ScenarioControl")
                DuplicateBox(ObjectPath&& boxToCopy);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_boxPath;

                id_type<BoxModel> m_newBoxId;
        };
    }
}
