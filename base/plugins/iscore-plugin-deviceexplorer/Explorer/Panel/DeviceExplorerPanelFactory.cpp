#include "DeviceExplorerPanelFactory.hpp"
#include "DeviceExplorerPanelDelegate.hpp"

namespace Explorer
{

std::unique_ptr<iscore::PanelDelegate> PanelDelegateFactory::make(
        const iscore::ApplicationContext& ctx)
{
    return std::make_unique<PanelDelegate>(ctx);
}

}
