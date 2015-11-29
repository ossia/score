#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "iscore/tools/ObjectPath.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

class DataStreamInput;
class DataStreamOutput;
class RackModel;

namespace Scenario
{
    namespace Command
    {
        class DuplicateRack final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), DuplicateRack, "Duplicate a rack")
            public:
                DuplicateRack(ObjectPath&& rackToCopy);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                ObjectPath m_rackPath;

                Id<RackModel> m_newRackId;
        };
    }
}
