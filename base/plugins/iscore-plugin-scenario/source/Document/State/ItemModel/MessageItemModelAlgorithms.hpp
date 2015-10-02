#pragma once
#include <State/Message.hpp>
#include <ProcessInterface/State/MessageNode.hpp>
class Process;
enum class Position {
    Previous, Following
};

void updateTreeWithMessageList(
        MessageNode& rootNode,
        iscore::MessageList lst,
        const Id<Process>& proc,
        Position pos);
