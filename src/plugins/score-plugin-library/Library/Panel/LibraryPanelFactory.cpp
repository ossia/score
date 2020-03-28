// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryPanelFactory.hpp"

#include "LibraryPanelDelegate.hpp"

namespace Library
{
std::unique_ptr<score::PanelDelegate>
UserPanelFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<UserPanel>(ctx);
}
std::unique_ptr<score::PanelDelegate>
ProjectPanelFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<ProjectPanel>(ctx);
}
std::unique_ptr<score::PanelDelegate>
ProcessPanelFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<ProcessPanel>(ctx);
}
}
