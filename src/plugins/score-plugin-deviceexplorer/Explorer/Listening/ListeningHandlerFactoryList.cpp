// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ListeningHandlerFactoryList.hpp"

#include "DefaultListeningHandlerFactory.hpp"
namespace Explorer
{
ListeningHandlerFactoryList::~ListeningHandlerFactoryList() { }

std::unique_ptr<Explorer::ListeningHandler> ListeningHandlerFactoryList::make(
    const Explorer::DeviceDocumentPlugin& plug,
    const score::DocumentContext& ctx) const
{
  if (empty())
  {
    DefaultListeningHandlerFactory fact;
    return fact.make(plug, ctx);
  }
  else
  {
    for (auto& fact : *this)
    {
      if (auto res = fact.make(plug, ctx))
        return res;
    }
    return nullptr;
  }
}
}
