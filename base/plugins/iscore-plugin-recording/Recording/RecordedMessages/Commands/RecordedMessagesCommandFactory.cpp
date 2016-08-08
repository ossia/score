#include "RecordedMessagesCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& RecordedMessages::CommandFactoryName() {
    static const CommandParentFactoryKey key{"RecordedMessages"};
    return key;
}
