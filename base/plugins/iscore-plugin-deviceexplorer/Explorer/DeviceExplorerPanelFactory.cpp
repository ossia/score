#include "DeviceExplorerPanelFactory.hpp"
#include "PanelBase/DeviceExplorerPanelModel.hpp"
#include "PanelBase/DeviceExplorerPanelPresenter.hpp"
#include "PanelBase/DeviceExplorerPanelView.hpp"
#include "PanelBase/DeviceExplorerPanelId.hpp"

namespace iscore {
class DocumentModel;


}  // namespace iscore

using namespace iscore;


namespace DeviceExplorer
{
QString DeviceExplorerPanelFactory::panelName() const
{
    return "DeviceExplorer";
}

int DeviceExplorerPanelFactory::panelId() const
{
    return DEVICEEXPLORER_PANEL_ID;
}

iscore::PanelView* DeviceExplorerPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        QObject* parent)
{
    return new DeviceExplorerPanelView {ctx, parent};
}

iscore::PanelPresenter* DeviceExplorerPanelFactory::makePresenter(
        const iscore::ApplicationContext& ctx,
        iscore::PanelView* view,
        QObject* parent)
{
    return new DeviceExplorerPanelPresenter {view, parent};
}

iscore::PanelModel* DeviceExplorerPanelFactory::makeModel(
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new DeviceExplorerPanelModel{ctx, parent};
}
}
