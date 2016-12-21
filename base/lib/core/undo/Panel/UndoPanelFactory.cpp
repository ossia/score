#include "UndoPanelFactory.hpp"
#include "UndoPanelDelegate.hpp"

namespace iscore
{

std::unique_ptr<PanelDelegate>
UndoPanelDelegateFactory::make(const GUIApplicationContext& ctx)
{
  return std::make_unique<UndoPanelDelegate>(ctx);
}
}
