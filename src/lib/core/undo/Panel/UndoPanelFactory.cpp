// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UndoPanelFactory.hpp"

#include "UndoPanelDelegate.hpp"

namespace score
{

std::unique_ptr<PanelDelegate>
UndoPanelDelegateFactory::make(const GUIApplicationContext& ctx)
{
  return std::make_unique<UndoPanelDelegate>(ctx);
}
}
