#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class RackModel;
namespace Scenario
{
    namespace Command
    {
        class DuplicateRack : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("DuplicateRack", "DuplicateRack")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(DuplicateRack, "ScenarioControl")
                DuplicateRack(ObjectPath&& rackToCopy);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_rackPath;

                Id<RackModel> m_newRackId;
        };
    }
}
