#include "PanelDelegateFactory.hpp"
#include "PanelDelegate.hpp"

namespace Scenario
{

std::unique_ptr<iscore::PanelDelegate> PanelDelegateFactory::make(
        const iscore::ApplicationContext& ctx)
{
    return std::make_unique<PanelDelegate>(ctx);
}

}
