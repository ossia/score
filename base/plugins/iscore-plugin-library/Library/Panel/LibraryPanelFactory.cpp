#include "LibraryPanelFactory.hpp"
#include "LibraryPanelDelegate.hpp"

namespace Library
{

std::unique_ptr<iscore::PanelDelegate>
PanelDelegateFactory::make(const iscore::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
