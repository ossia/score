#pragma once
#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>

namespace Process
{
class ProcessModel;
}
#include <score/model/Identifier.hpp>

namespace Scenario
{
// An enum that says if a process is before or after the state.
enum class ProcessPosition
{
  Previous,
  Following
};

// User messages
void updateTreeWithMessageList(
    State::MessageList& rootNode, const State::MessageList& lst);
}
