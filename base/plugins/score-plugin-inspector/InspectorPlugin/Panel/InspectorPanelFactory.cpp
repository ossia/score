// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InspectorPanelFactory.hpp"

#include "InspectorPanelDelegate.hpp"

namespace InspectorPanel
{

std::unique_ptr<score::PanelDelegate>
PanelDelegateFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
