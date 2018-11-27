// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageItemModelAlgorithms.hpp"

#include <State/Message.hpp>
namespace Scenario
{

void updateTreeWithMessageList(
    State::MessageList& rootNode, const State::MessageList& lst)
{
  for (const auto& mess : lst)
  {
    auto it = ossia::find_if(rootNode, [&] (const State::Message& other) { return other.address == mess.address; });
    if(it != rootNode.end())
    {
      it->value = mess.value;
    }
    else
    {
      rootNode.push_back(mess);
    }
  }
}

}
